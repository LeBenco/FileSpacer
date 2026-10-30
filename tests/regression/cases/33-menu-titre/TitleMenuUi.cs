using System;
using System.Text;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Threading;
using System.Drawing;
using System.Windows.Automation;

public static class TitleMenuUi {
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
    public static uint ProcessExitCode(IntPtr process) { uint code;if(!GetExitCodeProcess(process,out code))throw new Win32Exception();return code; }
    [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr window,uint command);
    [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr window);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
    [DllImport("user32.dll")] static extern bool ScreenToClient(IntPtr window,ref Point point);
    [DllImport("user32.dll")] static extern IntPtr RealChildWindowFromPoint(IntPtr parent,Point point);
    [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
    [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr menu,int index);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr window);
    [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
    [StructLayout(LayoutKind.Sequential)] struct Mouse { public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra; }
    [StructLayout(LayoutKind.Explicit,Size=32)] struct Union { [FieldOffset(0)] public Mouse Mouse; }
    [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
    [DllImport("user32.dll")] static extern uint SendInput(uint count,Input[] input,int size);
    [DllImport("user32.dll")] static extern int GetSystemMetrics(int metric);
    [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
    [StructLayout(LayoutKind.Sequential)] struct MenuBar {
        public int Size; public FileSpacerRegression.Rect Rect; public IntPtr Menu,MenuWindow; public uint Flags;
    }
    [DllImport("user32.dll")] static extern bool GetMenuBarInfo(IntPtr window,int objectId,int item,ref MenuBar info);
    static void Emit(uint flags,int x,int y) {
        var item=new Input(); item.Data.Mouse.Flags=flags; item.Data.Mouse.X=x; item.Data.Mouse.Y=y;
        if(SendInput(1,new Input[]{item},Marshal.SizeOf(typeof(Input)))!=1) throw new Win32Exception();
    }
    static void Move(int x,int y) {
        int left=GetSystemMetrics(76),top=GetSystemMetrics(77),width=GetSystemMetrics(78),height=GetSystemMetrics(79);
        Emit(0xc001,(x-left)*65535/Math.Max(1,width-1),(y-top)*65535/Math.Max(1,height-1));
        Thread.Sleep(12); Point point;
        if(!GetCursorPos(out point) || Math.Abs(point.X-x)>1 || Math.Abs(point.Y-y)>1) throw new InvalidOperationException("Pointer did not reach the requested VM point");
    }
    public static void Click(IntPtr control) {
        var rect=FileSpacerRegression.Native.Bounds(control); int x=(rect.Left+rect.Right)/2,y=(rect.Top+rect.Bottom)/2;
        ClickAt(control,x,y);
    }
    public static void ClickAt(IntPtr control,int x,int y) {
        Move(x,y); var actual=WindowFromPoint(new Point{X=x,Y=y});
        if(actual!=control) {
            IntPtr parent=GetParent(control),root=GetAncestor(control,2);
            var point=new Point{X=x,Y=y};
            if(!ScreenToClient(parent,ref point)) throw new Win32Exception();
            IntPtr resolved=RealChildWindowFromPoint(parent,point);
            if(resolved!=control || GetAncestor(actual,2)!=root || FileSpacerRegression.Native.GetForegroundWindow()!=root)
                throw new InvalidOperationException("Control is covered: expected="+control+" actual="+actual+" resolved="+resolved+" class="+FileSpacerRegression.Native.Class(actual)+" text="+FileSpacerRegression.Native.Text(actual)+" point="+x+","+y);
            Console.WriteLine("STEP: native RealChildWindowFromPoint resolves the exact control behind its transparent group box");
        }
        Emit(2,0,0); Thread.Sleep(35); Emit(4,0,0);
    }
    public static void Drag(IntPtr source,int x,int y,int targetX,int targetY,bool escape) {
        Move(x,y); IntPtr actual=WindowFromPoint(new Point{X=x,Y=y});
        if(actual!=source) throw new InvalidOperationException("Drag source is covered: expected="+source+" actual="+actual+" class="+FileSpacerRegression.Native.Class(actual)+" text="+FileSpacerRegression.Native.Text(actual)+" point="+x+","+y+" foreground="+FileSpacerRegression.Native.GetForegroundWindow());
        Emit(2,0,0);
        try {
            for(int i=1;i<=18;i++) Move(x+(targetX-x)*i/18,y+(targetY-y)*i/18);
            if(escape) FileSpacerRegression.Native.Keys(FileSpacerRegression.Native.GetForegroundWindow(),0x1b,false,false);
        } finally { Emit(4,0,0); }
    }
    public static string[] Catalog(IntPtr window) {
        var root=AutomationElement.FromHandle(window); var names=new List<string>();
        var items=root.FindAll(TreeScope.Descendants,new OrCondition(
            new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.DataItem),
            new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.ListItem)));
        foreach(AutomationElement item in items) names.Add(item.Current.Name);
        return names.ToArray();
    }
    public static int[] CatalogPoint(IntPtr window,string name) {
        var items=AutomationElement.FromHandle(window).FindAll(TreeScope.Descendants,
            new PropertyCondition(AutomationElement.NameProperty,name));
        foreach(AutomationElement item in items) {
            var r=item.Current.BoundingRectangle; if(!r.IsEmpty && r.Width>0 && r.Height>0) return new int[]{(int)(r.Left+r.Width/2),(int)(r.Top+r.Height/2)};
        }
        throw new InvalidOperationException("Catalog action not found: "+name);
    }
    public static uint[] MenuIds(IntPtr popup) {
        var info=new MenuBar(); info.Size=Marshal.SizeOf(typeof(MenuBar));
        if(!GetMenuBarInfo(popup,-4,0,ref info) || info.Menu==IntPtr.Zero) throw new Win32Exception();
        var ids=new List<uint>(); for(int i=0;i<GetMenuItemCount(info.Menu);i++)ids.Add(GetMenuItemID(info.Menu,i)); return ids.ToArray();
    }
    public static void Capture(IntPtr window,string path) {
        Thread.Sleep(400); // Let the native dialog finish its opening animation before capturing.
        var r=FileSpacerRegression.Native.Bounds(window);
        using(var bitmap=new Bitmap(r.Right-r.Left,r.Bottom-r.Top))using(var graphics=Graphics.FromImage(bitmap)) {
            graphics.CopyFromScreen(r.Left,r.Top,0,0,bitmap.Size);bitmap.Save(path);
        }
    }
}
