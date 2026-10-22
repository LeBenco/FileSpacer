using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerRegression {
    public sealed class SelectedTarget {
        public int X,Y;
        public uint HitFlags;
        public string Bounds;
    }
    public static class SelectedItemPoint {
        [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
        [DllImport("user32.dll",EntryPoint="SendMessageW")] static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr wParam,IntPtr lParam);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint processId);
        [DllImport("user32.dll",SetLastError=true)] static extern bool ClientToScreen(IntPtr window,ref Point point);
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint processId);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint allocation,uint protection);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process,IntPtr address,[In] byte[] bytes,UIntPtr size,out UIntPtr written);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,[Out] byte[] bytes,UIntPtr size,out UIntPtr read);
        [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint freeType);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);

        // RECT is 16 bytes; LVHITTESTINFO is 24 bytes on Windows x64.
        // These messages are above WM_USER: their buffer must be in the
        // control's process. Synchronous sends keep it alive until processing
        // completes. The regression runner owns the overall timeout.
        static void Write(IntPtr process,IntPtr address,byte[] bytes) {
            UIntPtr written;
            if(!WriteProcessMemory(process,address,bytes,new UIntPtr((uint)bytes.Length),out written) || written.ToUInt64()!=(ulong)bytes.Length)
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Cannot write the ListView query buffer.");
        }
        static byte[] Read(IntPtr process,IntPtr address) {
            var bytes=new byte[24]; UIntPtr read;
            if(!ReadProcessMemory(process,address,bytes,new UIntPtr(24),out read) || read.ToUInt64()!=24)
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Cannot read the ListView query buffer.");
            return bytes;
        }
        public static SelectedTarget Locate(IntPtr listWindow) {
            long count=SendMessage(listWindow,0x1004,IntPtr.Zero,IntPtr.Zero).ToInt64(); // LVM_GETITEMCOUNT
            long state=SendMessage(listWindow,0x102c,IntPtr.Zero,new IntPtr(2)).ToInt64(); // LVM_GETITEMSTATE, LVIS_SELECTED
            Console.WriteLine("LOCATE: native ListView count={0}, selected state=0x{1:X}",count,state);
            if(count!=1 || (state&2)==0)
                throw new InvalidOperationException("Expected one selected native ListView item; count="+count+", state="+state+".");
            uint processId;
            if(GetWindowThreadProcessId(listWindow,out processId)==0) throw new InvalidOperationException("The ListView disappeared.");
            IntPtr process=OpenProcess(0x38,false,processId),buffer=IntPtr.Zero; // VM operation/read/write
            if(process==IntPtr.Zero) throw new Win32Exception();
            try {
                buffer=VirtualAllocEx(process,IntPtr.Zero,new UIntPtr(24),0x3000,4);
                if(buffer==IntPtr.Zero) throw new Win32Exception();
                var bytes=new byte[24]; bytes[0]=1; // RECT.left = LVIR_ICON
                Write(process,buffer,bytes);
                if(SendMessage(listWindow,0x100e,IntPtr.Zero,buffer)==IntPtr.Zero) // LVM_GETITEMRECT, item 0
                    throw new InvalidOperationException("LVM_GETITEMRECT did not return the icon rectangle.");
                bytes=Read(process,buffer);
                int left=BitConverter.ToInt32(bytes,0),top=BitConverter.ToInt32(bytes,4);
                int right=BitConverter.ToInt32(bytes,8),bottom=BitConverter.ToInt32(bytes,12);
                if(right<=left || bottom<=top) throw new InvalidOperationException("The native icon rectangle is empty.");
                var point=new Point { X=left+(right-left)/2,Y=top+(bottom-top)/2 };
                bytes=new byte[24];
                Buffer.BlockCopy(BitConverter.GetBytes(point.X),0,bytes,0,4);
                Buffer.BlockCopy(BitConverter.GetBytes(point.Y),0,bytes,4,4);
                Write(process,buffer,bytes);
                long hit=SendMessage(listWindow,0x1012,IntPtr.Zero,buffer).ToInt64(); // LVM_HITTEST
                uint flags=BitConverter.ToUInt32(Read(process,buffer),8);
                if(hit!=0 || (flags&2)==0) // LVHT_ONITEMICON
                    throw new InvalidOperationException("The native point is not on item 0's icon; hit="+hit+", flags="+flags+".");
                if(!ClientToScreen(listWindow,ref point)) throw new Win32Exception();
                if(WindowFromPoint(point)!=listWindow) throw new InvalidOperationException("The native icon point is covered or outside the ListView.");
                Console.WriteLine("LOCATE: icon client rectangle=({0},{1},{2},{3}), hit={4}, flags=0x{5:X}, screen point=({6},{7})",
                    left,top,right-left,bottom-top,hit,flags,point.X,point.Y);
                return new SelectedTarget { X=point.X,Y=point.Y,HitFlags=flags,
                    Bounds=String.Format("client=({0},{1},{2},{3})",left,top,right-left,bottom-top) };
            } finally {
                if(buffer!=IntPtr.Zero) VirtualFreeEx(process,buffer,UIntPtr.Zero,0x8000);
                CloseHandle(process);
            }
        }
    }
}
