using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace FileSpacerRegression {
    public static class ShellIntegration {
        [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left,Top,Right,Bottom; }
        [StructLayout(LayoutKind.Sequential)] struct GuiInfo {
            public uint Size,Flags;
            public IntPtr Active,Focus,Capture,MenuOwner,MoveSize,Caret;
            public Rect CaretBounds;
        }
        [StructLayout(LayoutKind.Sequential)] struct Keyboard { public ushort Key,Scan; public uint Flags,Time; public UIntPtr Extra; }
        [StructLayout(LayoutKind.Sequential)] struct Mouse { public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra; }
        [StructLayout(LayoutKind.Explicit,Size=32)] struct Union {
            [FieldOffset(0)] public Keyboard Keyboard;
            [FieldOffset(0)] public Mouse Mouse;
        }
        [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] input,int size);
        [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref GuiInfo info);
        [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr window,uint relation);
        [DllImport("user32.dll")] static extern bool IsChild(IntPtr parent,IntPtr child);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,System.Text.StringBuilder name,int count);
        public static int InputSize { get { return Marshal.SizeOf(typeof(Input)); } }
        static Input Key(ushort key,bool up) {
            var result=new Input(); result.Type=1; result.Data.Keyboard.Key=key;
            result.Data.Keyboard.Flags=(up?2u:0u)|((key==0x2e)?1u:0u); return result;
        }
        static void Send(Input[] batch) {
            if(SendInput((uint)batch.Length,batch,Marshal.SizeOf(typeof(Input)))!=batch.Length) {
                int error=Marshal.GetLastWin32Error();var releases=new List<Input>();
                foreach(var input in batch) {
                    if(input.Type!=1 || (input.Data.Keyboard.Flags&2)!=0) continue;
                    var release=input;release.Data.Keyboard.Flags|=2;releases.Add(release);
                }
                if(releases.Count!=0) SendInput((uint)releases.Count,releases.ToArray(),Marshal.SizeOf(typeof(Input)));
                throw new Win32Exception(error,"Shell test input was only partially accepted.");
            }
        }
        public static void Shortcut(IntPtr owner,ushort key,bool control,bool shift) {
            if(GetForegroundWindow()!=owner) throw new InvalidOperationException("Input owner is not foreground.");
            foreach(int modifier in new int[]{0x10,0x11,0x12})
                if(GetAsyncKeyState(modifier)<0) throw new InvalidOperationException("Release modifiers before the test shortcut.");
            var batch=new List<Input>();
            if(control) batch.Add(Key(0x11,false)); if(shift) batch.Add(Key(0x10,false));
            batch.Add(Key(key,false));batch.Add(Key(key,true));
            if(shift) batch.Add(Key(0x10,true)); if(control) batch.Add(Key(0x11,true));
            Send(batch.ToArray());
        }
        public static void TypeText(IntPtr owner,string text) {
            if(GetForegroundWindow()!=owner) throw new InvalidOperationException("Text owner is not foreground.");
            if(!RenameFocused(owner)) throw new InvalidOperationException("No focused in-place rename editor in the target window.");
            var batch=new List<Input>();
            foreach(char character in text) {
                var down=new Input(); down.Type=1; down.Data.Keyboard.Scan=character;down.Data.Keyboard.Flags=4;
                var up=down;up.Data.Keyboard.Flags=6;batch.Add(down);batch.Add(up);
            }
            Send(batch.ToArray());
        }
        static GuiInfo Info(IntPtr owner) {
            uint process; uint thread=GetWindowThreadProcessId(owner,out process);
            var info=new GuiInfo();info.Size=(uint)Marshal.SizeOf(typeof(GuiInfo));
            if(thread==0 || !GetGUIThreadInfo(thread,ref info)) throw new Win32Exception();return info;
        }
        public static bool RenameFocused(IntPtr owner) {
            IntPtr focus=Info(owner).Focus;
            if(focus==IntPtr.Zero || !IsChild(owner,focus)) return false;
            var name=new System.Text.StringBuilder(64);GetClassName(focus,name,name.Capacity);
            return String.Equals(name.ToString(),"Edit",StringComparison.OrdinalIgnoreCase);
        }
        public static bool MenuActive(IntPtr owner) { return (Info(owner).Flags&0x1c)!=0; }
        public static bool OwnedBy(IntPtr dialog,IntPtr owner) {
            for(IntPtr current=GetWindow(dialog,4);current!=IntPtr.Zero;current=GetWindow(current,4))
                if(current==owner) return true;
            return false;
        }
        public static void Control(bool down) { Send(new Input[]{Key(0x11,!down)}); }
        public static bool ControlDown { get { return GetAsyncKeyState(0x11)<0; } }
        public sealed class MonitorArea { public int Left,Top,Width,Height; }
        [StructLayout(LayoutKind.Sequential)] struct MonitorInfo {
            public uint Size;public Rect Monitor,Work;public uint Flags;
        }
        delegate bool MonitorCallback(IntPtr monitor,IntPtr dc,ref Rect bounds,IntPtr data);
        [DllImport("user32.dll",SetLastError=true)] static extern bool EnumDisplayMonitors(IntPtr dc,IntPtr clip,MonitorCallback callback,IntPtr data);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetMonitorInfo(IntPtr monitor,ref MonitorInfo info);
        public static MonitorArea[] Monitors() {
            var areas=new List<MonitorArea>();Exception failure=null;
            bool ok=EnumDisplayMonitors(IntPtr.Zero,IntPtr.Zero,delegate(IntPtr monitor,IntPtr dc,ref Rect bounds,IntPtr data) {
                var info=new MonitorInfo();info.Size=(uint)Marshal.SizeOf(typeof(MonitorInfo));
                if(!GetMonitorInfo(monitor,ref info)) { failure=new Win32Exception();return false; }
                areas.Add(new MonitorArea{Left=info.Work.Left,Top=info.Work.Top,Width=info.Work.Right-info.Work.Left,Height=info.Work.Bottom-info.Work.Top});return true;
            },IntPtr.Zero);
            if(failure!=null) throw failure;if(!ok) throw new Win32Exception();return areas.ToArray();
        }
        [DllImport("shell32.dll",CharSet=CharSet.Unicode)] static extern int SHParseDisplayName(string name,IntPtr bind,out IntPtr pidl,uint mask,out uint attributes);
        [DllImport("shell32.dll")] static extern int SHGetNameFromIDList(IntPtr pidl,uint kind,out IntPtr name);
        public static string CanonicalName(string name) {
            IntPtr pidl=IntPtr.Zero,text=IntPtr.Zero;uint attributes;
            try {
                Marshal.ThrowExceptionForHR(SHParseDisplayName(name,IntPtr.Zero,out pidl,0,out attributes));
                Marshal.ThrowExceptionForHR(SHGetNameFromIDList(pidl,0x80028000,out text));return Marshal.PtrToStringUni(text);
            } finally { if(text!=IntPtr.Zero) Marshal.FreeCoTaskMem(text);if(pidl!=IntPtr.Zero) Marshal.FreeCoTaskMem(pidl); }
        }
    }
}
