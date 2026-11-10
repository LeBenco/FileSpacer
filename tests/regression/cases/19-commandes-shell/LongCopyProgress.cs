using System;
using System.IO;
using System.Threading;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Windows.Automation;

namespace FileSpacerRegression {
    public sealed class CopyCpuBudget : IDisposable {
        [StructLayout(LayoutKind.Sequential)] struct Rate { public uint Flags,PercentTimes100; }
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr attributes,string name);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint processId);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int information,ref Rate rate,uint size);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        IntPtr job;
        public CopyCpuBudget(uint processId) {
            job=CreateJobObject(IntPtr.Zero,null);
            if(job==IntPtr.Zero) throw new Win32Exception();
            IntPtr process=OpenProcess(0x101,false,processId); // SET_QUOTA | TERMINATE, required for assignment only.
            try {
                if(process==IntPtr.Zero || !AssignProcessToJobObject(job,process)) {
                    int error=Marshal.GetLastWin32Error();CloseHandle(job);job=IntPtr.Zero;throw new Win32Exception(error);
                }
            } finally { if(process!=IntPtr.Zero) CloseHandle(process); }
            // No kill-on-close limit: this job can never terminate FileSpacer.
        }
        public void Limit() { SetRate(100); }
        void SetRate(uint percentTimes100) {
            var rate=new Rate {Flags=5,PercentTimes100=percentTimes100}; // ENABLE | HARD_CAP.
            if(!SetInformationJobObject(job,15,ref rate,(uint)Marshal.SizeOf(typeof(Rate)))) throw new Win32Exception();
        }
        public void Dispose() {
            if(job==IntPtr.Zero) return;
            try { SetRate(10000); } finally { CloseHandle(job);job=IntPtr.Zero; }
        }
    }
    public static class LongCopyProgress {
        // UIA must run on an MTA worker, as in the shared mouse-target helper.
        // Only copied values leave the worker, never an AutomationElement.
        static T Read<T>(Func<T> action) {
            T result=default(T);Exception error=null;
            var worker=new Thread(delegate() {
                try { result=action(); } catch(Exception failure) { error=failure; }
            });
            worker.IsBackground=true;worker.SetApartmentState(ApartmentState.MTA);worker.Start();
            if(!worker.Join(5000)) throw new TimeoutException("Cannot inspect native copy progress within five seconds.");
            if(error!=null) throw new InvalidOperationException("Cannot inspect native copy progress.",error);
            return result;
        }
        static AutomationElement Button(IntPtr window,bool resume) {
            try {
                var root=AutomationElement.FromHandle(window);
                var buttons=root.FindAll(TreeScope.Descendants,new OrCondition(
                    new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.Button),
                    new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.CheckBox)));
                foreach(AutomationElement button in buttons) {
                    string name=button.Current.Name;
                    object toggle;bool on=button.TryGetCurrentPattern(TogglePattern.Pattern,out toggle) && ((TogglePattern)toggle).Current.ToggleState==ToggleState.On;
                    bool matches=resume
                        ? name.IndexOf("Resume",StringComparison.OrdinalIgnoreCase)>=0 || name.IndexOf("Reprendre",StringComparison.OrdinalIgnoreCase)>=0 || name.IndexOf("Continuer",StringComparison.OrdinalIgnoreCase)>=0
                        : name.IndexOf("pause",StringComparison.OrdinalIgnoreCase)>=0 || name.IndexOf("suspend",StringComparison.OrdinalIgnoreCase)>=0;
                    if(resume && button.Current.AutomationId=="eltPauseButton" && on) matches=true;
                    if(matches) return button;
                }
            } catch(ElementNotAvailableException) { return null; }
            catch(System.Runtime.InteropServices.COMException) { return null; }
            return null;
        }
        public static bool Pause(IntPtr window) {
            return Read(delegate() {
                var button=Button(window,false);
                if(button==null) return false;
                Press(button);return true;
            });
        }
        static void Press(AutomationElement button) {
            object toggle;
            if(button.TryGetCurrentPattern(TogglePattern.Pattern,out toggle)) ((TogglePattern)toggle).Toggle();
            else ((InvokePattern)button.GetCurrentPattern(InvokePattern.Pattern)).Invoke();
        }
        public static bool Paused(IntPtr window) {
            return Read(delegate() { return Button(window,true)!=null; });
        }
        public static void Resume(IntPtr window) {
            Read(delegate() {
                var button=Button(window,true);
                if(button==null) throw new InvalidOperationException("Native Resume button disappeared.");
                Press(button);return true;
            });
        }
        public static bool Complete(string path,long length) {
            try {
                using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.None))
                    return file.Length==length;
            } catch(IOException) { return false; }
        }
    }
}
