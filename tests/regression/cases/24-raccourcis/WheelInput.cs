using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerRegression {
    public static class WheelInput {
        [StructLayout(LayoutKind.Sequential)] struct Keyboard { public ushort Key,Scan; public uint Flags,Time; public UIntPtr Extra; }
        [StructLayout(LayoutKind.Sequential)] struct Mouse { public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra; }
        [StructLayout(LayoutKind.Explicit,Size=32)] struct Union {
            [FieldOffset(0)] public Keyboard Keyboard;
            [FieldOffset(0)] public Mouse Mouse;
        }
        [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SetCursorPos(int x,int y);
        static void Send(Input input) {
            if(SendInput(1,new[]{input},Marshal.SizeOf(typeof(Input)))!=1)
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Wheel test input was not accepted.");
        }
        public static void Control(bool down) {
            var input=new Input(); input.Type=1; input.Data.Keyboard.Key=0x11; input.Data.Keyboard.Flags=down?0u:2u; Send(input);
        }
        // One wheel message over the view, clear of its scroll bars.
        public static void Turn(Rect view,int delta) {
            if(!SetCursorPos(view.Right-40,view.Bottom-40)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Cannot aim the pointer.");
            var input=new Input(); input.Data.Mouse.Data=unchecked((uint)delta); input.Data.Mouse.Flags=0x0800; Send(input);
        }
    }
}
