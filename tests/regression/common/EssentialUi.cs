using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

// Windows x64 only. No application injection and no Shell control-class assumptions.
namespace FileSpacerRegression {
    [StructLayout(LayoutKind.Sequential)] public struct Rect {
        public int Left, Top, Right, Bottom;
        public int Width { get { return Right-Left; } }
        public int Height { get { return Bottom-Top; } }
    }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X,Y; }
    [ComImport, Guid("6d5140c1-7436-11ce-8034-00aa006009fa"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface Services { [PreserveSig] int QueryService(ref Guid service, ref Guid iid, out IntPtr result); }
    // IShellBrowser prefix through QueryActiveShellView, in Windows SDK order.
    [ComImport, Guid("000214e2-0000-0000-c000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface BrowserWindow {
        [PreserveSig] int GetWindow(out IntPtr window);
        [PreserveSig] int ContextSensitiveHelp([MarshalAs(UnmanagedType.Bool)] bool enter);
        [PreserveSig] int InsertMenusSB(IntPtr menu,IntPtr widths);
        [PreserveSig] int SetMenuSB(IntPtr menu,IntPtr descriptor,IntPtr active);
        [PreserveSig] int RemoveMenusSB(IntPtr menu);
        [PreserveSig] int SetStatusTextSB([MarshalAs(UnmanagedType.LPWStr)] string text);
        [PreserveSig] int EnableModelessSB([MarshalAs(UnmanagedType.Bool)] bool enable);
        [PreserveSig] int TranslateAcceleratorSB(IntPtr message,ushort id);
        [PreserveSig] int BrowseObject(IntPtr pidl,uint flags);
        [PreserveSig] int GetViewStateStream(uint mode,out IntPtr stream);
        [PreserveSig] int GetControlWindow(uint id,out IntPtr window);
        [PreserveSig] int SendControlMsg(uint id,uint message,UIntPtr wParam,IntPtr lParam,out IntPtr result);
        [PreserveSig] int QueryActiveShellView(out IntPtr view);
    }
    [ComImport, Guid("000214e3-0000-0000-c000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ShellView {
        [PreserveSig] int GetWindow(out IntPtr window);
        [PreserveSig] int ContextSensitiveHelp([MarshalAs(UnmanagedType.Bool)] bool enter);
        [PreserveSig] int TranslateAccelerator(IntPtr message);
        [PreserveSig] int EnableModeless([MarshalAs(UnmanagedType.Bool)] bool enable);
        [PreserveSig] int UIActivate(uint state);
    }
    [ComImport, Guid("cde725b0-ccc9-4519-917e-325d72fab4ce"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface FolderView {
        [PreserveSig] int GetCurrentViewMode(out uint mode);
        [PreserveSig] int SetCurrentViewMode(uint mode);
        [PreserveSig] int GetFolder(ref Guid iid, out IntPtr folder);
        [PreserveSig] int Item(int index, out IntPtr pidl);
        [PreserveSig] int ItemCount(uint flags, out int count);
        [PreserveSig] int Items(uint flags, ref Guid iid, out IntPtr items);
        [PreserveSig] int GetSelectionMarkedItem(out int index);
        [PreserveSig] int GetFocusedItem(out int index);
        [PreserveSig] int GetItemPosition(IntPtr pidl, out Point point);
        [PreserveSig] int GetSpacing(out Point point);
        [PreserveSig] int GetDefaultSpacing(out Point point);
        [PreserveSig] int GetAutoArrange();
        [PreserveSig] int SelectItem(int index, uint flags);
        [PreserveSig] int SelectAndPositionItems(uint count, IntPtr pidls, IntPtr points, uint flags);
    }
    // IFolderView2 prefix through GetViewModeAndIconSize, in Windows SDK order.
    [ComImport, Guid("1af3a467-214f-4298-908e-06b03e0b39f9"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface FolderView2 {
        [PreserveSig] int GetCurrentViewMode(out uint mode);
        [PreserveSig] int SetCurrentViewMode(uint mode);
        [PreserveSig] int GetFolder(ref Guid iid, out IntPtr folder);
        [PreserveSig] int Item(int index, out IntPtr pidl);
        [PreserveSig] int ItemCount(uint flags, out int count);
        [PreserveSig] int Items(uint flags, ref Guid iid, out IntPtr items);
        [PreserveSig] int GetSelectionMarkedItem(out int index);
        [PreserveSig] int GetFocusedItem(out int index);
        [PreserveSig] int GetItemPosition(IntPtr pidl, out Point point);
        [PreserveSig] int GetSpacing(out Point point);
        [PreserveSig] int GetDefaultSpacing(out Point point);
        [PreserveSig] int GetAutoArrange();
        [PreserveSig] int SelectItem(int index, uint flags);
        [PreserveSig] int SelectAndPositionItems(uint count, IntPtr pidls, IntPtr points, uint flags);
        [PreserveSig] int SetGroupBy(IntPtr key, int ascending);
        [PreserveSig] int GetGroupBy(IntPtr key, IntPtr ascending);
        [PreserveSig] int SetViewProperty(IntPtr pidl, IntPtr key, IntPtr value);
        [PreserveSig] int GetViewProperty(IntPtr pidl, IntPtr key, IntPtr value);
        [PreserveSig] int SetTileViewProperties(IntPtr pidl, IntPtr properties);
        [PreserveSig] int SetExtendedTileViewProperties(IntPtr pidl, IntPtr properties);
        [PreserveSig] int SetText(uint type, IntPtr text);
        [PreserveSig] int SetCurrentFolderFlags(uint mask, uint flags);
        [PreserveSig] int GetCurrentFolderFlags(out uint flags);
        [PreserveSig] int GetSortColumnCount(out int count);
        [PreserveSig] int SetSortColumns(IntPtr columns, int count);
        [PreserveSig] int GetSortColumns(IntPtr columns, int count);
        [PreserveSig] int GetItem(int index, ref Guid iid, out IntPtr item);
        [PreserveSig] int GetVisibleItem(int start, int previous, out int index);
        [PreserveSig] int GetSelectedItem(int start, out int index);
        [PreserveSig] int GetSelection(int noneImpliesFolder, out IntPtr selection);
        [PreserveSig] int GetSelectionState(IntPtr pidl, out uint flags);
        [PreserveSig] int InvokeVerbOnSelection(IntPtr verb);
        [PreserveSig] int SetViewModeAndIconSize(uint mode, int size);
        [PreserveSig] int GetViewModeAndIconSize(out uint mode, out int size);
    }
    [ComImport, Guid("1ac3d9f0-175c-11d1-95be-00609797ea4f"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface PersistFolder {
        [PreserveSig] int GetClassID(out Guid id);
        [PreserveSig] int Initialize(IntPtr pidl);
        [PreserveSig] int GetCurFolder(out IntPtr pidl);
    }
    [ComImport, Guid("000214f2-0000-0000-c000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface EnumPidls {
        [PreserveSig] int Next(uint count, out IntPtr pidl, out uint fetched);
        [PreserveSig] int Skip(uint count);
        [PreserveSig] int Reset();
        [PreserveSig] int Clone(out EnumPidls copy);
    }
    public sealed class ViewProbe : IDisposable {
        FolderView view; ShellView shellView; IntPtr parent;
        internal ViewProbe(FolderView folderView, ShellView activeView) {
            view=folderView; shellView=activeView;
            Guid iid=typeof(PersistFolder).GUID; IntPtr p=IntPtr.Zero;
            Native.Check(view.GetFolder(ref iid,out p));
            PersistFolder folder=null;
            try { folder=(PersistFolder)Marshal.GetObjectForIUnknown(p); Native.Check(folder.GetCurFolder(out parent)); }
            finally { if(p!=IntPtr.Zero) Marshal.Release(p); Native.Release(folder); }
        }
        string Name(IntPtr child) {
            IntPtr absolute=Native.ILCombine(parent,child), text=IntPtr.Zero;
            if(absolute==IntPtr.Zero) throw new OutOfMemoryException("ILCombine");
            try { Native.Check(Native.SHGetNameFromIDList(absolute,0x80058000,out text)); return Path.GetFileName(Marshal.PtrToStringUni(text)); }
            finally { if(text!=IntPtr.Zero) Marshal.FreeCoTaskMem(text); Marshal.FreeCoTaskMem(absolute); }
        }
        public int Count { get { int n; Native.Check(view.ItemCount(2,out n)); return n; } }
        public uint Mode { get { uint n; Native.Check(view.GetCurrentViewMode(out n)); return n; } set { Native.Check(view.SetCurrentViewMode(value)); } }
        public void SetViewModeAndIconSize(uint mode, int size) { Native.Check(((FolderView2)view).SetViewModeAndIconSize(mode,size)); }
        public int IconSize { get { uint mode; int size; Native.Check(((FolderView2)view).GetViewModeAndIconSize(out mode,out size)); return size; } }
        public bool AutoArrange {
            get { int result=view.GetAutoArrange(); Native.Check(result); return result==0; }
            set { Native.Check(((FolderView2)view).SetCurrentFolderFlags(1u,value?1u:0u)); } // FWF_AUTOARRANGE
        }
        public IntPtr Window { get { IntPtr window; Native.Check(shellView.GetWindow(out window)); return window; } }
        public Rect Bounds { get { IntPtr window; Native.Check(shellView.GetWindow(out window)); return Native.Bounds(window); } }
        public string[] Selected() {
            var names=new List<string>(); Guid iid=typeof(EnumPidls).GUID; IntPtr p=IntPtr.Zero;
            int count; Native.Check(view.ItemCount(1,out count));
            if(count==0) return names.ToArray();
            Native.Check(view.Items(1,ref iid,out p));
            if(p==IntPtr.Zero) return names.ToArray(); // Selection may have changed between the two calls.
            EnumPidls items=null;
            try {
                items=(EnumPidls)Marshal.GetObjectForIUnknown(p);
                for(;;) {
                    IntPtr child; uint fetched; int hr=items.Next(1,out child,out fetched); Native.Check(hr);
                    if(fetched==0) break;
                    try { names.Add(Name(child)); } finally { Marshal.FreeCoTaskMem(child); }
                }
            } finally { if(p!=IntPtr.Zero) Marshal.Release(p); Native.Release(items); }
            names.Sort(StringComparer.OrdinalIgnoreCase); return names.ToArray();
        }
        public string[] Names() {
            var names=new List<string>();
            for(int i=0,n=Count;i<n;++i) {
                IntPtr child; Native.Check(view.Item(i,out child));
                try { names.Add(Name(child)); } finally { Marshal.FreeCoTaskMem(child); }
            }
            names.Sort(StringComparer.OrdinalIgnoreCase); return names.ToArray();
        }
        public int IndexOf(string name) {
            for(int i=0,n=Count;i<n;++i) {
                IntPtr child; Native.Check(view.Item(i,out child));
                try { if(String.Equals(Name(child),name,StringComparison.OrdinalIgnoreCase)) return i; }
                finally { Marshal.FreeCoTaskMem(child); }
            }
            throw new InvalidOperationException("Item absent from actual Shell view: "+name);
        }
        public string FolderParsingName {
            get {
                IntPtr text=IntPtr.Zero;
                try { Native.Check(Native.SHGetNameFromIDList(parent,0x80028000,out text)); return Marshal.PtrToStringUni(text); }
                finally { if(text!=IntPtr.Zero) Marshal.FreeCoTaskMem(text); }
            }
        }
        public string[] ParsingNames(bool selected) {
            var names=new List<string>(); Guid iid=typeof(EnumPidls).GUID; IntPtr pointer=IntPtr.Zero;
            int count; Native.Check(view.ItemCount(selected?1u:2u,out count));
            if(count==0) return names.ToArray();
            Native.Check(view.Items(selected?1u:2u,ref iid,out pointer));
            EnumPidls items=null;
            try {
                items=(EnumPidls)Marshal.GetObjectForIUnknown(pointer);
                for(;;) {
                    IntPtr child; uint fetched; Native.Check(items.Next(1,out child,out fetched));
                    if(fetched==0) break;
                    IntPtr absolute=IntPtr.Zero,text=IntPtr.Zero;
                    try {
                        absolute=Native.ILCombine(parent,child);
                        if(absolute==IntPtr.Zero) throw new OutOfMemoryException("ILCombine");
                        Native.Check(Native.SHGetNameFromIDList(absolute,0x80028000,out text));
                        names.Add(Marshal.PtrToStringUni(text));
                    } finally {
                        if(text!=IntPtr.Zero) Marshal.FreeCoTaskMem(text);
                        if(absolute!=IntPtr.Zero) Marshal.FreeCoTaskMem(absolute);
                        if(child!=IntPtr.Zero) Marshal.FreeCoTaskMem(child);
                    }
                }
            } finally { if(pointer!=IntPtr.Zero) Marshal.Release(pointer); Native.Release(items); }
            names.Sort(StringComparer.OrdinalIgnoreCase); return names.ToArray();
        }
        public void SelectIndex(int index) { Native.Check(view.SelectItem(index,29u)); }
        public void Deselect(string name) { Native.Check(view.SelectItem(IndexOf(name),0)); }
        public void Select(string name) { Select(name,false); }
        public void Select(string name,bool extendSelection) {
            for(int i=0,n=Count;i<n;++i) {
                IntPtr child; Native.Check(view.Item(i,out child));
                bool match;
                try { match=String.Equals(Name(child),name,StringComparison.OrdinalIgnoreCase); }
                finally { Marshal.FreeCoTaskMem(child); }
                // Extending a fixture selection must not deselect the previous items.
                // Existing one-argument calls keep select/deselect-others/visible/focus.
                if(match) { Native.Check(view.SelectItem(i,extendSelection?9u:29u)); return; }
            }
            throw new InvalidOperationException("Item absent from actual Shell view: "+name);
        }
        public string Positions() {
            var positions=new List<string>();
            for(int i=0,n=Count;i<n;++i) {
                IntPtr child; Native.Check(view.Item(i,out child));
                try { Point p; Native.Check(view.GetItemPosition(child,out p)); positions.Add(Name(child)+"="+p.X+","+p.Y); }
                finally { Marshal.FreeCoTaskMem(child); }
            }
            positions.Sort(StringComparer.OrdinalIgnoreCase); return String.Join(";",positions.ToArray());
        }
        public void Position(string name,int x,int y) {
            IntPtr child=IntPtr.Zero, storage=IntPtr.Zero;
            try {
                Native.Check(view.Item(IndexOf(name),out child));
                if(child==IntPtr.Zero) throw new InvalidOperationException("Shell returned no item PIDL: "+name);
                storage=Marshal.AllocHGlobal(IntPtr.Size+Marshal.SizeOf(typeof(Point)));
                Marshal.WriteIntPtr(storage,child);
                IntPtr point=IntPtr.Add(storage,IntPtr.Size);
                Marshal.StructureToPtr(new Point{X=x,Y=y},point,false);
                // SVSI_POSITIONITEM | SVSI_NOSTATECHANGE: no selection/focus mutation.
                Native.Check(view.SelectAndPositionItems(1,storage,point,0x80000080u));
            } finally {
                if(storage!=IntPtr.Zero) Marshal.FreeHGlobal(storage);
                if(child!=IntPtr.Zero) Marshal.FreeCoTaskMem(child);
            }
        }
        // SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS: scroll to the item without selecting or focusing it.
        public void Reveal(string name) { Native.Check(view.SelectItem(IndexOf(name),0x48u)); }
        public void Focus() { Native.Check(shellView.UIActivate(2)); }
        public void Dispose() {
            if(parent!=IntPtr.Zero) { Marshal.FreeCoTaskMem(parent); parent=IntPtr.Zero; }
            Native.Release(shellView); shellView=null; Native.Release(view); view=null;
        }
    }
    public static class Native {
        delegate bool EnumWindow(IntPtr window,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindow callback,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent,EnumWindow callback,IntPtr data);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,StringBuilder text,int size);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window,StringBuilder text,int size);
        [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr window);
        [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr window);
        [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
        [DllImport("user32.dll",EntryPoint="GetWindowLongW")] public static extern int Style(IntPtr window,int index);
        [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr window,int id);
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
        [StructLayout(LayoutKind.Sequential)] struct GuiThreadInfo {
            public uint Size,Flags;
            public IntPtr Active,Focus,Capture,MenuOwner,MoveSize,Caret;
            public Rect CaretBounds;
        }
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetGUIThreadInfo(uint thread,ref GuiThreadInfo info);
        [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect rect);
        [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr window,out Rect rect);
        [DllImport("user32.dll")] static extern int MapWindowPoints(IntPtr from,IntPtr to,ref Rect rect,uint count);
        [StructLayout(LayoutKind.Sequential)] struct MonitorInfo {
            public uint Size;
            public Rect Monitor, Work;
            public uint Flags;
        }
        [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr window,uint flags);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetMonitorInfo(IntPtr monitor,ref MonitorInfo info);
        [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr window);
        [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
        [DllImport("user32.dll",SetLastError=true)] public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
        [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr window);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
        [DllImport("user32.dll")] static extern bool IsIconic(IntPtr window);
        [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr window);
        [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(Point point);
        [DllImport("user32.dll",SetLastError=true)] static extern bool SetCursorPos(int x,int y);
        [DllImport("user32.dll",SetLastError=true)] static extern bool GetCursorPos(out Point point);
        [DllImport("user32.dll",SetLastError=true)] public static extern bool PostMessage(IntPtr window,uint message,IntPtr wParam,IntPtr lParam);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wParam,IntPtr lParam,uint flags,uint timeout,out UIntPtr result);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wParam,StringBuilder text,uint flags,uint timeout,out UIntPtr result);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wParam,string text,uint flags,uint timeout,out UIntPtr result);
        [StructLayout(LayoutKind.Sequential)] struct Keyboard { public ushort Key,Scan; public uint Flags,Time; public UIntPtr Extra; }
        [StructLayout(LayoutKind.Sequential)] struct Mouse {
            public int X,Y;
            public uint Data,Flags,Time;
            public UIntPtr Extra;
        }
        [StructLayout(LayoutKind.Explicit,Size=32)] struct InputUnion {
            [FieldOffset(0)] public Keyboard Keyboard;
            [FieldOffset(0)] public Mouse Mouse;
        }
        [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public InputUnion Data; }
        [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
        [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
        [DllImport("shell32.dll")] internal static extern IntPtr ILCombine(IntPtr parent,IntPtr child);
        [DllImport("shell32.dll")] internal static extern int SHGetNameFromIDList(IntPtr pidl,uint nameType,out IntPtr text);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint pid);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint type,uint protect);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr read);
        [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint type);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        internal static void Check(int hr) { Marshal.ThrowExceptionForHR(hr); }
        internal static void Release(object value) { if(value!=null && Marshal.IsComObject(value)) Marshal.ReleaseComObject(value); }
        public static string Class(IntPtr window) { var text=new StringBuilder(128); GetClassName(window,text,text.Capacity); return text.ToString(); }
        public static IntPtr[] Top(string className,string title,uint pid) {
            var list=new List<IntPtr>();
            EnumWindows(delegate(IntPtr window,IntPtr data) {
                uint actual; GetWindowThreadProcessId(window,out actual); var text=new StringBuilder(1024); GetWindowText(window,text,text.Capacity);
                if(String.Equals(Class(window),className,StringComparison.OrdinalIgnoreCase) && (title==null||text.ToString()==title) && (pid==0||pid==actual)) list.Add(window); return true;
            },IntPtr.Zero); return list.ToArray();
        }
        public static IntPtr[] Children(IntPtr parent,string className) {
            var list=new List<IntPtr>();
            EnumChildWindows(parent,delegate(IntPtr window,IntPtr data) { if(String.Equals(Class(window),className,StringComparison.OrdinalIgnoreCase)) list.Add(window); return true; },IntPtr.Zero);
            return list.ToArray();
        }
        public static string[] DescribeChildren(IntPtr parent) {
            var result=new List<string>();
            EnumChildWindows(parent,delegate(IntPtr window,IntPtr data) {
                result.Add(window.ToInt64()+":"+Class(window)); return true;
            },IntPtr.Zero);
            return result.ToArray();
        }
        public static Rect Bounds(IntPtr window) { Rect rect; if(!GetWindowRect(window,out rect)) throw new Win32Exception(); return rect; }
        public static Rect WorkArea(IntPtr window) {
            var info=new MonitorInfo(); info.Size=(uint)Marshal.SizeOf(typeof(MonitorInfo));
            if(!GetMonitorInfo(MonitorFromWindow(window,2),ref info)) throw new Win32Exception();
            return info.Work;
        }
        public static Rect ClientBounds(IntPtr window) { Rect rect; if(!GetClientRect(window,out rect)) throw new Win32Exception(); MapWindowPoints(window,IntPtr.Zero,ref rect,2); return rect; }
        public static long Message(IntPtr window,uint message,long wParam,long lParam) {
            UIntPtr result;
            if(SendMessageTimeout(window,message,new UIntPtr(unchecked((ulong)wParam)),new IntPtr(lParam),2,3000,out result)==IntPtr.Zero)
                throw new Win32Exception(Marshal.GetLastWin32Error(),"Window message timed out/failed: "+message);
            return unchecked((long)result.ToUInt64());
        }
        public static string Text(IntPtr window) {
            var text=new StringBuilder(2048); UIntPtr result;
            if(SendMessageTimeout(window,0x0d,new UIntPtr((uint)text.Capacity),text,2,3000,out result)==IntPtr.Zero) throw new Win32Exception();
            return text.ToString();
        }
        public static void SetText(IntPtr window,string text) {
            UIntPtr result;
            if(SendMessageTimeout(window,0x0c,UIntPtr.Zero,text,2,3000,out result)==IntPtr.Zero||result==UIntPtr.Zero) throw new Win32Exception();
        }
        public static bool Activate(IntPtr window) {
            if(!IsWindow(window)||!IsWindowEnabled(window)) throw new InvalidOperationException("Activation target is absent or disabled.");
            if(IsIconic(window)) ShowWindow(window,9); // Do not unmaximize a normal visible window.
            return SetForegroundWindow(window);
        }
        public static Point ActivateByCaptionClick(IntPtr window) {
            if(!IsWindow(window)||!IsWindowVisible(window)||!IsWindowEnabled(window))
                throw new InvalidOperationException("No visible enabled activation target.");
            foreach(int key in new int[]{0x01,0x02,0x04,0x10,0x11,0x12})
                if(GetAsyncKeyState(key)<0) throw new InvalidOperationException("Release mouse buttons and Shift/Ctrl/Alt before UI tests.");
            // Try to raise the target; verify actual exposure below before clicking.
            if(!SetWindowPos(window,IntPtr.Zero,0,0,0,0,0x13)) throw new Win32Exception(); // NOMOVE|NOSIZE|NOACTIVATE
            Rect rect=Bounds(window);
            int scale=Math.Max(1,(int)GetDpiForWindow(window));
            int step=Math.Max(1,4*scale/96);
            int captionLimit=Math.Min(rect.Bottom,rect.Top+48*scale/96);
            int covered=0,nonCaption=0; IntPtr lastCover=IntPtr.Zero; long lastHit=0;
            for(int y=rect.Top+step;y<captionLimit;y+=step) {
                foreach(int numerator in new int[]{4,2,6,1,3,5,7}) {
                    var point=new Point{X=rect.Left+rect.Width*numerator/8,Y=y};
                    // A caption hit on the actual topmost window excludes controls and other applications.
                    IntPtr visible=WindowFromPoint(point);
                    if(visible!=window) { ++covered; lastCover=visible; continue; }
                    long packed=unchecked((uint)((point.X&0xffff)|((point.Y&0xffff)<<16)));
                    lastHit=Message(window,0x84,0,packed);
                    if(lastHit!=2) { ++nonCaption; continue; } // WM_NCHITTEST == HTCAPTION
                    if(!SetCursorPos(point.X,point.Y)) throw new Win32Exception();
                    Point actual;
                    if(!GetCursorPos(out actual)||actual.X!=point.X||actual.Y!=point.Y||WindowFromPoint(actual)!=window)
                        throw new InvalidOperationException("Activation click target changed or the cursor is constrained.");
                    if(Message(window,0x84,0,packed)!=2)
                        throw new InvalidOperationException("Activation point is no longer in the window caption.");
                    var down=new Input(); down.Type=0; down.Data.Mouse.Flags=2;
                    var up=new Input(); up.Type=0; up.Data.Mouse.Flags=4;
                    var inputs=new Input[]{down,up};
                    uint sent=SendInput(2,inputs,Marshal.SizeOf(typeof(Input)));
                    if(sent!=2) {
                        // Release the button if only the press was accepted.
                        if(sent==1) SendInput(1,new Input[]{up},Marshal.SizeOf(typeof(Input)));
                        throw new InvalidOperationException("Activation mouse input was not fully accepted: "+sent+"/2.");
                    }
                    return point;
                }
            }
            throw new InvalidOperationException("No uncovered caption point is available for an activation click. "+
                "Target="+window+", bounds=("+rect.Left+","+rect.Top+","+rect.Width+","+rect.Height+
                "), foreground="+GetForegroundWindow()+", covered samples="+covered+", non-caption samples="+nonCaption+
                ", last covering HWND="+lastCover+", last hit-test="+lastHit+".");
        }
        public static IntPtr ActiveWindowInOwningThread(IntPtr window) {
            uint pid; uint thread=GetWindowThreadProcessId(window,out pid);
            if(thread==0) throw new InvalidOperationException("Window thread is unavailable.");
            var info=new GuiThreadInfo(); info.Size=(uint)Marshal.SizeOf(typeof(GuiThreadInfo));
            if(!GetGUIThreadInfo(thread,ref info)) throw new Win32Exception(Marshal.GetLastWin32Error(),"Cannot inspect window GUI thread.");
            return info.Active;
        }
        public static void Keys(IntPtr window,ushort key,bool control,bool alt) {
            if(GetForegroundWindow()!=window) throw new InvalidOperationException("Test window is not foreground; input was not sent.");
            foreach(int modifier in new int[]{0x10,0x11,0x12}) if(GetAsyncKeyState(modifier)<0) throw new InvalidOperationException("Release Shift/Ctrl/Alt before running UI tests.");
            var inputs=new List<Input>();
            if(control) inputs.Add(Key(0x11,false)); if(alt) inputs.Add(Key(0x12,false));
            inputs.Add(Key(key,false)); inputs.Add(Key(key,true));
            if(alt) inputs.Add(Key(0x12,true)); if(control) inputs.Add(Key(0x11,true));
            var batch=inputs.ToArray();
            if(SendInput((uint)batch.Length,batch,Marshal.SizeOf(typeof(Input)))!=batch.Length) {
                // Also release modifiers if Windows accepted only part of the batch.
                var release=new Input[]{Key(key,true),Key(0x12,true),Key(0x11,true)};
                SendInput((uint)release.Length,release,Marshal.SizeOf(typeof(Input)));
                throw new Win32Exception(Marshal.GetLastWin32Error(),"SendInput failed.");
            }
        }
        static Input Key(ushort key,bool up) {
            var input=new Input(); input.Type=1; input.Data.Keyboard.Key=key;
            input.Data.Keyboard.Flags=(up?2u:0u)|((key==0x26)?1u:0u); return input;
        }
        static object Service(Services provider,Guid service,Guid iid) {
            IntPtr pointer; Check(provider.QueryService(ref service,ref iid,out pointer));
            try { return Marshal.GetObjectForIUnknown(pointer); } finally { Marshal.Release(pointer); }
        }
        public static string ActiveFolderParsingName(object registered,IntPtr window) {
            BrowserWindow browser=null; object active=null; PersistFolder folder=null;
            IntPtr viewPointer=IntPtr.Zero,folderPointer=IntPtr.Zero,pidl=IntPtr.Zero,text=IntPtr.Zero;
            string stage="IServiceProvider/SID_STopLevelBrowser";
            try {
                Services provider=registered as Services;
                if(provider==null) throw new COMException("No IServiceProvider",unchecked((int)0x80004002));
                browser=(BrowserWindow)Service(provider,new Guid("4c96be40-915c-11cf-99d3-00aa004ae837"),typeof(BrowserWindow).GUID);
                stage="IShellBrowser::GetWindow";
                IntPtr actual; Check(browser.GetWindow(out actual));
                // IOleWindow may return a document/container window rather than
                // IWebBrowser2's top-level frame. Compare their parent root.
                IntPtr root=actual==IntPtr.Zero?IntPtr.Zero:GetAncestor(actual,2); // GA_ROOT, not owners
                if(window==IntPtr.Zero || root!=window)
                    throw new COMException("Browser root HWND mismatch: registered="+window.ToInt64()+
                        ", browser="+actual.ToInt64()+", root="+root.ToInt64(),unchecked((int)0x80004005));
                stage="IShellBrowser::QueryActiveShellView";
                Check(browser.QueryActiveShellView(out viewPointer));
                if(viewPointer==IntPtr.Zero) return null;
                active=Marshal.GetObjectForIUnknown(viewPointer);
                stage="IShellView/IFolderView";
                FolderView view=active as FolderView;
                if(view==null) throw new COMException("No IFolderView",unchecked((int)0x80004002));
                stage="IFolderView::GetFolder(IPersistFolder2)";
                Guid iid=typeof(PersistFolder).GUID; Check(view.GetFolder(ref iid,out folderPointer));
                if(folderPointer==IntPtr.Zero) return null;
                folder=(PersistFolder)Marshal.GetObjectForIUnknown(folderPointer);
                stage="IPersistFolder2::GetCurFolder";
                Check(folder.GetCurFolder(out pidl));
                if(pidl==IntPtr.Zero) return null;
                stage="SHGetNameFromIDList(SIGDN_DESKTOPABSOLUTEPARSING)";
                Check(SHGetNameFromIDList(pidl,0x80028000,out text));
                return Marshal.PtrToStringUni(text);
            } catch(COMException failure) { throw new COMException(stage+": "+failure.Message,failure.ErrorCode); }
            finally {
                if(text!=IntPtr.Zero) Marshal.FreeCoTaskMem(text);
                if(pidl!=IntPtr.Zero) Marshal.FreeCoTaskMem(pidl);
                if(folderPointer!=IntPtr.Zero) Marshal.Release(folderPointer);
                if(viewPointer!=IntPtr.Zero) Marshal.Release(viewPointer);
                Release(folder); Release(active); Release(browser);
            }
        }
        public static ViewProbe View(object registered,IntPtr window) {
            Services provider=registered as Services; if(provider==null) return null;
            BrowserWindow browser=null;
            try {
                browser=(BrowserWindow)Service(provider,new Guid("4c96be40-915c-11cf-99d3-00aa004ae837"),typeof(BrowserWindow).GUID);
                IntPtr actual; Check(browser.GetWindow(out actual)); if(actual!=window) return null;
            } catch(COMException) { return null; } finally { Release(browser); }
            Guid folderService=typeof(FolderView).GUID;
            FolderView view=(FolderView)Service(provider,folderService,typeof(FolderView).GUID);
            ShellView shell=null;
            try { shell=(ShellView)Service(provider,folderService,typeof(ShellView).GUID); return new ViewProbe(view,shell); }
            catch { Release(shell); Release(view); throw; }
        }
        public static Rect ToolbarButton(IntPtr toolbar,int command) {
            // TB_GETRECT is above WM_USER: Windows does not marshal its RECT pointer.
            // Allocate a RECT in the control's process; read it back, then free it.
            uint pid; GetWindowThreadProcessId(toolbar,out pid); IntPtr process=OpenProcess(0x38,false,pid);
            if(process==IntPtr.Zero) throw new Win32Exception(); IntPtr remote=IntPtr.Zero;
            try {
                remote=VirtualAllocEx(process,IntPtr.Zero,new UIntPtr(16),0x3000,4);
                if(remote==IntPtr.Zero) throw new Win32Exception();
                if(Message(toolbar,0x433,command,remote.ToInt64())==0) throw new InvalidOperationException("Toolbar command has no rectangle: "+command);
                byte[] bytes=new byte[16]; UIntPtr read;
                if(!ReadProcessMemory(process,remote,bytes,new UIntPtr(16),out read)||read.ToUInt64()!=16) throw new Win32Exception();
                Rect rect=new Rect{Left=BitConverter.ToInt32(bytes,0),Top=BitConverter.ToInt32(bytes,4),Right=BitConverter.ToInt32(bytes,8),Bottom=BitConverter.ToInt32(bytes,12)};
                MapWindowPoints(toolbar,IntPtr.Zero,ref rect,2); return rect;
            } finally { if(remote!=IntPtr.Zero) VirtualFreeEx(process,remote,UIntPtr.Zero,0x8000); CloseHandle(process); }
        }
    }
}
