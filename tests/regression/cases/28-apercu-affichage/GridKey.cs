using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerViewMenuTests {
    public static class GridKey {
        public sealed class MarkerState {
            public bool Visible, InputTransparent;
            public int Left, Top, Width, Height;
            public string Class;
        }
        [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left, Top, Right, Bottom; }
        delegate bool EnumWindow(IntPtr window,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindow callback,IntPtr data);
        [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
        [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr window,uint command);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetProp(IntPtr window,string name);
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr window);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,System.Text.StringBuilder text,int count);
        [DllImport("user32.dll")] static extern int GetWindowRgnBox(IntPtr window,out Rect bounds);
        [DllImport("user32.dll")] static extern int MapWindowPoints(IntPtr from,IntPtr to,ref Rect rect,uint count);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
        [DllImport("user32.dll",SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr first,IntPtr second,uint flags,uint timeout,out UIntPtr result);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint process);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint type,uint protect);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr written);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr read);
        [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint type);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr value);
        public static int[] Origin(IntPtr list) { return Geometry(list,0x1029,0,new int[2]); }
        public static int[] IconBounds(IntPtr list,int item) { return Geometry(list,0x100e,(uint)item,new int[]{1,0,0,0}); }
        static int[] Geometry(IntPtr list,uint message,uint item,int[] fields) {
            uint pid; GetWindowThreadProcessId(list,out pid);
            IntPtr process=OpenProcess(0x38,false,pid);
            if(process==IntPtr.Zero) throw new Win32Exception();
            IntPtr remote=IntPtr.Zero;
            try {
                var bytes=new byte[fields.Length*4];
                Buffer.BlockCopy(fields,0,bytes,0,bytes.Length);
                var size=new UIntPtr((uint)bytes.Length);
                remote=VirtualAllocEx(process,IntPtr.Zero,size,0x3000,4);
                if(remote==IntPtr.Zero) throw new Win32Exception();
                UIntPtr result,count;
                if(!WriteProcessMemory(process,remote,bytes,size,out count)||count!=size) throw new Win32Exception();
                if(SendMessageTimeout(list,message,new UIntPtr(item),remote,2,3000,out result)==IntPtr.Zero || result==UIntPtr.Zero)
                    throw new InvalidOperationException("Native view geometry unavailable.");
                if(!ReadProcessMemory(process,remote,bytes,size,out count)||count!=size) throw new Win32Exception();
                Buffer.BlockCopy(bytes,0,fields,0,bytes.Length);
                return fields;
            } finally {if(remote!=IntPtr.Zero) VirtualFreeEx(process,remote,UIntPtr.Zero,0x8000);CloseHandle(process);}
        }
        public static MarkerState Marker(IntPtr parent) {
            var result=new MarkerState();
            IntPtr owner=GetAncestor(parent,2);
            EnumWindows(delegate(IntPtr window,IntPtr data) {
                if(GetWindow(window,4)!=owner || GetProp(window,"FileSpacer.SnapMarker")==IntPtr.Zero) return true;
                result.Visible=IsWindowVisible(window); result.InputTransparent=!IsWindowEnabled(window);
                var type=new System.Text.StringBuilder(64); GetClassName(window,type,type.Capacity); result.Class=type.ToString();
                if(!result.Visible) return false; // Creation precedes region publication.
                Rect bounds;
                if(GetWindowRgnBox(window,out bounds)==0) throw new InvalidOperationException("Native marker has no clipping region.");
                MapWindowPoints(window,IntPtr.Zero,ref bounds,2);
                result.Left=bounds.Left; result.Top=bounds.Top;
                result.Width=bounds.Right-bounds.Left; result.Height=bounds.Bottom-bounds.Top;
                return false;
            },IntPtr.Zero);
            return result;
        }
        [StructLayout(LayoutKind.Explicit,Size=40)] struct Input {
            [FieldOffset(0)] public uint Type;
            [FieldOffset(8)] public ushort Key;
            [FieldOffset(12)] public uint Flags;
        }
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
        static bool held;
        public static void Hold(bool down) {
            if(held==down) return;
            if(down) held=true;
            if(SendInput(1,new Input[]{new Input{Type=1,Key=0x53,Flags=down?0u:2u}},40)!=1)
                throw new Win32Exception(Marshal.GetLastWin32Error());
            held=down;
        }
    }
}
