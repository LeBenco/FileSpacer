// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace FileSpacerInstallerTests {
    public static class WizardArtwork {
        [StructLayout(LayoutKind.Sequential)]
        private struct Rect { public int Left, Top, Right, Bottom; }
        [StructLayout(LayoutKind.Sequential)]
        private struct Point { public int X, Y; }
        private delegate bool EnumWindow(IntPtr window, IntPtr data);
        [DllImport("user32.dll")] private static extern IntPtr GetDlgItem(IntPtr window, int id);
        [DllImport("user32.dll")] private static extern bool GetClientRect(IntPtr window, out Rect rect);
        [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr window);
        [DllImport("user32.dll")] private static extern bool SetProcessDPIAware();
        [DllImport("user32.dll")] private static extern bool ClientToScreen(IntPtr window, ref Point point);
        [DllImport("user32.dll")] private static extern IntPtr WindowFromPoint(Point point);
        [DllImport("user32.dll")] private static extern bool PostMessageW(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
        [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindow callback, IntPtr data);
        [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr SendMessageW(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
        [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")]
        private static extern IntPtr ReadText(IntPtr window, uint message, IntPtr length, StringBuilder text);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern IntPtr LoadImageW(IntPtr module, string path, uint type, int width, int height, uint flags);
        [DllImport("gdi32.dll")] private static extern bool DeleteObject(IntPtr value);

        private static void Require(bool condition, string message) {
            if (!condition) throw new InvalidOperationException(message);
        }

        private static IntPtr WaitForPanel(Process process) {
            Stopwatch timeout=Stopwatch.StartNew();
            while (timeout.ElapsedMilliseconds < 30000) {
                Require(!process.HasExited, "Installer exited before showing its panel.");
                process.Refresh();
                IntPtr window=process.MainWindowHandle;
                IntPtr panel=GetDlgItem(window,1025);
                if (panel!=IntPtr.Zero && IsWindowVisible(panel) &&
                    SendMessageW(panel,0x0173,IntPtr.Zero,IntPtr.Zero)!=IntPtr.Zero) return window;
                Thread.Sleep(50);
            }
            throw new TimeoutException("Installer artwork did not become ready.");
        }

        private static void Cancel(Process process, IntPtr window) {
            if (process.HasExited) return;
            Require(PostMessageW(window,0x0111,new IntPtr(2),IntPtr.Zero), "Could not request cancellation.");
            Stopwatch timeout=Stopwatch.StartNew();
            while (!process.HasExited && timeout.ElapsedMilliseconds < 10000) {
                EnumWindows(delegate(IntPtr candidate, IntPtr unused) {
                    uint owner;
                    GetWindowThreadProcessId(candidate,out owner);
                    if (owner==(uint)process.Id && IsWindowVisible(candidate)) {
                        IntPtr yes=GetDlgItem(candidate,6);
                        if (yes!=IntPtr.Zero) PostMessageW(yes,0x00F5,IntPtr.Zero,IntPtr.Zero);
                    }
                    return true;
                },IntPtr.Zero);
                Thread.Sleep(50);
            }
            Require(process.HasExited,"Installer did not close after cancellation.");
            Require(process.ExitCode==1,"Cancellation returned "+process.ExitCode+" instead of 1.");
        }

        private static int CaptureStable(Graphics graphics, Bitmap image, Point origin) {
            // Native input-idle precedes DWM's opening animation. Wait for stable
            // screen pixels independently of the expected interpolation result.
            using (Bitmap previous=new Bitmap(image.Width,image.Height)) {
                int stable=0, samples=0;
                Stopwatch timeout=Stopwatch.StartNew();
                while (timeout.ElapsedMilliseconds<10000) {
                    graphics.CopyFromScreen(origin.X,origin.Y,0,0,image.Size);
                    bool equal=samples>0;
                    for (int y=0;equal && y<image.Height;y++) for (int x=0;equal && x<image.Width;x++)
                        equal=image.GetPixel(x,y).ToArgb()==previous.GetPixel(x,y).ToArgb();
                    samples++;
                    stable=equal ? stable+1 : 0;
                    if (stable==2) return samples;
                    using (Graphics copy=Graphics.FromImage(previous)) copy.DrawImageUnscaled(image,0,0);
                    Thread.Sleep(100);
                }
            }
            throw new TimeoutException("The sidebar screen pixels did not stabilize.");
        }

        private static void CreateReference(string script, string source, int width, int height, string destination) {
            string powershell=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows),
                @"SysWOW64\WindowsPowerShell\v1.0\powershell.exe");
            string arguments="-NoProfile -ExecutionPolicy Bypass -File \""+script+"\" -SourcePath \""+source+
                "\" -Width "+width+" -Height "+height+" -OutputPath \""+destination+"\"";
            using (Process process=Process.Start(new ProcessStartInfo(powershell,arguments) {
                UseShellExecute=false, CreateNoWindow=true, RedirectStandardOutput=true, RedirectStandardError=true })) {
                var stdout=process.StandardOutput.ReadToEndAsync();
                var stderr=process.StandardError.ReadToEndAsync();
                Require(process.WaitForExit(15000),"x86 bicubic reference timed out.");
                File.WriteAllText(destination+".stdout.log",stdout.Result);
                File.WriteAllText(destination+".stderr.log",stderr.Result);
                Require(process.ExitCode==0,"x86 bicubic reference failed: "+stderr.Result);
            }
        }

        public static void Check(string executable, string arguments, string sourcePath, string referenceScript,
            string output, string name, int language) {
            SetProcessDPIAware();
            using (Process process=Process.Start(new ProcessStartInfo(executable,arguments) { UseShellExecute=false })) {
                IntPtr window=IntPtr.Zero;
                Exception failure=null;
                try {
                    window=WaitForPanel(process);
                    Require(process.WaitForInputIdle(10000),"Installer did not become idle.");
                    StringBuilder caption=new StringBuilder(256);
                    ReadText(GetDlgItem(window,1051),0x000D,new IntPtr(caption.Capacity),caption);
                    Require(caption.ToString().StartsWith(language==1036 ? "Gestionnaire" : "Spatial",StringComparison.Ordinal),"Unexpected branding language: "+caption);
                    IntPtr panel=GetDlgItem(window,1025);
                    Rect rect;
                    Require(GetClientRect(panel,out rect),"Cannot measure the actual sidebar.");
                    int width=rect.Right, height=rect.Bottom;
                    string referencePath=Path.Combine(output,name+"-expected.png");
                    CreateReference(referenceScript,sourcePath,width,height,referencePath);
                    using (Bitmap actual=new Bitmap(width,height,PixelFormat.Format32bppArgb))
                    using (Graphics capture=Graphics.FromImage(actual))
                    using (Bitmap expected=new Bitmap(referencePath)) {
                        Point origin=new Point();
                        Require(ClientToScreen(panel,ref origin),"Cannot locate the sidebar on screen.");
                        Point center=new Point { X=origin.X+width/2, Y=origin.Y+height/2 };
                        Require(WindowFromPoint(center)==panel,"The actual sidebar is covered by another window.");
                        int samples=CaptureStable(capture,actual,origin);
                        actual.Save(Path.Combine(output,name+"-actual.png"),ImageFormat.Png);
                        IntPtr old=LoadImageW(IntPtr.Zero,sourcePath,0,width,height,0x10);
                        Require(old!=IntPtr.Zero,"Cannot obtain the previous NSIS scaling reference.");
                        try {
                            using (Bitmap previous=Image.FromHbitmap(old)) {
                                int mismatches=0, changed=0;
                                for (int y=0;y<height;y++) for (int x=0;x<width;x++) {
                                    Color pixel=actual.GetPixel(x,y), reference=expected.GetPixel(x,y);
                                    if (Math.Abs(pixel.R-reference.R)>1 || Math.Abs(pixel.G-reference.G)>1 || Math.Abs(pixel.B-reference.B)>1) mismatches++;
                                    if (pixel.ToArgb()!=previous.GetPixel(x,y).ToArgb()) changed++;
                                }
                                Require(mismatches==0,"Sidebar differs from bicubic reference at "+mismatches+" pixels.");
                                Require(changed>100,"The sidebar still matches the old NSIS scaling.");
                                File.WriteAllText(Path.Combine(output,name+".txt"),"PASS: "+width+"x"+height+"; stable after "+samples+" captures; bicubic mismatches="+mismatches+"; pixels changed from NSIS="+changed+Environment.NewLine);
                            }
                        } finally { DeleteObject(old); }
                    }
                } catch (Exception error) { failure=error; }
                finally {
                    if (window!=IntPtr.Zero) {
                        try { Cancel(process,window); }
                        catch (Exception cleanup) {
                            failure=failure==null ? cleanup : new AggregateException(failure,cleanup);
                        }
                    }
                }
                if (failure!=null) throw failure;
            }
        }
    }
}
