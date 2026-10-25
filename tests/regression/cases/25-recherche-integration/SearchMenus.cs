using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Automation;

namespace FileSpacerRegression {
    public static class SearchMenus {
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string name,string text);
        [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wParam,StringBuilder text,uint flags,uint timeout,out UIntPtr result);
        [DllImport("user32.dll",SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr window,uint message,UIntPtr wParam,IntPtr data,uint flags,uint timeout,out UIntPtr result);
        [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern uint RegisterWindowMessage(string name);
        public static uint SettingsMessage() {
            uint message=RegisterWindowMessage("FileSpacer.SettingsChanged");
            if(message==0) throw new Win32Exception();return message;
        }
        public static IntPtr List(IntPtr popup) {
            IntPtr list=FindWindowEx(popup,IntPtr.Zero,"ListBox",null);
            if(list==IntPtr.Zero) throw new InvalidOperationException("Missing native suggestions list");
            return list;
        }
        static int Message(IntPtr window,uint message) {
            UIntPtr result;
            if(SendMessageTimeout(window,message,UIntPtr.Zero,IntPtr.Zero,2,3000,out result)==IntPtr.Zero) throw new Win32Exception();
            return unchecked((int)result.ToUInt64());
        }
        public static string[] Items(IntPtr popup) {
            IntPtr list=List(popup);var values=new List<string>();
            int count=Message(list,0x18b); // LB_GETCOUNT
            if(count<0) throw new InvalidOperationException("Cannot read native suggestions");
            for(int index=0;index<count;++index) {
                var text=new StringBuilder(2048);UIntPtr result;
                if(SendMessageTimeout(list,0x189,(UIntPtr)(uint)index,text,2,3000,out result)==IntPtr.Zero) throw new Win32Exception();
                values.Add(text.Length==0?"<separator>":text.ToString());
            }
            return values.ToArray();
        }
        public static int Selected(IntPtr popup) {
            int selected=Message(List(popup),0x188); // LB_GETCURSEL
            if(selected<0) return -1;
            var items=Items(popup);int position=0;
            for(int index=0;index<selected;++index) if(items[index]!="<separator>") ++position;
            return items[selected]=="<separator>"?-1:position;
        }
        public static string[] ResultNames(IntPtr window) {
            string[] names=null;Exception error=null;
            var worker=new Thread(delegate() {
                try {
                    var root=AutomationElement.FromHandle(window);
                    var items=root.FindAll(TreeScope.Descendants,new OrCondition(
                        new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.DataItem),
                        new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.ListItem)));
                    var values=new List<string>();
                    foreach(AutomationElement item in items) values.Add(item.Current.Name);
                    values.Sort(StringComparer.Ordinal);names=values.ToArray();
                } catch(Exception failure) {error=failure;}
            });
            worker.IsBackground=true;worker.SetApartmentState(ApartmentState.MTA);worker.Start();
            if(!worker.Join(5000)) throw new TimeoutException("Everything result inspection timed out.");
            if(error!=null) throw new InvalidOperationException("Cannot inspect Everything results.",error);
            return names;
        }
    }
}
