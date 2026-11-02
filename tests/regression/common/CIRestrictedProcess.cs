using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Security.AccessControl;
using System.Security.Principal;

namespace FileSpacerRegression {
    // A restricted copy of the caller's token works even on hosted images with
    // UAC disabled. It preserves the user/profile/session, never their admin rights.
    public static class CIRestrictedProcess {
        [StructLayout(LayoutKind.Sequential)] struct SidAttributes { public IntPtr Sid; public uint Attributes; }
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Startup {
            public int Size; public string Reserved,Desktop,Title;
            public uint X,Y,Width,Height,CountX,CountY,Fill,Flags;
            public ushort Show,ReservedSize; public IntPtr ReservedData,Input,Output,Error;
        }
        [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr Process,Thread; public uint Id,ThreadId; }
        [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
        [DllImport("advapi32.dll",SetLastError=true)] static extern bool OpenProcessToken(IntPtr process,uint access,out IntPtr token);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool ConvertStringSidToSid(string value,out IntPtr sid);
        [DllImport("advapi32.dll",SetLastError=true)] static extern bool CreateRestrictedToken(IntPtr source,uint flags,uint disabled,SidAttributes[] sids,uint deleted,IntPtr privileges,uint restricted,IntPtr restrictSids,out IntPtr token);
        [DllImport("advapi32.dll",SetLastError=true)] static extern bool SetTokenInformation(IntPtr token,int kind,IntPtr data,uint size);
        [DllImport("advapi32.dll")] static extern uint GetLengthSid(IntPtr sid);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessAsUser(IntPtr token,string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
        [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
        [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint code);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenWindowStation(string name,bool inherit,uint access);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenDesktop(string name,uint flags,bool inherit,uint access);
        [DllImport("user32.dll")] static extern bool CloseWindowStation(IntPtr handle);
        [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr handle);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetUserObjectSecurity(IntPtr handle,ref uint information,byte[] descriptor,uint size,out uint needed);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SetUserObjectSecurity(IntPtr handle,ref uint information,byte[] descriptor);
        static byte[] ReadDesktopAcl(IntPtr handle) {
            uint information=4,needed;
            GetUserObjectSecurity(handle,ref information,null,0,out needed);
            if(needed==0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI desktop security size");
            byte[] original=new byte[needed];
            if(!GetUserObjectSecurity(handle,ref information,original,needed,out needed)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI desktop security");
            return original;
        }
        static void GrantDesktopAccess(IntPtr handle,byte[] original,int access) {
            var descriptor=new RawSecurityDescriptor(original,0);
            if(descriptor.DiscretionaryAcl==null) return;
            var user=WindowsIdentity.GetCurrent().User;
            descriptor.DiscretionaryAcl.InsertAce(descriptor.DiscretionaryAcl.Count,new CommonAce(AceFlags.None,AceQualifier.AccessAllowed,access,user,false,null));
            byte[] updated=new byte[descriptor.BinaryLength];descriptor.GetBinaryForm(updated,0);
            uint information=4;
            if(!SetUserObjectSecurity(handle,ref information,updated)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Grant current user CI desktop access");
        }
        public static int Run(string executable,string arguments,string directory,int timeoutMilliseconds) {
            return Run(executable,arguments,directory,timeoutMilliseconds,false);
        }
        public static int Run(string executable,string arguments,string directory,int timeoutMilliseconds,bool prepareDesktop) {
            IntPtr source=IntPtr.Zero,token=IntPtr.Zero,admin=IntPtr.Zero,medium=IntPtr.Zero,label=IntPtr.Zero;
            IntPtr station=IntPtr.Zero,desktop=IntPtr.Zero;
            byte[] stationAcl=null,desktopAcl=null;
            Exception originalFailure=null;
            ProcessInfo child=new ProcessInfo();
            try {
                if(!OpenProcessToken(GetCurrentProcess(),0x8b,out source)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI token");
                if(!ConvertStringSidToSid("S-1-5-32-544",out admin)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Administrator SID");
                // DISABLE_MAX_PRIVILEGE | LUA_TOKEN; Administrator SID is deny-only.
                if(!CreateRestrictedToken(source,5,1,new[]{new SidAttributes{Sid=admin}},0,IntPtr.Zero,0,IntPtr.Zero,out token)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Restrict CI token");
                if(!ConvertStringSidToSid("S-1-16-8192",out medium)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Medium integrity SID");
                int size=Marshal.SizeOf(typeof(SidAttributes))+(int)GetLengthSid(medium);
                label=Marshal.AllocHGlobal(size);
                Marshal.StructureToPtr(new SidAttributes{Sid=medium,Attributes=0x20},label,false);
                if(!SetTokenInformation(token,25,label,(uint)size)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Set medium CI integrity");
                if(prepareDesktop){
                    // Hosted desktops may grant access only through Administrators.
                    // Temporarily grant this same user access to the existing
                    // interactive station/desktop, then restore both exact DACLs.
                    station=OpenWindowStation("winsta0",false,0x60000);
                    if(station==IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI window station security");
                    stationAcl=ReadDesktopAcl(station);GrantDesktopAccess(station,stationAcl,0x2037f);
                    desktop=OpenDesktop("default",0,false,0x60000);
                    if(desktop==IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI desktop security");
                    desktopAcl=ReadDesktopAcl(desktop);GrantDesktopAccess(desktop,desktopAcl,0x201ff);
                }
                var startup=new Startup{Size=Marshal.SizeOf(typeof(Startup)),Desktop="winsta0\\default",Flags=1,Show=0};
                var command=new StringBuilder("\""+executable+"\" "+arguments);
                if(!CreateProcessAsUser(token,executable,command,IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,directory,ref startup,out child)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Start restricted CI worker");
                uint wait=WaitForSingleObject(child.Process,(uint)timeoutMilliseconds);
                if(wait==258){TerminateProcess(child.Process,1);throw new TimeoutException("Restricted CI worker timed out.");}
                if(wait!=0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Wait for restricted CI worker");
                uint code;
                if(!GetExitCodeProcess(child.Process,out code)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI worker exit code");
                return unchecked((int)code);
            } catch(Exception error){originalFailure=error;throw;} finally {
                uint information=4;int restoreError=0;
                if(desktopAcl!=null && !SetUserObjectSecurity(desktop,ref information,desktopAcl)) restoreError=Marshal.GetLastWin32Error();
                if(stationAcl!=null && !SetUserObjectSecurity(station,ref information,stationAcl) && restoreError==0) restoreError=Marshal.GetLastWin32Error();
                if(desktop!=IntPtr.Zero) CloseDesktop(desktop);
                if(station!=IntPtr.Zero) CloseWindowStation(station);
                if(child.Thread!=IntPtr.Zero) CloseHandle(child.Thread);
                if(child.Process!=IntPtr.Zero) CloseHandle(child.Process);
                if(label!=IntPtr.Zero) Marshal.FreeHGlobal(label);
                if(medium!=IntPtr.Zero) LocalFree(medium);
                if(admin!=IntPtr.Zero) LocalFree(admin);
                if(token!=IntPtr.Zero) CloseHandle(token);
                if(source!=IntPtr.Zero) CloseHandle(source);
                if(restoreError!=0){
                    var error=new Win32Exception(restoreError,"Restore CI desktop security");
                    if(originalFailure!=null) throw new AggregateException(originalFailure,error);
                    throw error;
                }
            }
        }
    }
}
