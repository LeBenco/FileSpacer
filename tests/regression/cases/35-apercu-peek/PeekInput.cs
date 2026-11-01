using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerRegression {
    public static class PeekInput {
        // Windows x64 INPUT/KEYBDINPUT layout, as declared by winuser.h.
        [StructLayout(LayoutKind.Explicit, Size=40)] struct Input {
            [FieldOffset(0)] public uint Type;
            [FieldOffset(8)] public ushort Key;
            [FieldOffset(12)] public uint Flags;
        }
        [DllImport("user32.dll", SetLastError=true)] static extern uint SendInput(uint count,Input[] input,int size);
        static Input Key(ushort code,bool up) { return new Input { Type=1,Key=code,Flags=up?2u:0u }; }
        public static void Space(IntPtr window,bool shift,bool repeat) {
            if(Native.GetForegroundWindow()!=window) throw new InvalidOperationException("Peek input target is not foreground.");
            Input[] input=shift
                ? new Input[]{Key(0x10,false),Key(0x20,false),Key(0x20,true),Key(0x10,true)}
                : repeat
                    ? new Input[]{Key(0x20,false),Key(0x20,false),Key(0x20,false),Key(0x20,true)}
                    : new Input[]{Key(0x20,false),Key(0x20,true)};
            if(SendInput((uint)input.Length,input,Marshal.SizeOf(typeof(Input)))!=input.Length) {
                var release=new Input[]{Key(0x20,true),Key(0x10,true)};
                SendInput((uint)release.Length,release,Marshal.SizeOf(typeof(Input)));
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Peek keyboard input incomplete.");
            }
        }
    }
}
