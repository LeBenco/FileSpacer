using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using FileSpacerRegression;

namespace FileSpacerViewMenuTests {
    [StructLayout(LayoutKind.Sequential)] struct MenuBarInfo {
        public uint Size; public Rect Bar; public IntPtr Menu, Window; public uint Flags;
    }
    [StructLayout(LayoutKind.Sequential)] struct MenuItemInfo {
        public uint Size, Mask, Type, State, Id;
        public IntPtr SubMenu, Checked, Unchecked, Data, Text;
        public uint TextLength; public IntPtr Bitmap;
    }
    [StructLayout(LayoutKind.Explicit,Size=40)] struct Input {
        [FieldOffset(0)] public uint Type;
        [FieldOffset(8)] public ushort VirtualKey;
        [FieldOffset(12)] public uint KeyFlags;
        [FieldOffset(20)] public uint MouseFlags;
    }
    public sealed class Entry { public uint Command; public string Label; public Rect Bounds; public uint State; }
    public sealed class Popup { public IntPtr Window; public Entry[] Entries; }
    public static class MenuUi {
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetMenuBarInfo(IntPtr window,int obj,int item,ref MenuBarInfo info);
        [DllImport("user32.dll")] static extern int GetMenuItemCount(IntPtr menu);
        [DllImport("user32.dll",EntryPoint="GetMenuItemInfoW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetMenuItemInfo(IntPtr menu,uint index,bool position,ref MenuItemInfo info);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetMenuItemRect(IntPtr window,IntPtr menu,uint index,out Rect rect);
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] input,int size);
        static uint Owner(IntPtr window) {uint id;Native.GetWindowThreadProcessId(window,out id);return id;}
        public static void Move(int x,int y) {if(!Native.SetCursorPos(x,y))throw new Win32Exception();}
        public static void Click(int x,int y,IntPtr owner) {
            if(Owner(WindowFromPoint(new Point{X=x,Y=y}))!=Owner(owner))throw new InvalidOperationException("Input target belongs to another process");
            Move(x,y);
            if(SendInput(2,new Input[]{new Input{MouseFlags=2},new Input{MouseFlags=4}},40)!=2)throw new Win32Exception();
        }
        public static void Escape(IntPtr owner) {
            if(Native.GetForegroundWindow()!=owner)throw new InvalidOperationException("Escape target is not foreground");
            if(SendInput(2,new Input[]{new Input{Type=1,VirtualKey=27},new Input{Type=1,VirtualKey=27,KeyFlags=2}},40)!=2)throw new Win32Exception();
        }
        public static void Key(IntPtr owner,ushort key,int count) {
            if(Native.GetForegroundWindow()!=owner)throw new InvalidOperationException("Key target is not foreground");
            for(int i=0;i<count;++i) {
                if(SendInput(2,new Input[]{new Input{Type=1,VirtualKey=key},new Input{Type=1,VirtualKey=key,KeyFlags=2}},40)!=2)throw new Win32Exception();
                Thread.Sleep(80);
            }
        }
        public static Popup Active(IntPtr owner,bool includeAll=false) {
            foreach(IntPtr window in Native.Top("#32768",null,Owner(owner))) {
                if(!Native.IsWindowVisible(window))continue;
                var bar=new MenuBarInfo{Size=(uint)Marshal.SizeOf(typeof(MenuBarInfo))};
                if(!GetMenuBarInfo(window,-4,0,ref bar)||bar.Menu==IntPtr.Zero)continue;
                var entries=new List<Entry>();bool vanished=false;
                int count=GetMenuItemCount(bar.Menu);
                for(uint i=0;i<count;++i) {
                    IntPtr text=Marshal.AllocHGlobal(1024);
                    try {
                        var info=new MenuItemInfo{Size=(uint)Marshal.SizeOf(typeof(MenuItemInfo)),Mask=0x147,Text=text,TextLength=512};
                        if(!GetMenuItemInfo(bar.Menu,i,true,ref info)) {
                            int error=Marshal.GetLastWin32Error();
                            if(!Native.IsWindowVisible(window)||GetMenuItemCount(bar.Menu)<0){vanished=true;break;}
                            throw new Win32Exception(error);
                        }
                        if((info.Type&0x800)!=0){if(includeAll)continue;break;}
                        Rect bounds;
                        if(!GetMenuItemRect(window,bar.Menu,i,out bounds)) {
                            int error=Marshal.GetLastWin32Error();
                            if(!Native.IsWindowVisible(window)||GetMenuItemCount(bar.Menu)<0){vanished=true;break;}
                            throw new Win32Exception(error);
                        }
                        entries.Add(new Entry{Command=info.Id,Label=Marshal.PtrToStringUni(text),Bounds=bounds,State=info.State});
                    }finally{Marshal.FreeHGlobal(text);}
                }
                if(!vanished&&entries.Count>=6)return new Popup{Window=window,Entries=entries.ToArray()};
            }
            return null;
        }
        public static Popup Open(IntPtr window,IntPtr toolbar,int command) {
            Rect previous=new Rect(),button=new Rect();double stable=-1;
            var watch=Stopwatch.StartNew();
            while(watch.ElapsedMilliseconds<10000) {
                Native.Message(window,0,0,0);
                button=Native.ToolbarButton(toolbar,command);
                bool same=button.Left==previous.Left&&button.Top==previous.Top&&button.Right==previous.Right&&button.Bottom==previous.Bottom;
                if(same&&Native.GetForegroundWindow()==window&&Owner(WindowFromPoint(new Point{X=button.Right-4,Y=(button.Top+button.Bottom)/2}))==Owner(window)) {
                    if(stable<0)stable=watch.Elapsed.TotalMilliseconds;
                    if(watch.Elapsed.TotalMilliseconds-stable>=100)break;
                }else stable=-1;
                previous=button;Thread.Sleep(10);
            }
            if(stable<0||watch.Elapsed.TotalMilliseconds-stable<100)throw new InvalidOperationException("Toolbar geometry or exposure did not stabilize");
            Click(button.Right-4,(button.Top+button.Bottom)/2,window);
            watch.Restart();
            while(watch.ElapsedMilliseconds<15000){var popup=Active(window);if(popup!=null)return popup;Thread.Sleep(10);}
            throw new TimeoutException("The native View menu did not open");
        }
        public static void Hover(Popup popup,int index) {var r=popup.Entries[index].Bounds;Move((r.Left+r.Right)/2,(r.Top+r.Bottom)/2);}
        public static void Choose(Popup popup,int index,IntPtr owner) {var r=popup.Entries[index].Bounds;Click((r.Left+r.Right)/2,(r.Top+r.Bottom)/2,owner);}
        public static void Leave(Popup popup,IntPtr owner) {
            Rect view=Native.ClientBounds(owner);
            int x=view.Right-12,y=view.Bottom-12;
            if(Owner(WindowFromPoint(new Point{X=x,Y=y}))!=Owner(owner))throw new InvalidOperationException("Outside-hover target is covered");
            Move(x,y);
        }
        public static void Outside(IntPtr owner) {
            Rect r=Native.Bounds(owner);
            foreach(int divisor in new int[]{3,2,4}) {
                int x=r.Left+r.Width/divisor,y=r.Top+10;
                long packed=unchecked((uint)((x&0xffff)|((y&0xffff)<<16)));
                if(Owner(WindowFromPoint(new Point{X=x,Y=y}))==Owner(owner)&&Native.Message(owner,0x84,0,packed)==2){Click(x,y,owner);return;}
            }
            throw new InvalidOperationException("No exposed caption for outside click");
        }
    }
}
