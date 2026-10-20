using System;
using System.Collections.Generic;
using System.Globalization;
using System.Threading;
using AutomationRect = System.Windows.Rect;
using System.Windows.Automation;

namespace FileSpacerRegression {
    public sealed class VisibleItemPosition {
        public string Name;
        public double X, Y;
    }
    public sealed class VisibleViewLayout {
        public double ViewWidth, ViewHeight;
        public VisibleItemPosition[] Items;
        public string Signature {
            get {
                var parts=new List<string>();
                foreach(var item in Items) parts.Add(item.Name.Length+":"+item.Name+"="+
                    item.X.ToString("R",CultureInfo.InvariantCulture)+","+item.Y.ToString("R",CultureInfo.InvariantCulture));
                return String.Join(";",parts.ToArray());
            }
        }
    }
    // Observe visible geometry only: no control-class assumptions, injected code or remote buffers.
    // The real Shell HWND comes from IShellView::GetWindow, not from an Explorer window search.
    public static class ViewLayout {
        public static VisibleViewLayout Capture(IntPtr window,int processId,int expectedItems) {
            if(window==IntPtr.Zero || processId<=0 || expectedItems<=0) throw new ArgumentException("LAYOUT MEASUREMENT: invalid Shell view layout target.");
            VisibleViewLayout result=null; Exception error=null;
            var worker=new Thread(delegate() {
                try { result=Read(window,processId,expectedItems); }
                catch(Exception failure) { error=failure; }
            });
            worker.IsBackground=true;
            worker.SetApartmentState(ApartmentState.MTA);
            worker.Start();
            // A stalled provider must fail this measurement, not hang the whole test.
            if(!worker.Join(5000)) throw new TimeoutException("LAYOUT MEASUREMENT: UI Automation did not respond within 5 seconds.");
            if(error!=null) throw new InvalidOperationException("LAYOUT MEASUREMENT: "+error.Message,error);
            return result;
        }
        static object Required(AutomationElement element,AutomationProperty property) {
            object value=element.GetCurrentPropertyValue(property,true);
            if(Object.ReferenceEquals(value,AutomationElement.NotSupported))
                throw new InvalidOperationException("UI Automation property unavailable: "+property.ProgrammaticName);
            return value;
        }
        static AutomationRect Rectangle(AutomationElement element) {
            AutomationRect rect=(AutomationRect)Required(element,AutomationElement.BoundingRectangleProperty);
            if(rect.IsEmpty || rect.Width<=0 || rect.Height<=0 ||
                Double.IsNaN(rect.X) || Double.IsNaN(rect.Y) || Double.IsNaN(rect.Width) || Double.IsNaN(rect.Height) ||
                Double.IsInfinity(rect.X) || Double.IsInfinity(rect.Y) || Double.IsInfinity(rect.Width) || Double.IsInfinity(rect.Height))
                throw new InvalidOperationException("UI Automation returned an empty or invalid bounding rectangle.");
            return rect;
        }
        static VisibleViewLayout Read(IntPtr window,int processId,int expectedItems) {
            var root=AutomationElement.FromHandle(window);
            if(root==null) throw new InvalidOperationException("No UI Automation element for the registered Shell view HWND.");
            if((int)Required(root,AutomationElement.ProcessIdProperty)!=processId)
                throw new InvalidOperationException("UI Automation resolved a different process for the Shell view.");
            AutomationRect frame=Rectangle(root);
            var condition=new AndCondition(new PropertyCondition(AutomationElement.IsControlElementProperty,true),
                new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.ListItem));
            var elements=root.FindAll(TreeScope.Descendants,condition);
            if(elements.Count!=expectedItems) throw new InvalidOperationException(
                "Expected "+expectedItems+" Shell list items, UI Automation exposed "+elements.Count+".");
            var names=new Dictionary<string,bool>(StringComparer.OrdinalIgnoreCase);
            var positions=new List<VisibleItemPosition>();
            foreach(AutomationElement item in elements) {
                string name=(string)Required(item,AutomationElement.NameProperty);
                if(String.IsNullOrWhiteSpace(name) || names.ContainsKey(name))
                    throw new InvalidOperationException("Missing or ambiguous UI Automation item name: "+name);
                names.Add(name,true);
                if((bool)Required(item,AutomationElement.IsOffscreenProperty))
                    throw new InvalidOperationException("Shell item is offscreen: "+name);
                AutomationRect rect=Rectangle(item);
                if(!frame.IntersectsWith(rect)) throw new InvalidOperationException("Shell item is outside the view: "+name);
                positions.Add(new VisibleItemPosition { Name=name, X=rect.X-frame.X, Y=rect.Y-frame.Y });
            }
            positions.Sort(delegate(VisibleItemPosition a,VisibleItemPosition b) { return StringComparer.OrdinalIgnoreCase.Compare(a.Name,b.Name); });
            return new VisibleViewLayout { ViewWidth=frame.Width, ViewHeight=frame.Height, Items=positions.ToArray() };
        }
    }
}
