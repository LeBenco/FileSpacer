using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace FileSpacerRegression {
    // Scoped to the outer UI runner, not individual fixture processes. Windows x64.
    public sealed class DesktopActivity : IDisposable {
        [StructLayout(LayoutKind.Explicit,Size=32)] struct Reason {
            [FieldOffset(0)] public uint Version;
            [FieldOffset(4)] public uint Flags;
            [FieldOffset(8)] public IntPtr Text;
        }
        [DllImport("kernel32.dll",SetLastError=true)] static extern SafeFileHandle PowerCreateRequest(ref Reason reason);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool PowerSetRequest(SafeFileHandle request,int type);
        [DllImport("user32.dll",EntryPoint="SystemParametersInfoW",SetLastError=true)] static extern bool ReadParameter(uint action,uint param,out int value,uint flags);
        [DllImport("user32.dll",EntryPoint="SystemParametersInfoW",SetLastError=true)] static extern bool WriteParameter(uint action,uint param,IntPtr value,uint flags);
        SafeFileHandle request;
        bool screenSaverChanged;
        public bool OriginalScreenSaverActive { get; private set; }
        public bool ScreenSaverActive {
            get {
                int active;
                if(!ReadParameter(0x10,0,out active,0)) throw new Win32Exception();
                return active!=0;
            }
        }
        public DesktopActivity() {
            OriginalScreenSaverActive=ScreenSaverActive;
            try {
                var reason=new Reason { Flags=1,Text=Marshal.StringToHGlobalUni("FileSpacer: tests d'interface / UI regression tests") };
                try { request=PowerCreateRequest(ref reason); }
                finally { Marshal.FreeHGlobal(reason.Text); }
                if(request.IsInvalid) throw new Win32Exception();
                if(!PowerSetRequest(request,0) || !PowerSetRequest(request,1)) throw new Win32Exception();
                if(OriginalScreenSaverActive) {
                    // Flags zero: runtime change only; never save to the user profile.
                    if(!WriteParameter(0x11,0,IntPtr.Zero,0)) throw new Win32Exception();
                    screenSaverChanged=true;
                }
            } catch {
                Dispose();
                throw;
            }
        }
        public void Dispose() {
            // Closing the owned power-request handle removes both requests,
            // including if only one PowerSetRequest succeeded during setup.
            if(request!=null) { request.Dispose(); request=null; }
            if(screenSaverChanged) {
                if(!WriteParameter(0x11,1,IntPtr.Zero,0)) throw new Win32Exception();
                screenSaverChanged=false;
            }
            if(ScreenSaverActive!=OriginalScreenSaverActive)
                throw new InvalidOperationException("The original screen saver state was not restored.");
        }
    }
}
