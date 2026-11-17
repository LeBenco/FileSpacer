using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

namespace FileSpacerViewMenuTests {
    // A genuine external OLE source: starting a FileSpacer drag on the same
    // thread would stop the target's previous IconSnap and hide this regression.
    public static class ExternalDragSource {
        [StructLayout(LayoutKind.Sequential)] struct Point { public int X,Y; }
        [DllImport("user32.dll")] static extern bool GetCursorPos(out Point point);
        [StructLayout(LayoutKind.Sequential)] struct Rect { public int Left,Top,Right,Bottom; }
        delegate bool EnumChild(IntPtr window,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent,EnumChild callback,IntPtr data);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr window,StringBuilder text,int size);
        [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr window,out Rect bounds);
        [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
        static string Quote(string text) { return "'"+text.Replace("'","''")+"'"; }
        public static Process Start(string root) {
            string code="[void][Reflection.Assembly]::LoadFrom("+Quote(typeof(ExternalDragSource).Assembly.Location)+
                ");[FileSpacerViewMenuTests.ExternalDragSource]::Run("+Quote(root)+")";
            var start=new ProcessStartInfo(Path.Combine(Environment.SystemDirectory,"WindowsPowerShell\\v1.0\\powershell.exe"),
                "-NoProfile -STA -EncodedCommand "+Convert.ToBase64String(Encoding.Unicode.GetBytes(code)));
            start.UseShellExecute=false;start.CreateNoWindow=true;
            return Process.Start(start);
        }
        public static bool SHeld() { return GetAsyncKeyState(0x53)<0; }
        public static int[] ButtonPoint(IntPtr parent,string name) {
            int[] result=null;
            EnumChildWindows(parent,delegate(IntPtr child,IntPtr data) {
                var text=new StringBuilder(128);GetWindowText(child,text,text.Capacity);
                if(text.ToString()!=name)return true;
                Rect bounds;
                if(!GetWindowRect(child,out bounds))throw new InvalidOperationException("External source bounds unavailable.");
                result=new int[]{(bounds.Left+bounds.Right)/2,(bounds.Top+bounds.Bottom)/2};return false;
            },IntPtr.Zero);
            if(result==null)throw new InvalidOperationException("External source button unavailable: "+name);
            return result;
        }
        public static void Run(string root) {
            Application.EnableVisualStyles();
            using(var form=new Form()) {
                form.Text="External drop source "+Path.GetFileName(root);
                form.StartPosition=FormStartPosition.Manual;form.Left=850;form.Top=40;
                form.Width=280;form.Height=210;
                int top=15;
                foreach(string name in new string[]{"folder-drop.txt","background-drop.txt"}) {
                    var button=new Button();button.Text=name;button.Left=15;button.Top=top;
                    button.Width=235;button.Height=55;top+=65;
                    string file=Path.Combine(root,name);
                    button.GiveFeedback+=delegate(object sender,GiveFeedbackEventArgs e) {
                        Point point;
                        if(GetCursorPos(out point)) File.WriteAllText(file+".hover",
                            String.Format("{0},{1},{2}",point.X,point.Y,(int)e.Effect));
                    };
                    button.MouseDown+=delegate(object sender,MouseEventArgs e) {
                        if(e.Button!=MouseButtons.Left)return;
                        File.WriteAllText(file+".started","MouseDown");
                        var value=new DataObject(DataFormats.FileDrop,new string[]{file});
                        var effect=button.DoDragDrop(value,DragDropEffects.Copy|DragDropEffects.Move|DragDropEffects.Link);
                        File.WriteAllText(Path.Combine(root,name+".effect"),effect.ToString());
                    };
                    form.Controls.Add(button);
                }
                Application.Run(form);
            }
        }
    }
}
