using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerViewMenuTests {
    public static class GridKey {
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
