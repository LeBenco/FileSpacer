using System;
using System.Text;
using System.Runtime.InteropServices;
using System.ComponentModel;
public static class LockUi {
    [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr w);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] static extern bool GetMenuItemRect(IntPtr owner,IntPtr menu,uint item,out Rect rect);
    public static Rect ItemRect(IntPtr menu,int index) {
        Rect rect; if(!GetMenuItemRect(IntPtr.Zero,menu,(uint)index,out rect)) throw new Win32Exception(); return rect;
    }
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr menu,int index);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetMenuString(IntPtr menu,uint id,StringBuilder text,int size,uint flags);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [StructLayout(LayoutKind.Sequential)] struct Mouse { public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Explicit,Size=32)] struct Union { [FieldOffset(0)] public Mouse Mouse; }
    [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
    [DllImport("user32.dll")] static extern uint SendInput(uint n,Input[] input,int size);
    public static void Click(IntPtr expected,int x,int y) {
        if(WindowFromPoint(new Point{X=x,Y=y})!=expected) throw new InvalidOperationException("Caption menu button is covered.");
        if(!SetCursorPos(x,y)) throw new Win32Exception();
        var down=new Input(); down.Data.Mouse.Flags=2;
        var up=new Input(); up.Data.Mouse.Flags=4;
        if(SendInput(2,new Input[]{down,up},Marshal.SizeOf(typeof(Input)))!=2) throw new Win32Exception();
    }
    public static string MenuText(IntPtr menu,int index) {
        var text=new StringBuilder(1024); GetMenuString(menu,(uint)index,text,text.Capacity,0x400); return text.ToString();
    }
}
