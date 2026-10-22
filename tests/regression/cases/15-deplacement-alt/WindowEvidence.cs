using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace FileSpacerRegression {
    public sealed class WindowEvidenceRow {
        public long Handle,Owner,RootOwner; public uint ProcessId,ThreadId;
        public string Class,Title; public bool Visible,Enabled,MatchesTestProcess,MatchesTargetName;
        public bool ChildrenTruncated; public string[] Children;
    }
    public sealed class WindowEvidenceSnapshot {
        public long TestWindow,Foreground; public uint TestProcessId;
        public string ForegroundClass; public WindowEvidenceRow[] Windows;
    }
    public static class WindowEvidence {
        delegate bool EnumWindow(IntPtr window,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumWindows(EnumWindow callback,IntPtr data);
        [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr window,EnumWindow callback,IntPtr data);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr w,StringBuilder text,int count);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr w,StringBuilder text,int count);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr w,out uint process);
        [DllImport("user32.dll")] static extern IntPtr GetWindow(IntPtr w,uint command);
        [DllImport("user32.dll")] static extern IntPtr GetAncestor(IntPtr w,uint flags);
        [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
        [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr w);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr SendMessageTimeout(
            IntPtr w,uint message,UIntPtr wp,StringBuilder text,uint flags,uint timeout,out UIntPtr result);
        static string ClassName(IntPtr window) { var s=new StringBuilder(128); GetClassName(window,s,s.Capacity); return s.ToString(); }
        static string Caption(IntPtr window) { var s=new StringBuilder(2048); GetWindowText(window,s,s.Capacity); return s.ToString(); }
        static string ControlText(IntPtr window) {
            var s=new StringBuilder(2048); UIntPtr result;
            // WM_GETTEXT is system-marshalled; this is a bounded read only.
            return SendMessageTimeout(window,0x0d,new UIntPtr((uint)s.Capacity),s,2,200,out result)==IntPtr.Zero
                ? "<text read unavailable>" : s.ToString();
        }
        public static WindowEvidenceSnapshot Capture(IntPtr testWindow,uint testProcessId,string targetName) {
            var rows=new List<WindowEvidenceRow>(); var budget=Stopwatch.StartNew();
            IntPtr foreground=GetForegroundWindow();
            EnumWindows(delegate(IntPtr window,IntPtr data) {
                uint process; uint thread=GetWindowThreadProcessId(window,out process);
                string title=Caption(window),kind=ClassName(window);
                bool sameProcess=process==testProcessId;
                bool named=title.IndexOf(targetName,StringComparison.OrdinalIgnoreCase)>=0;
                // Include an unexpected foreground dialog as well: it can
                // reveal an error or Properties for the wrong target.
                bool foregroundDialog=window==foreground && kind=="#32770";
                if(!sameProcess && !named && !foregroundDialog && GetAncestor(window,3)!=testWindow) return true;
                var row=new WindowEvidenceRow { Handle=window.ToInt64(),Owner=GetWindow(window,4).ToInt64(),
                    RootOwner=GetAncestor(window,3).ToInt64(),ProcessId=process,ThreadId=thread,
                    Class=kind,Title=title,Visible=IsWindowVisible(window),Enabled=IsWindowEnabled(window),
                    MatchesTestProcess=sameProcess,MatchesTargetName=named };
                var children=new List<string>();
                EnumChildWindows(window,delegate(IntPtr child,IntPtr unused) {
                    if(budget.ElapsedMilliseconds>=2000 || children.Count>=128) { row.ChildrenTruncated=true; return false; }
                    string childClass=ClassName(child);
                    // Dialog labels/edit fields identify target/path or an error.
                    // For other controls only record the class and handle.
                    string text=(childClass=="Static"||childClass=="Edit"||childClass=="Button") ? ControlText(child) : "";
                    children.Add(child.ToInt64()+":"+childClass+":"+text);
                    return true;
                },IntPtr.Zero);
                row.Children=children.ToArray(); rows.Add(row); return true;
            },IntPtr.Zero);
            return new WindowEvidenceSnapshot { TestWindow=testWindow.ToInt64(),TestProcessId=testProcessId,
                Foreground=foreground.ToInt64(),ForegroundClass=ClassName(foreground),Windows=rows.ToArray() };
        }
    }
}
