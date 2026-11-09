using System;
using System.Runtime.InteropServices;
using Accessibility;
using FileSpacerRegression;

namespace FileSpacerColumnTests {
    public sealed class SystemCpuProbe {
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetSystemTimes(out long idle,out long kernel,out long user);
        long idle,kernel,user;
        readonly System.Diagnostics.Stopwatch watch=System.Diagnostics.Stopwatch.StartNew();
        public sealed class Sample { public double Percent,Seconds; }
        public SystemCpuProbe() {
            if(!GetSystemTimes(out idle,out kernel,out user)) throw new System.ComponentModel.Win32Exception();
        }
        public Sample Read() {
            long nextIdle,nextKernel,nextUser;
            if(!GetSystemTimes(out nextIdle,out nextKernel,out nextUser)) throw new System.ComponentModel.Win32Exception();
            long total=nextKernel-kernel+nextUser-user;
            var result=new Sample{Seconds=watch.Elapsed.TotalSeconds,
                Percent=total>0?Math.Max(0,Math.Min(100,100.0*(total-(nextIdle-idle))/total)):0};
            idle=nextIdle;kernel=nextKernel;user=nextUser;watch.Restart();return result;
        }
    }
    public static class ColumnsUi {
        [DllImport("user32.dll",SetLastError=true)] static extern bool RedrawWindow(IntPtr window,IntPtr rect,IntPtr region,uint flags);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SystemParametersInfoW(uint action,uint parameter,out int value,uint flags);
        public static void FlushPaint(IntPtr window) {
            // Paint existing invalid regions, including children, without invalidating more.
            if(!RedrawWindow(window,IntPtr.Zero,IntPtr.Zero,0x180)) throw new System.ComponentModel.Win32Exception();
        }
        public static bool DragFullWindows() {
            int enabled;
            if(!SystemParametersInfoW(0x26,0,out enabled,0)) throw new System.ComponentModel.Win32Exception();
            return enabled!=0;
        }
        [DllImport("oleacc.dll")] static extern int AccessibleObjectFromWindow(IntPtr window,uint objectId,ref Guid iid,[MarshalAs(UnmanagedType.Interface)] out IAccessible accessible);
        public static Rect[] Headers(IntPtr header) {
            Guid iid=new Guid("618736e0-3c3d-11cf-810c-00aa00389b71");
            IAccessible accessible=null;
            try {
                Marshal.ThrowExceptionForHR(AccessibleObjectFromWindow(header,0xfffffffc,ref iid,out accessible));
                int count=accessible.accChildCount;
                var result=new Rect[count];
                for(int i=0;i<count;++i) {
                    int left,top,width,height;
                    accessible.accLocation(out left,out top,out width,out height,i+1);
                    result[i]=new Rect{Left=left,Top=top,Right=left+width,Bottom=top+height};
                }
                return result;
            } finally { if(accessible!=null) Marshal.ReleaseComObject(accessible); }
        }
    }
}
