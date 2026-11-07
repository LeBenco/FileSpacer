# Native Shell view architecture

Read this note before changing view hosting, saved-state restoration or the
boundary between Shell items and spatial windows. For a broader source map,
see [Working on FileSpacer](README.md).

## Responsibilities

`ShellViewHost` embeds `CLSID_ExplorerBrowser`. Windows owns enumeration,
rendering, icons, view modes and file operations. `FolderWindow` owns spatial
navigation policy and coordinates persistence. Eligible filesystem folders use
`FVO_DEFAULT` for Details and List, and
`FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING` for spatial presentations. Virtual
folders can choose their initial presentation. `EBO_NOPERSISTVIEWSTATE`
keeps per-folder persistence under FileSpacer's control.

| Component | Responsibility |
| --- | --- |
| `ShellViewHost` | Browser lifetime, navigation, view access and public event observation. |
| `ShellViewSettings` | Capture/apply mode, icon size, flags, columns, sorting and grouping. |
| `ShellViewSelection` | Selection, focused item and one-shot reopen/scroll snapshot. |
| `ShellViewIcons` | Native item identities, free icon positions and the batch redraw guard. |
| `ShellViewFilter` | Name filtering through `IFolderFilter`. |
| `FolderStateStore` | Independently present state parts, transactions, locking and retention. |

View operations use public COM interfaces, with three optional adaptations
that fall back to native behavior when a prerequisite is missing:

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
- Qualified selection capabilities (`ShellViewSelection`, described below):
  optional private selection-cache access and a qualified native callback for
  global Select All. Their absence retains public Shell selection operations.

The additional optional viewport adapter below is limited to recording and
restoring scrolling. Other view operations remain COM calls. Tests can inspect
controls to observe a gesture; that instrumentation is not an application dependency.

## Mouse gestures across Shell renderers

The shared MSAA hit test recognizes both renderer families. Read-only text
cells and passive List labels are resolved to their logical item; native
editing controls, headers and scrollbars remain excluded from window gestures.
UI Automation locates test targets from a separate worker, not from the
application's GUI thread.

Classic controls supply `WM_LBUTTONDBLCLK`. A renderer without `CS_DBLCLKS`
recognizes double clicks itself, but a consumed first window-gesture press
prevents that recognition. FileSpacer recognizes the second press using the
Windows time and position limits, then requests native Properties through
`IFolderView2::InvokeVerbOnSelection`. It does not replay mouse presses.
The preparation preserves the renderer's focus/selection-mark behavior and
Ctrl selection; Shift ranges use public folder-view selection because the
modern MSAA provider rejects extended selection. Groups 15, 16 and 24 exercise
the actual gestures and compare Properties, selection, focus and selection mark
against the same Shell view with FileSpacer gestures disabled.

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

For changed selections of at least 128 items, a saved complete selection can
use `IShellFolderView::Select(SFVS_SELECT_ALLITEMS)`. An exact bijection of opaque
child PIDLs must match the current view first, and the selected identities are
checked again after the call. Duplicate, missing or differently encoded identities,
unsupported interfaces and failed operations retain the per-item restoration.
View-generation checks still apply after every external call. The qualified
Select All binding is reused when available; otherwise the public bulk operation
also serves free placement when the arrangement query is supported. In free icon
views, complete-set proofs use public item indices and recheck all/selected
counts, avoiding the native spatial sort while still comparing every identity.
Other renderers retain their efficient native enumeration. Capturing a large
complete selection also uses indices in free icon views, with native enumeration
when the indexed capture is unavailable. Focus and scroll restoration remain
separate.

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

Selection snapshots now embed the shared viewport described below (format 3),
before the optional focus and selected PIDLs. This avoids spatial enumeration
for an anchor and the last-item/anchor scroll detour. Both old formats remain
readable. If the viewport adapter is unavailable, capture retains the legacy
anchor/percentage protocol. Its classic anchor comes from `IFolderView::Items`
and positions inside the client rectangle, below the accessible header; it is
restored by revealing the last item, then the anchor under redraw suppression.
This legacy fallback does not promise an exact pixel offset. Do not treat
`IFolderView2::GetVisibleItem` as a viewport query, or assume `GetViewStateStream`
contains a scroll offset.



## Optional selection capabilities

`ShellViewSelection` starts with an immediately usable public clear path.
`ShellSelectionDiscovery` asynchronously reads the capability cache; it never
waits for symbols or filesystem I/O on the UI thread. A qualified private path
can be published once in the current process. Each operation retains its
binding, so publication cannot change an operation already in progress.
Existing folder windows receive a posted notification and bind the new
capability on their UI thread; navigation/closure and view-generation guards
still apply. A rejected binding or failed safety check retains the public path.

The cache under `%LOCALAPPDATA%\FileSpacer\ShellSelection-v1` contains relative
addresses/member offsets, not absolute pointers. Its key includes the loaded
Shell32 PDB GUID, age, PE image size and qualification-schema revision. Every
process validates cached data against its loaded system image, recognized
operands and CFG targets. Discovery
only runs when no usable decision exists for this image/schema. A saved
unsupported decision avoids repeated discovery; transient failures back off for
24 hours before a later launch may retry.

An isolated native worker obtains the matching Microsoft PDB through HTTPS and
uses system DbgHelp and bounded instruction recognition. No development tools,
prepared build profiles, executable-memory patches or CFG exemptions are used.
The worker qualifies actual invisible Shell views with 256 isolated files in all
nine configurations, including selection caches, identities, retained snapshots,
focus, follow-up actions and refresh. Only a successful qualification atomically
publishes the cache. Concurrent processes share a single producer and receive
cache-publication notifications without polling the UI. A watchdog terminates
only the disposable worker after three minutes.

This optimization uses private COM interfaces, a native entry callback and an
undocumented selection-array count write. Public native clear performs its own
allocation cleanup; FileSpacer does not free another component's allocation.
Unknown signatures, unsupported CFG policies and elevated processes use public
operations. CFG does not establish a public ABI contract: do not bypass a failed
validation or enable an unqualified image. Tests of foreign DLL bytes do not
establish runtime compatibility with their Windows version.

The host regression case `18-vue-shell-native/04-SelectionCache.ps1` exercises
the production adapter, asynchronous publication, persistent positive/negative
decisions, transient failure backoff, concurrent discovery and the 405-row real
Shell matrix with 5000 files. Its views are invisible; this is not presentation
latency or a full Explorer benchmark. Ordinary `/test` launches do not read or
populate the user's capability cache.

The optional Select All capability uses the same discovery, cache and binding
lifecycle. It observes global selection notifications on the public Shell-view
window and validates the registered native callback, control identity and native
notification guard. Windows still constructs every selected-item entry and its
subscription. FileSpacer stages those opaque entries while keeping the native
search list empty, then publishes the complete array and invalidates selection
snapshots. This avoids the classic engine's quadratic duplicate searches without
calling unqualified private item constructors. A partial previous selection is
released through the qualified native cleanup callback before reconstruction.

Temporary staging costs 16 bytes per item (80,000 bytes for 5,000 items). The
native allocator owns the final array. Missing symbols, unknown instruction
forms or an unsupported binding retain ordinary Shell selection silently; they
do not disable an independently qualified clear capability. This is an optional
private protocol, not a guaranteed Windows interface. Arbitrary subset selection
continues to use the native per-item path.

Cases `05-SelectionAll.ps1` and `06-SelectionAllVisible.ps1` exercise the production
adapter from empty and partial selections in nine configurations, plus a forced
public selection fallback. They verify identities, retained snapshots, selection
data objects, sorting, focus and subsequent operations. Case 06 uses visible
800-by-400 views and checks sampled icon positions. API-return timings do not
establish composed-frame presentation latency or full-application parity with
Explorer.

## Persistent viewport

Ordinary folder opens restore a separate 48-byte viewport snapshot, after the
saved presentation and free positions are applied. This is independent of the
closed-window history and does not select files or restore keyboard focus.
The snapshot is consumed once for the window; refresh and renderer recreation
do not replay it. An explicit closed-window or transient selection snapshot
takes precedence. Unreadable/provisional state and filtered views never replace
the saved viewport. A closing View-menu preview retains its original viewport.

Modern views use the existing in-process public `IScrollProvider`. Classic
views optionally use a unique, visible `SysListView32` in the Shell view's own
process and UI thread, with matching view type and item count. The documented
`LVM_GETITEMRECT` and `LVM_SCROLL` messages save and restore the first item's
client coordinates. One pixel displacement runs under `IFolderView2::SetRedraw`
to avoid native scrolling animation; drawing resumes on cancellation too.
There is no per-file traversal. DPI is recorded, and incompatible presentation
or unavailable capabilities leave the native viewport unchanged.

Recognizing that child control is an optional dependency on Shell composition,
not a guaranteed Shell COM contract. No additional private Windows function,
offset or executable patch is introduced. With changed folder contents, layout
or DPI, restoration remains best effort rather than preserving a semantic item.

Database schema 7 adds a nullable `scroll` column, preserving existing view and
icon blobs. Closing updates an existing folder only; viewport saving ignores
the layout lock because scrolling is navigation. Rekeying carries the snapshot;
view reset, deletion, expiry and quota eviction discard it with the folder state.
Tests `07-fenetres/08-DefilementPersistant` and the independent real-Shell
`ScrollState` experiment cover this behavior and its incremental cost.

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
- [IFolderView2::InvokeVerbOnSelection](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifolderview2-invokeverbonselection)
- [MSAA selection flags](https://learn.microsoft.com/en-us/windows/win32/winauto/selflag)
- [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions)
- [Native folder views](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreateshellfolderview)
- [Namespace junctions](https://learn.microsoft.com/en-us/windows/win32/shell/nse-junction)
- [SFVM_GETNOTIFY](https://learn.microsoft.com/en-us/windows/win32/shell/sfvm-getnotify)
- [IOpenControlPanel::Open](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-iopencontrolpanel-open)
