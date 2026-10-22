using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerRegression {
    public static class NativeTargets {
        [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
        [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left,Top,Right,Bottom; }
        [StructLayout(LayoutKind.Sequential)] struct ScrollInfo {
            public uint Size; public Rect Bounds; public int Line,ThumbTop,ThumbBottom,Reserved;
            [MarshalAs(UnmanagedType.ByValArray,SizeConst=6)] public uint[] States;
        }
        [StructLayout(LayoutKind.Sequential)] struct GuiInfo {
            public uint Size,Flags; public IntPtr Active,Focus,Capture,MenuOwner,MoveSize,Caret; public Rect CaretBounds;
        }
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref GuiInfo info);
        [DllImport("user32.dll",EntryPoint="SendMessageW")] static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr wp,IntPtr lp);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint process);
        [DllImport("user32.dll",SetLastError=true)] static extern bool ClientToScreen(IntPtr w,ref Point p);
        [DllImport("user32.dll",SetLastError=true)] static extern bool ScreenToClient(IntPtr w,ref Point p);
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point p);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetWindowRect(IntPtr w,out Rect rect);
        [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr w);
        [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr w,uint flags);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetScrollBarInfo(IntPtr w,int id,ref ScrollInfo info);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint process);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr a,UIntPtr n,uint type,uint protect);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr p,IntPtr a,[In] byte[] b,UIntPtr n,out UIntPtr done);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr p,IntPtr a,[Out] byte[] b,UIntPtr n,out UIntPtr done);
        [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr p,IntPtr a,UIntPtr n,uint type);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr p);

        // Same read-only ListView protocol as the validated SelectedItemPoint.
        // Messages above WM_USER require structures in the control's process.
        // Synchronous sends retain the buffer until processing is complete.
        sealed class Query : IDisposable {
            IntPtr process; public IntPtr Address;
            public Query(IntPtr window) {
                uint id;
                if(GetWindowThreadProcessId(window,out id)==0) throw new InvalidOperationException("Target disappeared.");
                process=OpenProcess(0x38,false,id);
                if(process==IntPtr.Zero) throw new Win32Exception();
                Address=VirtualAllocEx(process,IntPtr.Zero,new UIntPtr(24),0x3000,4);
                if(Address==IntPtr.Zero) { int error=Marshal.GetLastWin32Error(); CloseHandle(process); process=IntPtr.Zero; throw new Win32Exception(error); }
            }
            public void Write(byte[] bytes) {
                UIntPtr done;
                if(!WriteProcessMemory(process,Address,bytes,new UIntPtr((uint)bytes.Length),out done)||done.ToUInt64()!=(ulong)bytes.Length) throw new Win32Exception();
            }
            public byte[] Read() {
                var bytes=new byte[24]; UIntPtr done;
                if(!ReadProcessMemory(process,Address,bytes,new UIntPtr(24),out done)||done.ToUInt64()!=24) throw new Win32Exception();
                return bytes;
            }
            public void Dispose() {
                if(Address!=IntPtr.Zero) { VirtualFreeEx(process,Address,UIntPtr.Zero,0x8000); Address=IntPtr.Zero; }
                if(process!=IntPtr.Zero) { CloseHandle(process); process=IntPtr.Zero; }
            }
        }
        static void Put(byte[] b,int offset,int value) { Buffer.BlockCopy(BitConverter.GetBytes(value),0,b,offset,4); }
        static Point Center(byte[] b) {
            int left=BitConverter.ToInt32(b,0),top=BitConverter.ToInt32(b,4),right=BitConverter.ToInt32(b,8),bottom=BitConverter.ToInt32(b,12);
            if(right<=left||bottom<=top) throw new InvalidOperationException("Native target rectangle is empty.");
            Console.WriteLine("LOCATE: client rectangle=({0},{1},{2},{3})",left,top,right-left,bottom-top);
            return new Point { X=left+(right-left)/2,Y=top+(bottom-top)/2 };
        }
        static Point Exposed(IntPtr window,Point point) {
            if(WindowFromPoint(point)!=window) throw new InvalidOperationException("Native target point is covered or belongs to another control.");
            Console.WriteLine("LOCATE: HWND={0}, screen point=({1},{2})",window.ToInt64(),point.X,point.Y);
            return point;
        }
        static Point ToScreen(IntPtr window,Point point) { if(!ClientToScreen(window,ref point)) throw new Win32Exception(); return Exposed(window,point); }
        static long Hit(IntPtr list,Query q,Point point,bool subitem,out byte[] bytes) {
            bytes=new byte[24]; Put(bytes,0,point.X); Put(bytes,4,point.Y); q.Write(bytes);
            long index=SendMessage(list,subitem?0x1039u:0x1012u,IntPtr.Zero,q.Address).ToInt64(); bytes=q.Read();
            Console.WriteLine("LOCATE: hit={0}, flags=0x{1:X}, subitem={2}",index,BitConverter.ToUInt32(bytes,8),BitConverter.ToInt32(bytes,16));
            return index;
        }
        static void ItemState(IntPtr list,int index,int count,bool selected) {
            long actual=SendMessage(list,0x1004,IntPtr.Zero,IntPtr.Zero).ToInt64();
            bool state=(SendMessage(list,0x102c,new IntPtr(index),new IntPtr(2)).ToInt64()&2)!=0;
            if(actual!=count||index<0||index>=actual||state!=selected) throw new InvalidOperationException("Unexpected native item count/selection before locating index "+index+"; count="+actual+", selected="+state+".");
        }
        public static Point Item(IntPtr list,int index,int count,bool selected,bool cell) {
            ItemState(list,index,count,selected);
            using(var q=new Query(list)) {
                var b=new byte[24]; Put(b,0,cell?0:1); if(cell) Put(b,4,1);
                q.Write(b);
                if(SendMessage(list,cell?0x1038u:0x100eu,new IntPtr(index),q.Address)==IntPtr.Zero) throw new InvalidOperationException("Native item rectangle query failed.");
                Point point=Center(q.Read()); byte[] hit;
                long found=Hit(list,q,point,cell,out hit);
                if(found!=index || (cell?BitConverter.ToInt32(hit,16)!=1:(BitConverter.ToUInt32(hit,8)&2)==0)) throw new InvalidOperationException("Point is not in the requested item part.");
                return ToScreen(list,point);
            }
        }
        public static void Background(IntPtr list,int x,int y) {
            Point point=Exposed(list,new Point {X=x,Y=y});
            if(!ScreenToClient(list,ref point)) throw new Win32Exception();
            using(var q=new Query(list)) { byte[] hit; if(Hit(list,q,point,false,out hit)!=-1) throw new InvalidOperationException("Background point overlaps an item."); }
        }
        public static Point Toolbar(IntPtr toolbar,int command) {
            using(var q=new Query(toolbar)) {
                q.Write(new byte[24]);
                if(SendMessage(toolbar,0x433,new IntPtr(command),q.Address)==IntPtr.Zero) throw new InvalidOperationException("Toolbar command has no rectangle: "+command);
                return ToScreen(toolbar,Center(q.Read()));
            }
        }
        public static Point Control(IntPtr window,int left,int top,int right,int bottom) {
            if(right<=left||bottom<=top) throw new InvalidOperationException("Control has no client area.");
            return Exposed(window,new Point { X=left+(right-left)/2,Y=top+(bottom-top)/2 });
        }
        public static Point Caption(IntPtr window) {
            Rect rect; if(!GetWindowRect(window,out rect)) throw new Win32Exception();
            int dpi=Math.Max(1,(int)GetDpiForWindow(window)),step=Math.Max(1,4*dpi/96);
            // Same read-only checks as Native.ActivateByCaptionClick, without
            // sending a click. This point must be outside every child/menu.
            for(int y=rect.Top+step;y<Math.Min(rect.Bottom,rect.Top+48*dpi/96);y+=step) {
                foreach(int numerator in new int[]{4,2,6,1,3,5,7}) {
                    var point=new Point {X=rect.Left+(rect.Right-rect.Left)*numerator/8,Y=y};
                    if(WindowFromPoint(point)!=window) continue;
                    int packed=unchecked((point.X&0xffff)|((point.Y&0xffff)<<16));
                    if(SendMessage(window,0x84,IntPtr.Zero,new IntPtr(packed)).ToInt64()!=2) continue; // WM_NCHITTEST == HTCAPTION
                    return Exposed(window,point);
                }
            }
            throw new InvalidOperationException("No exposed caption point is available to avoid menu commands.");
        }
        static GuiInfo GuiState(uint thread) {
            var info=new GuiInfo(); info.Size=(uint)Marshal.SizeOf(typeof(GuiInfo));
            if(thread==0||!GetGUIThreadInfo(thread,ref info)) throw new Win32Exception();
            return info;
        }
        static GuiInfo GuiState(IntPtr window) {
            uint process; return GuiState(GetWindowThreadProcessId(window,out process));
        }
        public static bool MenuActive(IntPtr window) { return (GuiState(window).Flags&0x1c)!=0; }
        public static IntPtr MenuOwner(IntPtr window) { return GuiState(window).MenuOwner; }
        public static string DescribeThread(uint thread) {
            var info=GuiState(thread);
            return String.Format("flags=0x{0:X}, active={1}, focus={2}, capture={3}, menuOwner={4}, moveSize={5}",
                info.Flags,info.Active,info.Focus,info.Capture,info.MenuOwner,info.MoveSize);
        }
        public static Point VerticalThumb(IntPtr list) {
            var info=new ScrollInfo(); info.Size=(uint)Marshal.SizeOf(typeof(ScrollInfo)); info.States=new uint[6];
            if(!GetScrollBarInfo(list,-5,ref info)) throw new Win32Exception(); // OBJID_VSCROLL
            if((info.States[0]&0x18001)!=0||info.ThumbBottom<=info.ThumbTop) throw new InvalidOperationException("No visible enabled vertical scrollbar thumb.");
            return Exposed(list,new Point { X=info.Bounds.Left+(info.Bounds.Right-info.Bounds.Left)/2,Y=info.Bounds.Top+(info.ThumbTop+info.ThumbBottom)/2 });
        }
    }
}
