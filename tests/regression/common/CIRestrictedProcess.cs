using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Cryptography;
using System.Security.Principal;
using System.Text;

namespace FileSpacerRegression {
    // Real standard users preserve per-user COM behavior on disposable hosted VMs.
    public static class CIRestrictedProcess {
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct UserInfo {
            public string Name,Password; public uint PasswordAge,Privilege;
            public string Home,Comment; public uint Flags; public string Script;
        }
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Startup {
            public int Size; public string Reserved,Desktop,Title;
            public uint X,Y,Width,Height,CountX,CountY,Fill,Flags;
            public ushort Show,ReservedSize; public IntPtr ReservedData,Input,Output,Error;
        }
        [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr Process,Thread; public uint Id,ThreadId; }
        [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserAdd(string server,uint level,ref UserInfo user,out uint parameter);
        [DllImport("netapi32.dll",CharSet=CharSet.Unicode)] static extern uint NetUserDel(string server,string user);
        [DllImport("advapi32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessWithLogonW(string user,string domain,string password,uint logon,string application,StringBuilder command,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo process);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenWindowStation(string name,bool inherit,uint access);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr OpenDesktop(string name,uint flags,bool inherit,uint access);
        [DllImport("user32.dll",SetLastError=true)] static extern bool CloseWindowStation(IntPtr handle);
        [DllImport("user32.dll",SetLastError=true)] static extern bool CloseDesktop(IntPtr handle);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetUserObjectSecurity(IntPtr handle,ref uint information,byte[] descriptor,uint size,out uint needed);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SetUserObjectSecurity(IntPtr handle,ref uint information,byte[] descriptor);
        [DllImport("kernel32.dll",SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        static byte[] ReadAcl(IntPtr handle) {
            uint information=4,needed;
            GetUserObjectSecurity(handle,ref information,null,0,out needed);
            if(needed==0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI desktop security size");
            byte[] original=new byte[needed];
            if(!GetUserObjectSecurity(handle,ref information,original,needed,out needed)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI desktop security");
            return original;
        }
        static void GrantDesktop(IntPtr handle,byte[] original,SecurityIdentifier user,int access) {
            var descriptor=new RawSecurityDescriptor(original,0);
            if(descriptor.DiscretionaryAcl==null) return;
            descriptor.DiscretionaryAcl.InsertAce(descriptor.DiscretionaryAcl.Count,new CommonAce(AceFlags.None,AceQualifier.AccessAllowed,access,user,false,null));
            byte[] updated=new byte[descriptor.BinaryLength];descriptor.GetBinaryForm(updated,0);
            uint information=4;
            if(!SetUserObjectSecurity(handle,ref information,updated)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Grant CI user desktop access");
        }
        public static int Run(string executable,string arguments,string directory,int timeoutMilliseconds) {
            if(Environment.GetEnvironmentVariable("GITHUB_ACTIONS")!="true" || String.IsNullOrEmpty(Environment.GetEnvironmentVariable("ImageOS")))
                throw new InvalidOperationException("CI account setup requires a GitHub-hosted image.");
            string name="fsci"+Guid.NewGuid().ToString("N").Substring(0,8);
            byte[] random=new byte[24];using(var generator=RandomNumberGenerator.Create()) generator.GetBytes(random);
            var account=new UserInfo{Name=name,Password=Convert.ToBase64String(random)+"aA1!",Privilege=1,Flags=0x201};
            bool created=false;IntPtr station=IntPtr.Zero,desktop=IntPtr.Zero;
            byte[] stationAcl=null,desktopAcl=null,directoryAcl=null;
            ProcessInfo child=new ProcessInfo();Exception failure=null;
            try {
                uint parameter,error=NetUserAdd(null,1,ref account,out parameter);
                if(error!=0) throw new Win32Exception((int)error,"Create standard CI account; parameter="+parameter);
                created=true;
                var user=(SecurityIdentifier)new NTAccount(Environment.MachineName,name).Translate(typeof(SecurityIdentifier));
                Console.WriteLine("CI standard account: "+name+" | SID="+user.Value);
                station=OpenWindowStation("winsta0",false,0x60000);
                if(station==IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI window station security");
                stationAcl=ReadAcl(station);GrantDesktop(station,stationAcl,user,0xf037f);
                desktop=OpenDesktop("default",0,false,0x60081);
                if(desktop==IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(),"Open CI desktop security");
                desktopAcl=ReadAcl(desktop);GrantDesktop(desktop,desktopAcl,user,0xf01ff);
                directoryAcl=Directory.GetAccessControl(directory,AccessControlSections.Access).GetSecurityDescriptorBinaryForm();
                var access=new DirectorySecurity();access.SetSecurityDescriptorBinaryForm(directoryAcl,AccessControlSections.Access);
                access.AddAccessRule(new FileSystemAccessRule(user,FileSystemRights.Modify,InheritanceFlags.ContainerInherit|InheritanceFlags.ObjectInherit,PropagationFlags.None,AccessControlType.Allow));
                Directory.SetAccessControl(directory,access);
                var startup=new Startup{Size=Marshal.SizeOf(typeof(Startup)),Desktop="winsta0\\default",Flags=1,Show=0};
                var command=new StringBuilder("\""+executable+"\" "+arguments);
                // LOGON_WITH_PROFILE and the native profile environment provide
                // this user's real HKCU, COM configuration and known folders.
                if(!CreateProcessWithLogonW(name,Environment.MachineName,account.Password,1,executable,command,0x08000000,IntPtr.Zero,directory,ref startup,out child))
                    throw new Win32Exception(Marshal.GetLastWin32Error(),"Start standard CI worker");
                uint wait=WaitForSingleObject(child.Process,(uint)timeoutMilliseconds);
                if(wait==258){TerminateProcess(child.Process,1);WaitForSingleObject(child.Process,15000);throw new TimeoutException("Standard CI worker timed out.");}
                if(wait!=0) throw new Win32Exception(Marshal.GetLastWin32Error(),"Wait for standard CI worker");
                uint code;if(!GetExitCodeProcess(child.Process,out code)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Read CI worker exit code");
                return unchecked((int)code);
            } catch(Exception error){failure=error;throw;} finally {
                var errors=new List<Exception>();uint information=4;
                if(directoryAcl!=null) try {
                    var original=new DirectorySecurity();original.SetSecurityDescriptorBinaryForm(directoryAcl,AccessControlSections.Access);Directory.SetAccessControl(directory,original);
                } catch(Exception error){errors.Add(error);}
                if(desktopAcl!=null && !SetUserObjectSecurity(desktop,ref information,desktopAcl)) errors.Add(new Win32Exception(Marshal.GetLastWin32Error(),"Restore CI desktop security"));
                if(stationAcl!=null && !SetUserObjectSecurity(station,ref information,stationAcl)) errors.Add(new Win32Exception(Marshal.GetLastWin32Error(),"Restore CI window station security"));
                if(desktop!=IntPtr.Zero && !CloseDesktop(desktop)) errors.Add(new Win32Exception(Marshal.GetLastWin32Error(),"Close CI desktop"));
                if(station!=IntPtr.Zero && !CloseWindowStation(station)) errors.Add(new Win32Exception(Marshal.GetLastWin32Error(),"Close CI window station"));
                if(child.Thread!=IntPtr.Zero) CloseHandle(child.Thread);if(child.Process!=IntPtr.Zero) CloseHandle(child.Process);
                if(created){uint error=NetUserDel(null,name);if(error!=0) errors.Add(new Win32Exception((int)error,"Remove standard CI account"));}
                account.Password=null;
                if(errors.Count!=0){if(failure!=null) errors.Insert(0,failure);throw new AggregateException("CI account/desktop cleanup failed.",errors);}
            }
        }
    }
}