// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
using System;
using System.ComponentModel;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Collections.Generic;
using System.Text;
using System.Globalization;

public static class CaptureDesktop {
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint processId);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr SendMessageW(IntPtr window,uint message,IntPtr wParam,IntPtr lParam);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr OpenProcess(uint access,bool inherit,uint processId);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr address,UIntPtr size,uint type,uint protect);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr read);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process,IntPtr address,byte[] buffer,UIntPtr size,out UIntPtr written);
    [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process,IntPtr address,UIntPtr size,uint type);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [StructLayout(LayoutKind.Sequential)] struct ScreenPoint { public int X,Y; }
    [StructLayout(LayoutKind.Sequential)] struct FindInfo { public uint Flags; public IntPtr Text,Data; public ScreenPoint Point; public uint Direction; }
    [DllImport("user32.dll",SetLastError=true)] static extern bool ClientToScreen(IntPtr window,ref ScreenPoint point);
    [DllImport("user32.dll")] static extern IntPtr WindowFromPoint(ScreenPoint point);
    [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr window,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr window,StringBuilder name,int capacity);
    public static bool OwnsPoint(IntPtr window,int x,int y) {
        return GetAncestor(WindowFromPoint(new ScreenPoint{X=x,Y=y}),2)==window;
    }
    public static bool IsDesktopPoint(int x,int y) {
        IntPtr root=GetAncestor(WindowFromPoint(new ScreenPoint{X=x,Y=y}),2);
        var name=new StringBuilder(256);
        GetClassNameW(root,name,name.Capacity);
        return name.ToString()=="Progman" || name.ToString()=="WorkerW";
    }
    public static Rectangle TabItem(IntPtr tab,int index) {
        // TCM_GETITEMRECT returns the actual localized tab bounds. Like the bench's
        // TB_GETRECT helper, this message above WM_USER needs a RECT in its owner process.
        uint processId; GetWindowThreadProcessId(tab,out processId);
        IntPtr process=OpenProcess(0x18,false,processId);
        if(process==IntPtr.Zero) throw new Win32Exception();
        IntPtr remote=IntPtr.Zero;
        try {
            remote=VirtualAllocEx(process,IntPtr.Zero,new UIntPtr(16),0x3000,4);
            if(remote==IntPtr.Zero) throw new Win32Exception();
            if(SendMessageW(tab,0x130a,new IntPtr(index),remote)==IntPtr.Zero) throw new InvalidOperationException("The tab has no rectangle: "+index);
            var bytes=new byte[16]; UIntPtr read;
            if(!ReadProcessMemory(process,remote,bytes,new UIntPtr(16),out read)||read.ToUInt64()!=16) throw new Win32Exception();
            int left=BitConverter.ToInt32(bytes,0),top=BitConverter.ToInt32(bytes,4);
            int right=BitConverter.ToInt32(bytes,8),bottom=BitConverter.ToInt32(bytes,12);
            var point=new ScreenPoint{X=left,Y=top};
            if(!ClientToScreen(tab,ref point)) throw new Win32Exception();
            return new Rectangle(point.X,point.Y,right-left,bottom-top);
        } finally {
            if(remote!=IntPtr.Zero) VirtualFreeEx(process,remote,UIntPtr.Zero,0x8000);
            CloseHandle(process);
        }
    }
    public static Rectangle CatalogItem(IntPtr catalog,string label) {
        uint processId; GetWindowThreadProcessId(catalog,out processId);
        IntPtr process=OpenProcess(0x38,false,processId);
        if(process==IntPtr.Zero) throw new Win32Exception();
        IntPtr remote=IntPtr.Zero,local=IntPtr.Zero;
        try {
            int headerSize=Marshal.SizeOf(typeof(FindInfo));
            byte[] text=Encoding.Unicode.GetBytes(label+"\0");
            byte[] buffer=new byte[headerSize+text.Length];
            remote=VirtualAllocEx(process,IntPtr.Zero,new UIntPtr((uint)buffer.Length),0x3000,4);
            if(remote==IntPtr.Zero) throw new Win32Exception();
            local=Marshal.AllocHGlobal(headerSize);
            Marshal.StructureToPtr(new FindInfo{Flags=2,Text=IntPtr.Add(remote,headerSize)},local,false);
            Marshal.Copy(local,buffer,0,headerSize);
            Array.Copy(text,0,buffer,headerSize,text.Length);
            UIntPtr written;
            if(!WriteProcessMemory(process,remote,buffer,new UIntPtr((uint)buffer.Length),out written)||written.ToUInt64()!=(ulong)buffer.Length) throw new Win32Exception();
            int index=SendMessageW(catalog,0x1053,new IntPtr(-1),remote).ToInt32(); // LVM_FINDITEMW, exact localized label.
            if(index<0) throw new InvalidOperationException("Catalog action not found: "+label);
            buffer=new byte[16]; // RECT.left = LVIR_BOUNDS.
            if(!WriteProcessMemory(process,remote,buffer,new UIntPtr(16),out written)||written.ToUInt64()!=16) throw new Win32Exception();
            if(SendMessageW(catalog,0x100e,new IntPtr(index),remote)==IntPtr.Zero) throw new InvalidOperationException("Catalog row has no bounds.");
            UIntPtr read;
            if(!ReadProcessMemory(process,remote,buffer,new UIntPtr(16),out read)||read.ToUInt64()!=16) throw new Win32Exception();
            int left=BitConverter.ToInt32(buffer,0),top=BitConverter.ToInt32(buffer,4);
            int right=BitConverter.ToInt32(buffer,8),bottom=BitConverter.ToInt32(buffer,12);
            var point=new ScreenPoint{X=left,Y=top};
            if(!ClientToScreen(catalog,ref point)) throw new Win32Exception();
            return new Rectangle(point.X,point.Y,right-left,bottom-top);
        } finally {
            if(local!=IntPtr.Zero) Marshal.FreeHGlobal(local);
            if(remote!=IntPtr.Zero) VirtualFreeEx(process,remote,UIntPtr.Zero,0x8000);
            CloseHandle(process);
        }
    }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    static extern IntPtr LoadLibraryExW(string file,IntPtr reserved,uint flags);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    delegate bool ResourceLanguage(IntPtr module,IntPtr type,IntPtr name,ushort language,IntPtr parameter);
    [DllImport("kernel32.dll",ExactSpelling=true,SetLastError=true)]
    static extern bool EnumResourceLanguagesW(IntPtr module,IntPtr type,IntPtr name,ResourceLanguage callback,IntPtr parameter);
    [DllImport("kernel32.dll",ExactSpelling=true,SetLastError=true)]
    static extern IntPtr FindResourceExW(IntPtr module,IntPtr type,IntPtr name,ushort language);
    [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr LoadResource(IntPtr module,IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll",SetLastError=true)] static extern uint SizeofResource(IntPtr module,IntPtr resource);
    public static string Resource(string file,uint id,string language) {
        var culture=CultureInfo.GetCultureInfo(language);
        if(id>UInt16.MaxValue)throw new ArgumentOutOfRangeException("id");
        IntPtr module=LoadLibraryExW(file,IntPtr.Zero,0x22);
        if(module==IntPtr.Zero)throw new Win32Exception();
        try {
            IntPtr type=new IntPtr(6),block=new IntPtr((id>>4)+1); // RT_STRING: 16 counted UTF-16 strings per block.
            var available=new List<ushort>();
            ResourceLanguage callback=delegate(IntPtr m,IntPtr t,IntPtr n,ushort value,IntPtr p){available.Add(value);return true;};
            if(!EnumResourceLanguagesW(module,type,block,callback,IntPtr.Zero))throw new Win32Exception();
            ushort selected=0;bool found=false;
            foreach(ushort candidate in available)if(candidate==(ushort)culture.LCID){selected=candidate;found=true;break;}
            if(!found)foreach(ushort candidate in available) {
                try {
                    if(CultureInfo.GetCultureInfo(candidate).TwoLetterISOLanguageName!=culture.TwoLetterISOLanguageName)continue;
                    selected=candidate;found=true;break;
                }catch(CultureNotFoundException){}
            }
            if(!found)throw new InvalidOperationException("Missing resource language "+language);
            IntPtr resource=FindResourceExW(module,type,block,selected);
            if(resource==IntPtr.Zero)throw new Win32Exception();
            int size=checked((int)SizeofResource(module,resource));
            IntPtr handle=LoadResource(module,resource);
            if(handle==IntPtr.Zero)throw new Win32Exception();
            IntPtr data=LockResource(handle);
            if(data==IntPtr.Zero)throw new InvalidOperationException("Cannot read string resource.");
            int offset=0;
            for(uint index=0;index<=id%16;index++) {
                if(offset+2>size)throw new InvalidOperationException("Truncated string resource.");
                int length=(ushort)Marshal.ReadInt16(data,offset);offset+=2;
                if(offset+length*2>size)throw new InvalidOperationException("Truncated string resource.");
                if(index==id%16) {
                    if(length==0)throw new InvalidOperationException("Missing localized string "+id);
                    // This application compiles resources with rc.exe /n.
                    return Marshal.PtrToStringUni(IntPtr.Add(data,offset),length).TrimEnd('\0');
                }
                offset+=length*2;
            }
            throw new InvalidOperationException("Missing string resource "+id);
        } finally {FreeLibrary(module);}
    }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    public struct DisplayMode {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string DeviceName;
        public ushort SpecVersion, DriverVersion, Size, DriverExtra;
        public uint Fields;
        public int PositionX, PositionY;
        public uint Orientation, FixedOutput;
        public short Color, Duplex, YResolution, TTOption, Collate;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string FormName;
        public ushort LogPixels;
        public uint BitsPerPixel, Width, Height, Flags, Frequency, IcmMethod, IcmIntent, MediaType, DitherType;
        public uint Reserved1, Reserved2, PanningWidth, PanningHeight;
    }
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    static extern bool EnumDisplaySettings(string device, int mode, ref DisplayMode value);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    static extern int ChangeDisplaySettings(ref DisplayMode value, uint flags);
    public static DisplayMode CurrentMode() {
        var mode=new DisplayMode(); mode.Size=(ushort)Marshal.SizeOf(typeof(DisplayMode));
        if(!EnumDisplaySettings(null,-1,ref mode)) throw new Win32Exception(); return mode;
    }
    public static DisplayMode[] Modes() {
        var modes=new List<DisplayMode>();
        for(int index=0;;index++) {
            var mode=new DisplayMode(); mode.Size=(ushort)Marshal.SizeOf(typeof(DisplayMode));
            if(!EnumDisplaySettings(null,index,ref mode)) break; modes.Add(mode);
        }
        return modes.ToArray();
    }
    public static void SetMode(DisplayMode mode) {
        int result=ChangeDisplaySettings(ref mode,0);
        if(result!=0) throw new InvalidOperationException("ChangeDisplaySettings failed: "+result);
    }
    [ComImport, Guid("B92B56A9-8B55-4E14-9A89-0199BBB6F93B"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface DesktopWallpaper {
        void SetWallpaper([MarshalAs(UnmanagedType.LPWStr)] string monitor, [MarshalAs(UnmanagedType.LPWStr)] string path);
        void GetWallpaper([MarshalAs(UnmanagedType.LPWStr)] string monitor, [MarshalAs(UnmanagedType.LPWStr)] out string path);
        void GetMonitorDevicePathAt(uint index, [MarshalAs(UnmanagedType.LPWStr)] out string monitor);
        void GetMonitorDevicePathCount(out uint count);
        void GetMonitorRECT([MarshalAs(UnmanagedType.LPWStr)] string monitor, out RectangleBounds bounds);
        void SetBackgroundColor(uint color);
        void GetBackgroundColor(out uint color);
        void SetPosition(uint position);
        void GetPosition(out uint position);
    }
    [DllImport("dwmapi.dll")]
    static extern int DwmGetWindowAttribute(IntPtr window, uint attribute, out RectangleBounds value, int size);
    [StructLayout(LayoutKind.Sequential)]
    public struct RectangleBounds { public int Left, Top, Right, Bottom; }

    public static void Wallpaper(string path) {
        var desktop=(DesktopWallpaper)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("C2CF3110-460E-4FC1-B9D0-8A1C0C9CC4BD")));
        try {
            desktop.SetPosition(3); // DWPOS_FIT: complete image at its original aspect ratio.
            desktop.SetWallpaper(null,path);
            uint position; desktop.GetPosition(out position);
            if(position!=3) throw new InvalidOperationException("Windows did not retain DWPOS_FIT");
        } finally { Marshal.ReleaseComObject(desktop); }
    }
    public sealed class WallpaperState { public string Path; public uint Position; }
    public static WallpaperState GetWallpaperState() {
        var desktop=(DesktopWallpaper)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("C2CF3110-460E-4FC1-B9D0-8A1C0C9CC4BD")));
        try {
            string path; uint position;
            desktop.GetWallpaper(null,out path); desktop.GetPosition(out position);
            return new WallpaperState{Path=path,Position=position};
        } finally { Marshal.ReleaseComObject(desktop); }
    }
    public static void RestoreWallpaper(WallpaperState state) {
        var desktop=(DesktopWallpaper)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("C2CF3110-460E-4FC1-B9D0-8A1C0C9CC4BD")));
        try { desktop.SetPosition(state.Position); desktop.SetWallpaper(null,state.Path); }
        finally { Marshal.ReleaseComObject(desktop); }
    }
    public static void VerifyWallpaper(string expected) {
        var actual=GetWallpaperState();
        if(actual.Position!=3 || String.IsNullOrEmpty(actual.Path)) throw new InvalidOperationException("The approved wallpaper is not active in Fit mode.");
        using(var hash=System.Security.Cryptography.SHA256.Create()) {
            var wanted=Convert.ToBase64String(hash.ComputeHash(System.IO.File.ReadAllBytes(expected)));
            var current=Convert.ToBase64String(hash.ComputeHash(System.IO.File.ReadAllBytes(actual.Path)));
            if(current!=wanted) throw new InvalidOperationException("The visible desktop uses a different wallpaper file.");
        }
    }
    public static RectangleBounds Frame(IntPtr window) {
        RectangleBounds value;
        int result=DwmGetWindowAttribute(window, 9, out value, Marshal.SizeOf(typeof(RectangleBounds)));
        if(result<0) Marshal.ThrowExceptionForHR(result);
        return value;
    }
    public static void Region(string file, int left, int top, int width, int height) {
        var rect=new Rectangle(left,top,width,height);
        if(!SystemInformation.VirtualScreen.Contains(rect)) throw new InvalidOperationException("Capture exceeds the actual desktop");
        using(var bitmap=new Bitmap(width,height,PixelFormat.Format32bppArgb))
        using(var graphics=Graphics.FromImage(bitmap)) {
            graphics.CopyFromScreen(left,top,0,0,bitmap.Size);
            bitmap.Save(file,ImageFormat.Png);
        }
    }
    public static void Window(string file, IntPtr window) {
        var r=Frame(window); Region(file,r.Left,r.Top,r.Right-r.Left,r.Bottom-r.Top);
    }
    public static void Desktop(string file) {
        var r=SystemInformation.VirtualScreen; Region(file,r.Left,r.Top,r.Width,r.Height);
    }
}
