using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;

public static class StorageRecoveryInput {
    [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
    [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left,Top,Right,Bottom; }
    [StructLayout(LayoutKind.Sequential)] struct Mouse { public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Explicit,Size=32)] struct Union { [FieldOffset(0)] public Mouse Mouse; }
    [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
    [StructLayout(LayoutKind.Sequential)] struct ThreadInfo {
        public uint Size,Flags;
        public IntPtr Active,Focus,Capture,MenuOwner,MoveSize,Caret;
        public Rect CaretBounds;
    }
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
    [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref ThreadInfo info);
    [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
    [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
    [DllImport("user32.dll",EntryPoint="SendNotifyMessageW",ExactSpelling=true,SetLastError=true)]
    static extern bool SendNotifyMessage(IntPtr window,uint message,UIntPtr wParam,IntPtr lParam);
    public static void RequestClose(IntPtr window) {
        // A sent notification reaches a native menu loop without blocking the
        // sender, so the receiver can still make outgoing Shell COM calls.
        if(!SendNotifyMessage(window,0x10,UIntPtr.Zero,IntPtr.Zero))throw new Win32Exception();
    }
    static void Button(uint flags) {
        var inputs=new[]{new Input{Data=new Union{Mouse=new Mouse{Flags=flags}}}};
        if(SendInput(1,inputs,Marshal.SizeOf(typeof(Input)))!=1)throw new Win32Exception();
    }
    public static void Down(IntPtr owner,int x,int y) {
        foreach(int key in new[]{1,2,4,5,6,0x10,0x11,0x12})
            if(GetAsyncKeyState(key)<0)throw new InvalidOperationException("Release mouse buttons and modifiers; input was not sent.");
        if(!FileSpacerRegression.Native.SetCursorPos(x,y))throw new Win32Exception();
        Thread.Sleep(12);
        PhysicalTarget(owner,x,y);
        Button(2);
    }
    public static void Up() { Button(4); }
    public static IntPtr PhysicalTarget(IntPtr owner,int x,int y) {
        if(FileSpacerRegression.Native.GetForegroundWindow()!=owner)
            throw new InvalidOperationException("Product frame is not foreground; input was not sent.");
        IntPtr target=WindowFromPoint(new Point{X=x,Y=y});
        if(target==IntPtr.Zero || GetAncestor(target,2)!=owner)
            throw new InvalidOperationException("Product input target is covered; input was not sent.");
        return target;
    }
    public static long CaptureOwner(IntPtr owner) {
        uint process;uint thread=FileSpacerRegression.Native.GetWindowThreadProcessId(owner,out process);
        if(thread==0)throw new Win32Exception();
        var info=new ThreadInfo{Size=(uint)Marshal.SizeOf(typeof(ThreadInfo))};
        if(!GetGUIThreadInfo(thread,ref info))throw new Win32Exception();
        return info.Capture.ToInt64();
    }
    public static bool NativeMenuActive(IntPtr owner) {
        uint process;uint thread=FileSpacerRegression.Native.GetWindowThreadProcessId(owner,out process);
        if(thread==0)throw new Win32Exception();
        var info=new ThreadInfo{Size=(uint)Marshal.SizeOf(typeof(ThreadInfo))};
        if(!GetGUIThreadInfo(thread,ref info))throw new Win32Exception();
        return (info.Flags&0x1c)!=0 && info.MenuOwner!=IntPtr.Zero && GetAncestor(info.MenuOwner,2)==owner;
    }
}
