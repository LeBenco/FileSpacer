using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.IO;
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
        [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,uint size,out uint needed);
        [DllImport("advapi32.dll")] static extern uint GetLengthSid(IntPtr sid);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessAsUser(IntPtr token,string application,StringBuilder command,IntPtr processSecurity,IntPtr threadSecurity,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
        [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
        [DllImport("kernel32.dll")] static extern bool TerminateProcess(IntPtr process,uint code);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
        static void PrepareDefaultDacl(IntPtr token) {
            uint needed;
            GetTokenInformation(token,6,IntPtr.Zero,0,out needed);
            if(needed==0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read restricted token default DACL size");
            IntPtr buffer=Marshal.AllocHGlobal((int)needed),updated=IntPtr.Zero;
            try {
                if(!GetTokenInformation(token,6,buffer,needed,out needed)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read restricted token default DACL");
                IntPtr existing=Marshal.ReadIntPtr(buffer);
                if(existing==IntPtr.Zero) return;
                byte[] bytes=new byte[(ushort)Marshal.ReadInt16(existing,2)];Marshal.Copy(existing,bytes,0,bytes.Length);
                var acl=new RawAcl(bytes,0);
                var user=WindowsIdentity.GetCurrent().User;
                Console.WriteLine("CI original token default DACL: "+new RawSecurityDescriptor(ControlFlags.DiscretionaryAclPresent,user,null,null,acl).GetSddlForm(AccessControlSections.Access));
                // The built-in Administrator's defaults may grant only the
                // now-disabled Administrators group and SYSTEM. Child objects
                // must also remain accessible to this same unprivileged user.
                acl.InsertAce(acl.Count,new CommonAce(AceFlags.None,AceQualifier.AccessAllowed,0x10000000,user,false,null));
                bytes=new byte[acl.BinaryLength];acl.GetBinaryForm(bytes,0);
                updated=Marshal.AllocHGlobal(bytes.Length);Marshal.Copy(bytes,0,updated,bytes.Length);
                Marshal.WriteIntPtr(buffer,updated);
                if(!SetTokenInformation(token,6,buffer,(uint)IntPtr.Size)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Set restricted token default DACL");
            } finally {if(updated!=IntPtr.Zero) Marshal.FreeHGlobal(updated);Marshal.FreeHGlobal(buffer);}
        }
        static void ReportProgress(string directory,ref string previous) {
            try {
                string progress;
                using(var stream=new FileStream(Path.Combine(directory,"output","progress.json"),FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
                using(var reader=new StreamReader(stream,Encoding.UTF8)) progress=reader.ReadToEnd().Trim();
                if(progress!=previous && progress.StartsWith("{") && progress.EndsWith("}")) {
                    previous=progress;
                    Console.WriteLine("CI progress: "+progress.Replace("\r","").Replace("\n"," "));
                }
            } catch(IOException) {} catch(UnauthorizedAccessException) {}
        }
        public static int Run(string executable,string arguments,string directory,int timeoutMilliseconds) {
            if(Environment.GetEnvironmentVariable("GITHUB_ACTIONS")!="true" || String.IsNullOrEmpty(Environment.GetEnvironmentVariable("ImageOS")))
                throw new InvalidOperationException("CI worker setup requires a GitHub-hosted image.");
            IntPtr source=IntPtr.Zero,token=IntPtr.Zero,admin=IntPtr.Zero,medium=IntPtr.Zero,label=IntPtr.Zero;
            ProcessInfo child=new ProcessInfo();
            try {
                if(!OpenProcessToken(GetCurrentProcess(),0x8b,out source)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI token");
                if(!ConvertStringSidToSid("S-1-5-32-544",out admin)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Administrator SID");
                // DISABLE_MAX_PRIVILEGE | LUA_TOKEN; keep the administrator SID deny-only.
                if(!CreateRestrictedToken(source,5,1,new[]{new SidAttributes{Sid=admin}},0,IntPtr.Zero,0,IntPtr.Zero,out token)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Restrict CI token");
                PrepareDefaultDacl(token);
                if(!ConvertStringSidToSid("S-1-16-8192",out medium)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Medium integrity SID");
                int size=Marshal.SizeOf(typeof(SidAttributes))+(int)GetLengthSid(medium);
                label=Marshal.AllocHGlobal(size);
                Marshal.StructureToPtr(new SidAttributes{Sid=medium,Attributes=0x20},label,false);
                if(!SetTokenInformation(token,25,label,(uint)size)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Set medium CI integrity");
                IntPtr elevation=Marshal.AllocHGlobal(4);
                try {
                    uint needed;
                    if(!GetTokenInformation(token,20,elevation,4,out needed)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI token elevation");
                    int elevated=Marshal.ReadInt32(elevation);
                    Console.WriteLine("CI token elevation: "+elevated);
                    if(elevated!=0) throw new InvalidOperationException("The CI worker token is still elevated.");
                } finally {Marshal.FreeHGlobal(elevation);}
                var startup=new Startup{Size=Marshal.SizeOf(typeof(Startup)),Desktop="winsta0\\default",Flags=1,Show=0};
                var command=new StringBuilder("\""+executable+"\" "+arguments);
                if(!CreateProcessAsUser(token,executable,command,IntPtr.Zero,IntPtr.Zero,false,0x08000000,IntPtr.Zero,directory,ref startup,out child)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Start restricted CI worker");
                var timer=System.Diagnostics.Stopwatch.StartNew();string progress=null;uint wait;
                while(true) {
                    long remaining=timeoutMilliseconds-timer.ElapsedMilliseconds;
                    if(remaining<=0){TerminateProcess(child.Process,1);WaitForSingleObject(child.Process,15000);throw new TimeoutException("Restricted CI worker timed out.");}
                    wait=WaitForSingleObject(child.Process,(uint)Math.Min(1000L,remaining));
                    if(wait!=258) break;
                    ReportProgress(directory,ref progress);
                }
                if(wait!=0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Wait for restricted CI worker");
                uint code;
                if(!GetExitCodeProcess(child.Process,out code)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI worker exit code");
                return unchecked((int)code);
            } finally {
                if(child.Thread!=IntPtr.Zero) CloseHandle(child.Thread);
                if(child.Process!=IntPtr.Zero) CloseHandle(child.Process);
                if(label!=IntPtr.Zero) Marshal.FreeHGlobal(label);
                if(medium!=IntPtr.Zero) LocalFree(medium);
                if(admin!=IntPtr.Zero) LocalFree(admin);
                if(token!=IntPtr.Zero) CloseHandle(token);
                if(source!=IntPtr.Zero) CloseHandle(source);
            }
        }
    }
}
