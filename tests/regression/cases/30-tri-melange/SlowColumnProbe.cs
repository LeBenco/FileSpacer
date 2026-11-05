using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

// Read-only observation of the native folder's column states (IShellFolder2),
// to know which columns Windows itself marks SLOW / PREFER_VARCMP / PREFER_FMTCMP.
namespace FileSpacerRegression {
    [ComImport, Guid("93F2F68C-1D1B-11d3-A30E-00C04F79ABD1"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface ShellFolder2 {
        [PreserveSig] int ParseDisplayName(IntPtr hwnd,IntPtr context,[MarshalAs(UnmanagedType.LPWStr)] string name,IntPtr eaten,out IntPtr pidl,IntPtr attributes);
        [PreserveSig] int EnumObjects(IntPtr hwnd,uint flags,out IntPtr enumerator);
        [PreserveSig] int BindToObject(IntPtr pidl,IntPtr context,ref Guid iid,out IntPtr result);
        [PreserveSig] int BindToStorage(IntPtr pidl,IntPtr context,ref Guid iid,out IntPtr result);
        [PreserveSig] int CompareIDs(IntPtr flags,IntPtr left,IntPtr right);
        [PreserveSig] int CreateViewObject(IntPtr hwnd,ref Guid iid,out IntPtr result);
        [PreserveSig] int GetAttributesOf(uint count,IntPtr pidls,ref uint attributes);
        [PreserveSig] int GetUIObjectOf(IntPtr hwnd,uint count,IntPtr pidls,ref Guid iid,IntPtr reserved,out IntPtr result);
        [PreserveSig] int GetDisplayNameOf(IntPtr pidl,uint flags,IntPtr name);
        [PreserveSig] int SetNameOf(IntPtr hwnd,IntPtr pidl,[MarshalAs(UnmanagedType.LPWStr)] string name,uint flags,out IntPtr result);
        [PreserveSig] int GetDefaultSearchGUID(out Guid guid);
        [PreserveSig] int EnumSearches(out IntPtr enumerator);
        [PreserveSig] int GetDefaultColumn(uint reserved,out uint sort,out uint display);
        [PreserveSig] int GetDefaultColumnState(uint column,out uint state);
        [PreserveSig] int GetDetailsEx(IntPtr pidl,IntPtr key,IntPtr value);
        [PreserveSig] int GetDetailsOf(IntPtr pidl,uint column,IntPtr details);
        [PreserveSig] int MapColumnToSCID(uint column,IntPtr key);
    }
    public sealed class NativeColumnState {
        public uint Index; public string Title; public Guid Format; public uint Id; public uint State;
        // Values from the SDK's shtypes.h (SHCOLSTATE enumeration).
        public bool Slow { get { return (State & 0x20)!=0; } }            // SHCOLSTATE_SLOW
        public bool PreferVarCmp { get { return (State & 0x200)!=0; } }   // SHCOLSTATE_PREFER_VARCMP
        public bool PreferFmtCmp { get { return (State & 0x400)!=0; } }   // SHCOLSTATE_PREFER_FMTCMP
        public bool NoSortByFolderness { get { return (State & 0x800)!=0; } } // SHCOLSTATE_NOSORTBYFOLDERNESS
        public override string ToString() {
            return Index+"|"+Title+"|"+Format.ToString("D")+":"+Id+"|0x"+State.ToString("X")+
                "|slow="+(Slow?1:0)+"|varcmp="+(PreferVarCmp?1:0)+"|fmtcmp="+(PreferFmtCmp?1:0)+"|nofolderness="+(NoSortByFolderness?1:0);
        }
    }
    public static class SlowColumnProbe {
        [DllImport("shell32.dll",CharSet=CharSet.Unicode)] static extern int SHParseDisplayName(string name,IntPtr context,out IntPtr pidl,uint attributesIn,out uint attributesOut);
        [DllImport("shell32.dll")] static extern int SHBindToObject(IntPtr folder,IntPtr pidl,IntPtr context,ref Guid iid,out IntPtr result);
        [DllImport("shlwapi.dll",CharSet=CharSet.Unicode)] static extern int StrRetToStrW(IntPtr strret,IntPtr pidl,out IntPtr text);
        static void Check(int hr) { if(hr<0) Marshal.ThrowExceptionForHR(hr); }
        // Every column the native filesystem folder exposes, with its default state.
        public static NativeColumnState[] ColumnStates(string folder,uint limit) {
            IntPtr pidl=IntPtr.Zero,pointer=IntPtr.Zero; object instance=null;
            var states=new List<NativeColumnState>();
            try {
                uint attributes; Check(SHParseDisplayName(folder,IntPtr.Zero,out pidl,0,out attributes));
                Guid iid=typeof(ShellFolder2).GUID; Check(SHBindToObject(IntPtr.Zero,pidl,IntPtr.Zero,ref iid,out pointer));
                instance=Marshal.GetObjectForIUnknown(pointer); var shellFolder=(ShellFolder2)instance;
                IntPtr details=Marshal.AllocHGlobal(512),key=Marshal.AllocHGlobal(32);
                try {
                    for(uint column=0;column<limit;++column) {
                        for(int i=0;i<512;++i) Marshal.WriteByte(details,i,0);
                        int hr=shellFolder.GetDetailsOf(IntPtr.Zero,column,details);
                        if(hr<0) break; // E_FAIL/E_INVALIDARG past the last column.
                        IntPtr text; string title="";
                        if(StrRetToStrW(IntPtr.Add(details,8),IntPtr.Zero,out text)>=0 && text!=IntPtr.Zero) {
                            try { title=Marshal.PtrToStringUni(text); } finally { Marshal.FreeCoTaskMem(text); }
                        }
                        var state=new NativeColumnState{Index=column,Title=title};
                        if(shellFolder.MapColumnToSCID(column,key)>=0) {
                            state.Format=(Guid)Marshal.PtrToStructure(key,typeof(Guid)); state.Id=(uint)Marshal.ReadInt32(key,16);
                        }
                        uint flags; if(shellFolder.GetDefaultColumnState(column,out flags)>=0) state.State=flags;
                        states.Add(state);
                    }
                } finally { Marshal.FreeHGlobal(details); Marshal.FreeHGlobal(key); }
            } finally {
                if(instance!=null) Marshal.ReleaseComObject(instance);
                if(pointer!=IntPtr.Zero) Marshal.Release(pointer);
                if(pidl!=IntPtr.Zero) Marshal.FreeCoTaskMem(pidl);
            }
            return states.ToArray();
        }
        // Default states reported by an arbitrary folder interface (for example the
        // provider folder shown by the view, through its cross-process proxy).
        public static NativeColumnState[] FolderColumnStates(IntPtr folderPointer,uint[] columns) {
            object instance=null; var states=new List<NativeColumnState>();
            try {
                instance=Marshal.GetObjectForIUnknown(folderPointer); var shellFolder=(ShellFolder2)instance;
                IntPtr key=Marshal.AllocHGlobal(32);
                try {
                    foreach(uint column in columns) {
                        var state=new NativeColumnState{Index=column,Title=""};
                        if(shellFolder.MapColumnToSCID(column,key)>=0) {
                            state.Format=(Guid)Marshal.PtrToStructure(key,typeof(Guid)); state.Id=(uint)Marshal.ReadInt32(key,16);
                        }
                        uint flags; Check(shellFolder.GetDefaultColumnState(column,out flags)); state.State=flags;
                        states.Add(state);
                    }
                } finally { Marshal.FreeHGlobal(key); }
            } finally {
                if(instance!=null) Marshal.ReleaseComObject(instance);
                if(folderPointer!=IntPtr.Zero) Marshal.Release(folderPointer);
            }
            return states.ToArray();
        }
    }
}
