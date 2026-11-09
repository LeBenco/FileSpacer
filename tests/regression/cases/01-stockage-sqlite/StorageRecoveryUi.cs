using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class MutexRecoveryNative {
    public delegate bool EnumProc(IntPtr h,IntPtr p);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f,IntPtr p);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h,StringBuilder s,int n);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr h,EnumProc f,IntPtr p);
    [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr h);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr LoadLibraryEx(string p,IntPtr file,uint flags);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindResourceEx(IntPtr m,IntPtr type,IntPtr name,ushort lang);
    [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr m,IntPtr r);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr r);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr m);
    [DllImport("user32.dll")] static extern bool OpenClipboard(IntPtr owner);
    [DllImport("user32.dll")] static extern bool CloseClipboard();
    [DllImport("user32.dll")] static extern IntPtr GetClipboardData(uint format);
    [DllImport("user32.dll")] public static extern uint GetClipboardSequenceNumber();
    [DllImport("kernel32.dll")] static extern IntPtr GlobalLock(IntPtr memory);
    [DllImport("kernel32.dll")] static extern bool GlobalUnlock(IntPtr memory);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out Rect r);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr z,int x,int y,int w,int height,uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
    public static string ClassName(IntPtr h) {
        var text=new StringBuilder(256); GetClassName(h,text,text.Capacity); return text.ToString();
    }
    public static IntPtr[] Find(int pid,string name) {
        var result=new List<IntPtr>();
        EnumWindows(delegate(IntPtr h,IntPtr p) {
            uint owner; GetWindowThreadProcessId(h,out owner);
            if(owner==pid && ClassName(h)==name && IsWindowVisible(h)) result.Add(h);
            return true;
        },IntPtr.Zero);
        return result.ToArray();
    }
    public static string[] Describe(int pid) {
        var result=new List<string>();
        EnumWindows(delegate(IntPtr h,IntPtr p) {
            uint owner; GetWindowThreadProcessId(h,out owner);
            if(owner!=pid) return true;
            var text=new StringBuilder(1024); GetWindowText(h,text,text.Capacity);
            result.Add(h+" "+ClassName(h)+" visible="+IsWindowVisible(h)+" title="+text);
            EnumChildWindows(h,delegate(IntPtr child,IntPtr unused) {
                var label=new StringBuilder(2048); GetWindowText(child,label,label.Capacity);
                result.Add("  "+child+" "+ClassName(child)+" id="+GetDlgCtrlID(child)+" text="+label);
                return true;
            },IntPtr.Zero);
            return true;
        },IntPtr.Zero);
        return result.ToArray();
    }
    public static string[] Messages(string executable,int id) {
        var result=new List<string>();
        IntPtr module=LoadLibraryEx(executable,IntPtr.Zero,0x22);
        if(module==IntPtr.Zero) throw new Exception("Load product string resources.");
        try {
            foreach(ushort language in new ushort[]{0x409,0x40c,0x407,0x40a,0x410,0x816,0x408}) {
                IntPtr entry=FindResourceEx(module,new IntPtr(6),new IntPtr((id>>4)+1),language);
                if(entry==IntPtr.Zero) continue;
                IntPtr data=LockResource(LoadResource(module,entry));
                if(data==IntPtr.Zero) throw new Exception("Read product string resource.");
                for(int index=0;index<(id&15);index++) data=IntPtr.Add(data,2+2*(ushort)Marshal.ReadInt16(data));
                int length=(ushort)Marshal.ReadInt16(data);
                result.Add(Marshal.PtrToStringUni(IntPtr.Add(data,2),length).TrimEnd('\0'));
            }
        } finally { FreeLibrary(module); }
        return result.ToArray();
    }
    public static string ClipboardText() {
        if(!OpenClipboard(IntPtr.Zero)) return null;
        try {
            IntPtr memory=GetClipboardData(13);
            if(memory==IntPtr.Zero) return null;
            IntPtr text=GlobalLock(memory);
            if(text==IntPtr.Zero) return null;
            try {return Marshal.PtrToStringUni(text);} finally {GlobalUnlock(memory);}
        } finally {CloseClipboard();}
    }
}
