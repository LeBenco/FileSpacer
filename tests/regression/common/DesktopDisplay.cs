// SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
using System;
using System.ComponentModel;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows.Automation;

namespace FileSpacerRegression {
 public sealed class DisplayScale : IDisposable {
  AutomationElement combo;IntPtr window;public int OriginalPercent;public uint WindowDpi;
  public static string[] LastSettingsControls=new string[0];
  [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
  [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr window,uint message,IntPtr first,IntPtr second);
  static readonly PropertyCondition combos=new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.ComboBox);
  static void OpenSettings() {
   // Delegate through the desktop's Shell object, rather than the restricted worker token.
   // https://learn.microsoft.com/windows/win32/shell/samples-execinexplorer
   object shell=null,windows=null,browser=null,document=null,application=null;
   try {
    shell=Activator.CreateInstance(Type.GetTypeFromProgID("Shell.Application",true));
    windows=shell.GetType().InvokeMember("Windows",BindingFlags.InvokeMethod,null,shell,null);
    object[] arguments={null,null,8,0,1}; // SWC_DESKTOP, SWFO_NEEDDISPATCH (exdisp.h).
    var refs=new ParameterModifier(5);refs[0]=true;refs[1]=true;refs[3]=true;
    browser=windows.GetType().InvokeMember("FindWindowSW",BindingFlags.InvokeMethod,null,windows,
     arguments,new[]{refs},System.Globalization.CultureInfo.InvariantCulture,null);
    if(browser==null)throw new InvalidOperationException("The desktop Shell browser is unavailable.");
    document=browser.GetType().InvokeMember("Document",BindingFlags.GetProperty,null,browser,null);
    application=document.GetType().InvokeMember("Application",BindingFlags.GetProperty,null,document,null);
    application.GetType().InvokeMember("ShellExecute",BindingFlags.InvokeMethod,null,application,
     new object[]{"ms-settings:display",null,null,"open",1});
   } finally {
    foreach(object item in new[]{application,document,browser,windows,shell})
     if(item!=null&&Marshal.IsComObject(item))Marshal.ReleaseComObject(item);
   }
  }
  static string Selected(AutomationElement element) {
   object p;if(element.TryGetCurrentPattern(SelectionPattern.Pattern,out p)){var selected=((SelectionPattern)p).Current.GetSelection();if(selected.Length==1)return selected[0].Current.Name;}
   if(element.TryGetCurrentPattern(ValuePattern.Pattern,out p))return ((ValuePattern)p).Current.Value;return "";
  }
  static int Percent(string value) {var m=Regex.Match(value,@"(?<!\d)(\d{2,3})\s*%");return m.Success?int.Parse(m.Groups[1].Value):0;}
  static AutomationElement FindCombo() {
   var all=AutomationElement.RootElement.FindAll(TreeScope.Descendants,combos);
   foreach(AutomationElement e in all)if(Percent(Selected(e))!=0)return e;return null;
  }
  static AutomationElement SettingsWindow() {
   var all=AutomationElement.RootElement.FindAll(TreeScope.Children,new PropertyCondition(AutomationElement.ControlTypeProperty,ControlType.Window));
   foreach(AutomationElement e in all){string name=e.Current.Name.Trim();if(name=="Paramètres"||name=="Settings")return e;}return null;
  }
  static void CaptureUnavailableControls() {
   try {
    var lines=new System.Collections.Generic.List<string>();var root=SettingsWindow();
    if(root!=null){
     var items=root.FindAll(TreeScope.Descendants,Condition.TrueCondition);
     foreach(AutomationElement item in items){
      lines.Add(item.Current.ControlType.ProgrammaticName+"|"+item.Current.AutomationId+"|"+item.Current.Name+"|enabled="+item.Current.IsEnabled);
      if(lines.Count==256)break;
     }
    }
    LastSettingsControls=lines.ToArray();
   }catch(Exception error){LastSettingsControls=new[]{"Settings diagnostics unavailable: "+error.Message};}
  }
  public DisplayScale() : this(false) {}
  public DisplayScale(bool useOpenedTestSettings) {
   combo=FindCombo();
   if(combo!=null&&!useOpenedTestSettings)throw new InvalidOperationException("Preserve already open scaling settings.");
   if(combo==null)OpenSettings();
   var until=DateTime.UtcNow.AddSeconds(20);
   while(DateTime.UtcNow<until && (combo=FindCombo())==null){var settings=SettingsWindow();object p;if(settings!=null&&settings.TryGetCurrentPattern(WindowPattern.Pattern,out p)){((WindowPattern)p).SetWindowVisualState(WindowVisualState.Maximized);window=new IntPtr(settings.Current.NativeWindowHandle);}Thread.Sleep(100);}
   if(combo==null){CaptureUnavailableControls();if(window!=IntPtr.Zero)PostMessage(window,0x10,IntPtr.Zero,IntPtr.Zero);throw new InvalidOperationException("Display scale combo unavailable.");}
   var root=combo;var walker=TreeWalker.ControlViewWalker;
   for(;;){var parent=walker.GetParent(root);if(parent==null||Automation.Compare(parent,AutomationElement.RootElement))break;root=parent;}
   window=new IntPtr(root.Current.NativeWindowHandle);if(window==IntPtr.Zero)throw new InvalidOperationException("Cannot own Settings window.");
   OriginalPercent=Percent(Selected(combo));WindowDpi=GetDpiForWindow(window);
  }
  public int Current {get{combo=FindCombo();if(combo==null)throw new ElementNotAvailableException("Scaling control is not currently exposed.");return Percent(Selected(combo));}}

  public void Hide() { ShowWindow(window,6); }
  public void Set(int percent) {
   ShowWindow(window,9);
   var ready=DateTime.UtcNow.AddSeconds(15);bool exposed=false;
   while(DateTime.UtcNow<ready){try{if(Current!=0){exposed=true;break;}}catch(ElementNotAvailableException){}Thread.Sleep(100);}
   if(!exposed)throw new InvalidOperationException("Scaling control did not reappear after restore.");
   if(Current==percent){WindowDpi=GetDpiForWindow(window);return;}
   object sc;if(combo.TryGetCurrentPattern(ScrollItemPattern.Pattern,out sc))((ScrollItemPattern)sc).ScrollIntoView();
   object p;if(!combo.TryGetCurrentPattern(ExpandCollapsePattern.Pattern,out p))throw new InvalidOperationException("Scale combo cannot expand.");
   ((ExpandCollapsePattern)p).Expand();AutomationElement chosen=null;
   var until=DateTime.UtcNow.AddSeconds(10);
   while(DateTime.UtcNow<until&&chosen==null){
    var items=AutomationElement.RootElement.FindAll(TreeScope.Descendants,Condition.TrueCondition);
    foreach(AutomationElement item in items)if(Percent(item.Current.Name)==percent && item.TryGetCurrentPattern(SelectionItemPattern.Pattern,out p)){chosen=item;break;}
    if(chosen==null)Thread.Sleep(100);
   }
   if(chosen==null)throw new InvalidOperationException("Requested scale is absent.");
   object scroll;if(chosen.TryGetCurrentPattern(ScrollItemPattern.Pattern,out scroll))((ScrollItemPattern)scroll).ScrollIntoView();
   if(!chosen.TryGetCurrentPattern(SelectionItemPattern.Pattern,out p))throw new InvalidOperationException("Scale item cannot select.");
   ((SelectionItemPattern)p).Select();
   until=DateTime.UtcNow.AddSeconds(20);
   while(DateTime.UtcNow<until){try{if(Current==percent){WindowDpi=GetDpiForWindow(window);if(WindowDpi==percent*96/100)return;}}catch(ElementNotAvailableException){}Thread.Sleep(100);}
   throw new InvalidOperationException("Actual Settings scale/DPI did not reach requested value.");
  }
  public void Dispose() {
   try {if(combo!=null&&OriginalPercent!=0)Set(OriginalPercent);}finally{if(window!=IntPtr.Zero){if(!PostMessage(window,0x10,IntPtr.Zero,IntPtr.Zero))throw new Win32Exception();window=IntPtr.Zero;}}
  }
 }
}

namespace FileSpacerRegression {
 public sealed class Resolution : IDisposable {
  [StructLayout(LayoutKind.Explicit,Size=220)] struct Mode {
   [FieldOffset(68)] public ushort Size;
   [FieldOffset(72)] public uint Fields;
   [FieldOffset(172)] public uint Width;
   [FieldOffset(176)] public uint Height;
  }
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool EnumDisplaySettings(string device,int number,ref Mode mode);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int ChangeDisplaySettingsEx(string device,ref Mode mode,IntPtr window,uint flags,IntPtr parameter);
  Mode original;bool changed;public uint Width,Height;
  static Mode Current() {var m=new Mode{Size=220};if(!EnumDisplaySettings(null,-1,ref m))throw new InvalidOperationException("Cannot read current resolution.");return m;}
  public static uint[] ReadSize() {var m=Current();return new uint[]{m.Width,m.Height};}
  public Resolution(uint width,uint height) {
   original=Current();Width=original.Width;Height=original.Height;
   if(Width==width&&Height==height)return;
   var wanted=original;wanted.Fields=0x180000;wanted.Width=width;wanted.Height=height;
   int result=ChangeDisplaySettingsEx(null,ref wanted,IntPtr.Zero,2,IntPtr.Zero);if(result!=0)throw new InvalidOperationException("Resolution test refused: "+result);
   result=ChangeDisplaySettingsEx(null,ref wanted,IntPtr.Zero,0,IntPtr.Zero);if(result!=0)throw new InvalidOperationException("Resolution change refused: "+result);
   changed=true;var actual=Current();if(actual.Width!=width||actual.Height!=height)throw new InvalidOperationException("Resolution verification failed.");Width=actual.Width;Height=actual.Height;
  }
  public void Dispose() {
   if(!changed)return;var wanted=original;wanted.Fields=0x180000;
   int result=ChangeDisplaySettingsEx(null,ref wanted,IntPtr.Zero,0,IntPtr.Zero);if(result!=0)throw new InvalidOperationException("Resolution restore refused: "+result);
   var actual=Current();if(actual.Width!=original.Width||actual.Height!=original.Height)throw new InvalidOperationException("Resolution restore verification failed.");changed=false;Width=actual.Width;Height=actual.Height;
  }
 }
}

namespace FileSpacerRegression {
 public static class DisplayState {
  [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
  [DllImport("user32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr CreateWindowEx(uint ex,string type,string text,uint style,int x,int y,int width,int height,IntPtr parent,IntPtr menu,IntPtr instance,IntPtr parameter);
  [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr window);
  [DllImport("user32.dll")] static extern bool DestroyWindow(IntPtr window);
  public static uint ReadMonitorDpi() {
   var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));if(previous==IntPtr.Zero)throw new Win32Exception();
   try {
    var window=CreateWindowEx(0x80,"STATIC","FileSpacer test DPI observation",0x80000000,0,0,1,1,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero);
    if(window==IntPtr.Zero)throw new Win32Exception();
    try{uint dpi=GetDpiForWindow(window);if(dpi==0)throw new InvalidOperationException("No actual monitor DPI.");return dpi;}finally{DestroyWindow(window);}
   }finally{SetThreadDpiAwarenessContext(previous);}
  }
 }
}
