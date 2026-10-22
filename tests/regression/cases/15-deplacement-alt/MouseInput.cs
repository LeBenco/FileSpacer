using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Automation;

namespace FileSpacerRegression {
    [StructLayout(LayoutKind.Sequential)] public struct MousePoint { public int X,Y; }
    public static class AltDragInput {
        [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left,Top,Right,Bottom; }
        [StructLayout(LayoutKind.Sequential)] struct GuiInfo {
            public uint Size,Flags;
            public IntPtr Active,Focus,Capture,MenuOwner,MoveSize,Caret;
            public Rect CaretBounds;
        }
        [StructLayout(LayoutKind.Sequential)] struct Keyboard {
            public ushort Key,Scan; public uint Flags,Time; public UIntPtr Extra;
        }
        [StructLayout(LayoutKind.Sequential)] struct Mouse {
            public int X,Y; public uint Data,Flags,Time; public UIntPtr Extra;
        }
        [StructLayout(LayoutKind.Explicit,Size=32)] struct Union {
            [FieldOffset(0)] public Keyboard Keyboard;
            [FieldOffset(0)] public Mouse Mouse;
        }
        [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public Union Data; }
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SetCursorPos(int x,int y);
        [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
        [DllImport("user32.dll")] static extern uint GetDoubleClickTime();
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(MousePoint point);
        [DllImport("user32.dll")] static extern bool IsChild(IntPtr parent,IntPtr child);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint processId);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref GuiInfo info);
        static bool buttonHeld,altHeld,rightButton;
        static DateTime lastRelease=DateTime.MinValue;

        static Input Key(ushort key,bool up) {
            var input=new Input(); input.Type=1; input.Data.Keyboard.Key=key;
            input.Data.Keyboard.Flags=up?2u:0u; return input;
        }
        static Input Button(bool up) {
            var input=new Input(); input.Data.Mouse.Flags=rightButton?(up?0x10u:8u):(up?4u:2u); return input;
        }
        static void Send(params Input[] inputs) {
            if(SendInput((uint)inputs.Length,inputs,Marshal.SizeOf(typeof(Input)))!=inputs.Length)
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Mouse test input was not fully accepted.");
        }
        public static void Begin(IntPtr window,MousePoint point,bool alt,bool doublePress) {
            BeginButton(window,point,alt,doublePress,false);
        }
        public static void BeginRight(IntPtr window,MousePoint point,bool alt,bool doublePress) {
            BeginButton(window,point,alt,doublePress,true);
        }
        static void BeginButton(IntPtr window,MousePoint point,bool alt,bool doublePress,bool right) {
            // Separate independent gestures using the actual Windows setting.
            // doublePress below still emits both clicks in one uninterrupted batch.
            double elapsed=(DateTime.UtcNow-lastRelease).TotalMilliseconds;
            if(elapsed<=GetDoubleClickTime()) Thread.Sleep((int)(GetDoubleClickTime()-elapsed)+1);
            foreach(int key in new int[]{0x01,0x02,0x10,0x11,0x12})
                if(GetAsyncKeyState(key)<0) throw new InvalidOperationException("Release mouse buttons and modifiers before the test gesture.");
            Move(point);
            IntPtr target=WindowFromPoint(point);
            if(target!=window && !IsChild(window,target))
                throw new InvalidOperationException("The test gesture point is covered by another window.");
            // Track ownership before sending, so finally can release partial input too.
            buttonHeld=true; altHeld=alt; rightButton=right;
            if(alt) {
                Send(Key(0x12,false));
                // SendInput inserts input; confirm the OS key state separately.
                // This is not the target thread's message-time GetKeyState.
                var deadline=Stopwatch.StartNew();
                while(GetAsyncKeyState(0x12)>=0) {
                    if(deadline.ElapsedMilliseconds>=2000) throw new TimeoutException("Windows did not report Alt down before the mouse press.");
                    Thread.Sleep(10);
                }
            }
            Trace(rightButton?"before right press":"before left press");
            if(doublePress) Send(Button(false),Button(true),Button(false));
            else Send(Button(false));
        }
        static void Trace(string stage) {
            Console.WriteLine("INPUT {0:O}: {1}; foreground={2}, Alt={3}, left={4}, right={5}",
                DateTime.UtcNow,stage,GetForegroundWindow().ToInt64(),GetAsyncKeyState(0x12)<0,GetAsyncKeyState(1)<0,GetAsyncKeyState(2)<0);
        }
        public static void VerifyHeld(bool alt) {
            Trace("before drag motion");
            var deadline=Stopwatch.StartNew();
            while(GetAsyncKeyState(rightButton?2:1)>=0 || GetAsyncKeyState(rightButton?1:2)<0 || (GetAsyncKeyState(0x12)<0)!=alt) {
                if(deadline.ElapsedMilliseconds>=2000)
                    throw new InvalidOperationException("Unexpected OS button/modifier state before drag motion; expected gesture button down, other button up, and Alt="+alt+".");
                Thread.Sleep(10);
            }
            Trace("gesture state verified");
        }
        public static void Move(MousePoint point) {
            Trace("pointer to ("+point.X+","+point.Y+")");
            if(!SetCursorPos(point.X,point.Y)) throw new Win32Exception();
        }
        public static void ReleaseAlt() {
            if(altHeld) { Trace("releasing Alt"); Send(Key(0x12,true)); altHeld=false; }
        }
        public static void End() {
            try { if(buttonHeld) { Trace(rightButton?"releasing right":"releasing left"); Send(Button(true)); buttonHeld=false; lastRelease=DateTime.UtcNow; } }
            finally { ReleaseAlt(); }
        }
        public static void Escape() {
            ReleaseAlt();
            try { Send(Key(0x1b,false),Key(0x1b,true)); }
            catch { SendInput(1,new Input[]{Key(0x1b,true)},Marshal.SizeOf(typeof(Input))); throw; }
        }
        static GuiInfo Info(IntPtr window) {
            uint process; uint thread=GetWindowThreadProcessId(window,out process);
            if(thread==0) throw new InvalidOperationException("The test window disappeared.");
            var info=new GuiInfo(); info.Size=(uint)Marshal.SizeOf(typeof(GuiInfo));
            if(!GetGUIThreadInfo(thread,ref info)) throw new Win32Exception();
            return info;
        }
        public static bool Captured(IntPtr window) {
            IntPtr capture=Info(window).Capture;
            return capture==window || (capture!=IntPtr.Zero && IsChild(window,capture));
        }
        public static bool Moving(IntPtr window) { return Info(window).MoveSize==window; }
        public static bool Idle(IntPtr window) { var info=Info(window); return info.Capture==IntPtr.Zero && info.MoveSize==IntPtr.Zero; }
        public static string Describe(IntPtr window) {
            var info=Info(window);
            return String.Format("flags=0x{0:X}, active={1}, focus={2}, capture={3}, moveSize={4}, Alt={5}, left={6}, right={7}",
                info.Flags,info.Active,info.Focus,info.Capture,info.MoveSize,GetAsyncKeyState(0x12)<0,GetAsyncKeyState(1)<0,GetAsyncKeyState(2)<0);
        }

        [StructLayout(LayoutKind.Sequential)] struct CursorInfo {
            public uint Size,Flags; public IntPtr Cursor; public MousePoint Position;
        }
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetCursorInfo(ref CursorInfo info);
        [DllImport("user32.dll")] static extern IntPtr LoadCursor(IntPtr instance,IntPtr name);
        [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
        public static bool ResizeCursor() {
            var info=new CursorInfo(); info.Size=(uint)Marshal.SizeOf(typeof(CursorInfo));
            if(!GetCursorInfo(ref info)) throw new Win32Exception();
            return (info.Flags&1)!=0 && info.Cursor==LoadCursor(IntPtr.Zero,new IntPtr(32642)); // IDC_SIZENWSE
        }

        // UIA locates semantic targets in the Shell view. Only copied
        // points/booleans leave this bounded MTA worker; input remains native.
        static T ReadView<T>(IntPtr viewWindow,Func<AutomationElement,T> read) {
            T result=default(T); Exception error=null;
            var worker=new Thread(delegate() {
                try { result=read(AutomationElement.FromHandle(viewWindow)); }
                catch(Exception failure) { error=failure; }
            });
            worker.IsBackground=true; worker.SetApartmentState(ApartmentState.MTA); worker.Start();
            if(!worker.Join(5000)) throw new TimeoutException("UI Automation did not inspect the Shell view within five seconds.");
            if(error!=null) throw new InvalidOperationException("Cannot inspect Shell mouse target: "+error.Message,error);
            return result;
        }
        static List<AutomationElement> Visible(AutomationElement root,ControlType type,string name) {
            Condition condition=new PropertyCondition(AutomationElement.ControlTypeProperty,type);
            if(name!=null) condition=new AndCondition(condition,new PropertyCondition(AutomationElement.NameProperty,name));
            var result=new List<AutomationElement>();
            foreach(AutomationElement element in root.FindAll(TreeScope.Descendants,condition))
                if(!element.Current.IsOffscreen && !element.Current.BoundingRectangle.IsEmpty) result.Add(element);
            return result;
        }
        static MousePoint Clickable(AutomationElement element) {
            System.Windows.Point point;
            if(!element.TryGetClickablePoint(out point)) throw new InvalidOperationException("No clickable point: "+element.Current.ControlType.ProgrammaticName+" / "+element.Current.Name);
            return new MousePoint { X=(int)Math.Round(point.X),Y=(int)Math.Round(point.Y) };
        }
        public static MousePoint ItemPoint(IntPtr viewWindow,string name) {
            return ReadView(viewWindow,delegate(AutomationElement root) {
                var found=Visible(root,ControlType.ListItem,name);
                if(found.Count!=1) throw new InvalidOperationException("Expected one visible item named "+name+"; found "+found.Count+".");
                return Clickable(found[0]);
            });
        }
        public static MousePoint HeaderPoint(IntPtr viewWindow) {
            return ReadView(viewWindow,delegate(AutomationElement root) {
                var found=Visible(root,ControlType.HeaderItem,null);
                if(found.Count==0) throw new InvalidOperationException("Details view exposes no visible column header.");
                return Clickable(found[0]);
            });
        }
        public static bool RenameVisible(IntPtr viewWindow) {
            return ReadView(viewWindow,delegate(AutomationElement root) {
                int count=Visible(root,ControlType.Edit,null).Count;
                if(count>1) throw new InvalidOperationException("Ambiguous in-place rename edits: "+count+".");
                return count==1;
            });
        }
        public static MousePoint RenamePoint(IntPtr viewWindow) {
            return ReadView(viewWindow,delegate(AutomationElement root) {
                var found=Visible(root,ControlType.Edit,null);
                if(found.Count!=1) throw new InvalidOperationException("Expected one visible in-place rename edit; found "+found.Count+".");
                return Clickable(found[0]);
            });
        }
        public static MousePoint BackgroundPoint(IntPtr viewWindow) {
            return ReadView(viewWindow,delegate(AutomationElement root) {
                var items=Visible(root,ControlType.ListItem,"AnchorA");
                if(items.Count!=1) throw new InvalidOperationException("Cannot identify the fixture's item container.");
                var container=TreeWalker.ControlViewWalker.GetParent(items[0]);
                while(container!=null) {
                    var type=container.Current.ControlType;
                    if(type==ControlType.List || type==ControlType.DataGrid) break;
                    if(Automation.Compare(container,root)) { container=null; break; }
                    container=TreeWalker.ControlViewWalker.GetParent(container);
                }
                if(container==null) throw new InvalidOperationException("No accessible Shell item container.");
                var bounds=container.Current.BoundingRectangle;
                var obstacles=new List<System.Windows.Rect>();
                foreach(var type in new ControlType[]{ControlType.ListItem,ControlType.Header,ControlType.Edit,ControlType.ScrollBar,ControlType.Button})
                    foreach(var element in Visible(container,type,null)) obstacles.Add(element.Current.BoundingRectangle);
                // The complete 48x24 selection rectangle must be empty, with a
                // margin. Never rely on an assumed row height or client class.
                for(int y=(int)bounds.Bottom-70;y>=bounds.Top+12;y-=32) {
                    for(int x=(int)bounds.Left+40;x+60<bounds.Right;x+=64) {
                        var area=new System.Windows.Rect(x-4,y-4,56,32);
                        bool clear=true;
                        foreach(var obstacle in obstacles) if(area.IntersectsWith(obstacle)) { clear=false; break; }
                        if(!clear) continue;
                        for(int dx=0;dx<=48 && clear;dx+=24)
                            for(int dy=0;dy<=24;dy+=12)
                                if(!Automation.Compare(AutomationElement.FromPoint(new System.Windows.Point(x+dx,y+dy)),container)) { clear=false; break; }
                        if(clear) return new MousePoint { X=x,Y=y };
                    }
                }
                throw new InvalidOperationException("No verified empty Shell background for a 48x24 selection rectangle.");
            });
        }
    }
}
