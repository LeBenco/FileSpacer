using System;
using System.Windows.Automation;

public static class StorageTaskDialog {
    public static bool HasButton(IntPtr dialog,int id) {
        try {
            var root=AutomationElement.FromHandle(dialog);
            var condition=new PropertyCondition(AutomationElement.AutomationIdProperty,"CommandButton_"+id);
            var buttons=root.FindAll(TreeScope.Descendants,condition);
            foreach(AutomationElement button in buttons) {
                var parent=TreeWalker.ControlViewWalker.GetParent(button);
                while(parent!=null && parent.Current.ControlType!=ControlType.Window)
                    parent=TreeWalker.ControlViewWalker.GetParent(parent);
                // An owned confirmation can appear under its parent in UIA.
                // Only count controls belonging to this exact task dialog.
                if(parent!=null && Automation.Compare(parent,root) && !button.Current.IsOffscreen) return true;
            }
            return false;
        } catch(ElementNotAvailableException) { return false; }
    }
}
