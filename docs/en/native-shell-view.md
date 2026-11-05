# Native Shell view architecture

Read this note before changing view hosting, saved-state restoration or the
boundary between Shell items and spatial windows. For a broader source map,
see [Working on FileSpacer](README.md).

## Responsibilities

`ShellViewHost` embeds `CLSID_ExplorerBrowser`. Windows owns enumeration,
rendering, icons, view modes and file operations. `FolderWindow` owns spatial
navigation policy and coordinates persistence. The production options are
`FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`, without forcing `FVO_VISTALAYOUT`;
virtual folders can choose their initial presentation. `EBO_NOPERSISTVIEWSTATE`
keeps per-folder persistence under FileSpacer's control.

| Component | Responsibility |
| --- | --- |
| `ShellViewHost` | Browser lifetime, navigation, view access and public event observation. |
| `ShellViewSettings` | Capture/apply mode, icon size, flags, columns, sorting and grouping. |
| `ShellViewSelection` | Selection, focused item and one-shot reopen/scroll snapshot. |
| `ShellViewIcons` | Native item identities, free icon positions and the batch redraw guard. |
| `ShellViewFilter` | Name filtering through `IFolderFilter`. |
| `FolderStateStore` | Independently present state parts, transactions, locking and retention. |

View operations use public COM interfaces. Exactly two narrow exceptions touch
Shell-owned windows; both are optional implementation dependencies that fall
back to native behavior when a prerequisite is missing:

- Selected-label background (`ShellViewLabels.h`, described below): subclasses
  only the public view window returned by `IShellView::GetWindow`, reads
  `NM_CUSTOMDRAW` notifications and documented ListView state. It discovers no
  child control and sends nothing to one.
- Icon redraw guard (`IconRedrawGuard` in `ShellViewIcons.cpp`): around one
  `SelectAndPositionItems` batch of saved free positions, it withholds the
  Shell's internal `WM_SETREDRAW(TRUE)` resumes so that
  `IFolderView2::SetRedraw(FALSE)` stays effective for the whole batch.
  Discovery is limited to a single `WC_LISTVIEW` child of the
  `IShellView::GetWindow` window, on the same thread and in the same process.
  The view and the control must be visible, redraw must not already be
  disabled (`SysSetRedraw` property) and no earlier guard may be installed;
  otherwise the batch runs under `SetRedraw` alone. The guard never changes
  the control's state: `SetRedraw(FALSE)` removes `WS_VISIBLE` (the documented
  `WM_SETREDRAW` effect), `SetRedraw(TRUE)` restores it, and only then does the
  guard request the withheld repaint on the public view window
  (`RedrawWindow` with `RDW_ALLCHILDREN`, as the `WM_SETREDRAW` contract
  prescribes). If `SetRedraw(TRUE)` fails, the subclass is removed and nothing
  is sent to the control. Measured reason: restoring 5,000 positions took
  about 3.7 s with the guard against about 162 s under the public interface
  alone, whose suspension the Shell resumed inside the batch. Group 18 covers
  this restoration.

Do not generalize either exception to other view operations, which remain COM
calls. Tests can inspect controls to observe a gesture; that instrumentation is
not an application dependency.

## Readiness, restoration and lifetime

Navigation completion, view creation and completed enumeration are distinct
events. `OnViewCreated` does not mean the contents are ready. Observe public
signals; neither elapsed time nor an item count proves readiness.

Notifications carry a view observation generation. Invalidate it before
unsubscribing or destroying a view, and ignore queued messages from an earlier
generation. Consuming a one-shot restoration must happen before reentrant COM
calls. If applying view settings causes re-enumeration, wait for a subsequent
enumeration-complete signal before restoring positions/selection or saving.

Shell calls, menus and refresh can pump messages. Hold the necessary COM
references across these calls, but also recheck whether the owner window and
view still exist. A reference protects the C++ object, not its `HWND` or browser
resources. Route refresh and view verbs through `FolderWindow::refreshView()`
and `invokeViewCommand()` so navigation-depth guards defer destruction safely.
A stale continuation must not restore a view or schedule another state save.

Geometry, view settings and icon positions are separate snapshot parts. Capture
failure must leave the known part intact. In particular, filtered position
capture returns `S_FALSE`; persist positions only on `S_OK`. Empty, filtered
or incompletely enumerated contents are not a complete saved icon layout.
Transient View-menu previews must also stay out of durable state.

Selection and focused item are restored independently, once, without stealing
keyboard focus. To clear selection, `SelectItem(nullptr, SVSI_DESELECTOTHERS)`
must not include `SVSI_NOTAKEFOCUS`; that flag belongs on a targeted item.
Navigating upward does not automatically select the source folder or replace
the selection of an already-open parent.

## Mixed file/folder sorting

When mixed sorting is enabled for a filesystem folder, `MixedFolder` supplies a
Shell junction rooted in FileSpacer's local data, with a COM registration
written by the installer through `regsvr32` (`DllRegisterServer`, HKLM, for all
users; `/n /i:user` reaching `DllInstall`, HKCU, for the current user, whatever
the installer's elevation) or, once per process, by the application in HKCU when the effective
registration names neither the DLL beside the executable nor a copy of the
same file version. `FileSpacerFolder.dll` implements `IShellFolder2` and
`IPersistFolder2` and creates the native view through `SHCreateShellFolderView`.
`IExplorerBrowser` remains the host. Virtual folders and archives retain their
Windows provider. The application and DLL must be deployed together.

Only the provider root has its own junction identity. Children keep the native
folder's relative PIDLs unchanged. Enumeration, metadata, icons, context menus,
transfer objects and file actions delegate to the underlying folder. The
provider compares column values without the usual folder-first priority;
identity comparisons remain native. Free-position mode keeps this provider
active and preserves icon positions without imposing automatic arrangement.

The private `IMixedFolder::NativeItem` contract crosses FileSpacer's DLL boundary.
Use `nativeShellItem()` / `nativeViewSelection()` when an operation needs the
real filesystem item outside the provider. SQLite keeps the native child
identifiers, allowing the same selection and positions across preference
changes. Do not wrap or reinterpret child PIDLs. The native view subscribes to
the real root's changes through `SFVM_GETNOTIFY`, rather than relaying duplicate
notifications from the junction.

Registration and the junction are isolated in `/test` mode. Test cases must
remove only their own registration and directory. Normal uninstall removes
the registration owned by FileSpacer.

## Selected-name background and scrolling

`ShellViewLabels.h` optionally fills an opaque background behind the single
selected item's label in Icons mode. Windows still draws and expands the text.
Multiple selection, focus-only items and other modes retain native rendering.

The adaptation subclasses only the public view window returned by
`IShellView::GetWindow`, on its own thread. It reads `NM_CUSTOMDRAW` from a
verified ListView belonging to that view and uses documented ListView reads.
Check actual selection rather than relying solely on the drawing notification's
`CDIS_SELECTED`. Preserve `DefSubclassProc` and its result; do not add
`CDRF_SKIPDEFAULT`. The callback holds no FileSpacer pointer and removes itself
at `WM_NCDESTROY`.

This notification path is an implementation-dependent rendering adaptation,
not a general Shell contract for label expansion. If its prerequisites or
notifications disappear, leave the standard drawing intact. Do not generalize
it into a custom multi-label renderer or use it for view operations.

Reopening Details/List views restores a first-visible-row anchor, not an exact
pixel offset. `ShellViewSelection` finds it from `IFolderView::Items` and item
positions inside the view's client rectangle, below the accessible column
header. Restoration ensures the last item, then the anchor, is visible under
redraw suppression, without taking focus. Icon views use unscrolled positions
and reopen at the top. Do not treat `IFolderView2::GetVisibleItem` as a viewport
query, or assume `GetViewStateStream` contains a scroll offset.

## Activation and external folders

With selection-preserving activation enabled, a plain left click on the
background of an inactive view activates it without clearing selection. The
message loop uses `WM_ACTIVATE` / `WA_CLICKACTIVE` and the actual Shell-view hit
target before discarding that first background button-down. A title-bar click
must not arm suppression of a later client click. Item clicks, right clicks,
headers, scrollbars and active-window clicks retain their normal handling.
`WM_MOUSEACTIVATE` on the outer frame is not a reliable hook through Shell-owned
windows.

Control Panel opens externally through `IOpenControlPanel::Open`. Compare Shell
identities, not translated names or decoded PIDLs. Keep the source folder and
its selection intact.

## Tests and references

The [regression guide](tests/regression/README.md) maps behavior to groups.
Start with group 18 for real Shell/SQLite persistence, 19 for commands, 21 for
activation, 23 for selected-name rendering, 28 for transient View previews, and
`30-tri-melange` for provider boundaries. Injected-failure/lifetime cases cover
paths that a successful real-view test cannot establish.

- [IExplorerBrowser](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorerbrowser)
- [IFolderView2](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-ifolderview2)
- [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions)
- [Native folder views](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreateshellfolderview)
- [Namespace junctions](https://learn.microsoft.com/en-us/windows/win32/shell/nse-junction)
- [SFVM_GETNOTIFY](https://learn.microsoft.com/en-us/windows/win32/shell/sfvm-getnotify)
- [IOpenControlPanel::Open](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iopencontrolpanel-open)
