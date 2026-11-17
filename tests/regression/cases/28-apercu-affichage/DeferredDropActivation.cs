using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace FileSpacerViewMenuTests {
    public sealed class DeferredDropActivation : IDisposable {
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibrary(string path);
        [DllImport("kernel32.dll",CharSet=CharSet.Ansi,ExactSpelling=true,SetLastError=true)] static extern IntPtr GetProcAddress(IntPtr module,string name);
        [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
        [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate bool Install(uint thread,IntPtr target,uint message);
        [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate uint Proof();
        [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate bool Remove();
        readonly Thread thread;
        readonly ManualResetEvent ready=new ManualResetEvent(false);
        readonly Install install;
        readonly Proof proof;
        readonly Remove remove;
        IntPtr library,window;
        Form form;
        Exception failure;
        bool hooked;

        public DeferredDropActivation(string path) {
            library=LoadLibrary(path);
            if(library==IntPtr.Zero)throw new Win32Exception();
            try {
                IntPtr start=GetProcAddress(library,"installActivationHook"),read=GetProcAddress(library,"readActivationProof"),stop=GetProcAddress(library,"removeActivationHook");
                if(start==IntPtr.Zero||read==IntPtr.Zero||stop==IntPtr.Zero)throw new Win32Exception();
                install=(Install)Marshal.GetDelegateForFunctionPointer(start,typeof(Install));
                proof=(Proof)Marshal.GetDelegateForFunctionPointer(read,typeof(Proof));
                remove=(Remove)Marshal.GetDelegateForFunctionPointer(stop,typeof(Remove));
                thread=new Thread(delegate() {
                    try {
                        using(var target=new Form()) {
                            form=target;target.Text="Deferred placement activation";target.StartPosition=FormStartPosition.Manual;
                            target.SetBounds(820,620,180,100);target.Shown+=delegate{window=target.Handle;ready.Set();};
                            Application.Run(target);
                        }
                    } catch(Exception error) {failure=error;ready.Set();}
                });
                thread.SetApartmentState(ApartmentState.STA);thread.IsBackground=true;thread.Start();
                if(!ready.WaitOne(5000)||failure!=null)throw new InvalidOperationException("Activation target did not open.",failure);
            } catch {FreeLibrary(library);library=IntPtr.Zero;throw;}
        }
        public void Hook(IntPtr source,uint message) {
            uint process;uint id=GetWindowThreadProcessId(source,out process);
            if(id==0||!install(id,window,message))throw new Win32Exception();
            hooked=true;
        }
        public int Result {get{return (int)proof();}}
        public IntPtr Window {get{return window;}}
        public void Dispose() {
            try {if(hooked&&!remove())throw new Win32Exception();hooked=false;}
            finally {
                try {
                    if(form!=null&&form.IsHandleCreated)form.BeginInvoke(new MethodInvoker(delegate{form.Close();}));
                    if(!thread.Join(5000))throw new TimeoutException("Activation target did not close normally.");
                } finally {ready.Dispose();if(library!=IntPtr.Zero){FreeLibrary(library);library=IntPtr.Zero;}}
            }
        }
    }
}
