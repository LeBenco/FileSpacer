<a id="native-shell-view-architecture"></a>

# Native Shell view architecture

Read this note before changing view hosting, saved-state restoration or the
boundary between Shell items and spatial windows. For a broader source map,
see [Working on FileSpacer](README.md).

<a id="responsibilities"></a>

## Responsibilities

`ShellViewHost` embeds `CLSID_ExplorerBrowser`. Windows owns enumeration,
rendering, icons, view modes and file operations. `FolderWindow` owns spatial
navigation policy and coordinates persistence. Eligible filesystem folders use
`FVO_DEFAULT` for List, Tiles and Content, and for Details without automatic column
filling. Spatial presentations and Details with column filling use
`FVO_CUSTOMPOSITION | FVO_CUSTOMORDERING`. Tiles and Content never enable spatial
placement, one-shot realignment or S-drag Snap. Virtual
folders can choose their initial presentation. `EBO_NOPERSISTVIEWSTATE`
keeps per-folder persistence under FileSpacer's control.

For ordinary filesystem folders, FileSpacer builds its View popup locally from
the eight supported modes and placement settings. It uses the public
`IFolderView2` methods for commands, without querying the folder's background
context menu or recreating the view before opening the popup. Nilesoft uses the
same registered COM activation and standard Win32 tracking path as local title
menus; the successful activation is retained for the folder window. The local
popup requests no animation and performs no SQLite write on opening. Its initial
view, selection, scroll and icon snapshot is captured in memory only when a mode
is first previewed or chosen. Original settings and coordinates remain available
for a close during preview; actual changes and closure drive persistence. A
changed trusted icon snapshot is flushed when a mode choice is validated,
before a nonspatial view can prevent that snapshot from being captured again. Other
providers retain their native submenu and an in-memory capture. Neither path
flushes SQLite before opening the menu. Disabled hover preview still captures
and commits chosen commands safely, without changing the view on hover.
Unchanged placement choices do not arm a save timer. Lock-state reads are
needed only when locking is enabled and the folder identity is valid. Content is committed
after closing the menu because its full-width renderer requires a profile change.

| Component | Responsibility |
| --- | --- |
| `ShellViewHost` | Browser lifetime, navigation, view access and public event observation. |
| `ShellViewSettings` | Capture/apply mode, icon size, flags, columns, sorting and grouping. |
| `ShellViewSelection` | Selection, focused item and one-shot reopen/scroll snapshot. |
| `ShellViewOperations` | Opaque selection bindings, complete-set selection, grid alignment and public/private dispatch. |
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
  `SelectAndPositionItems` batch of free positions, restored from the saved
  state or written by a drop, with or without Snap, it withholds the
  Shell's internal `WM_SETREDRAW(TRUE)` resumes so that
  `IFolderView2::SetRedraw(FALSE)` stays effective for the whole batch.
  Discovery is limited to a single `WC_LISTVIEW` child of the
  `IShellView::GetWindow` window, on the same thread and in the same process.
  The view and the control must be visible, with no earlier guard installed;
  otherwise the batch runs without the guard and without suspending redraw.
  The guard never changes
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

<a id="mouse-gestures-across-shell-renderers"></a>

## Mouse gestures across Shell renderers

Ungrouped classic ListViews use their synchronous public hit test before any
accessibility RPC can dispatch queued input. Other renderers retain the shared
MSAA hit test. Read-only text cells and passive List labels resolve to their
logical item; native editing controls, headers and scrollbars remain excluded
from window gestures.
UI Automation locates test targets from a separate worker, not from the
application's GUI thread.

Classic controls supply `WM_LBUTTONDBLCLK`; the pointed native index is validated
against the current view and selected through `IFolderView`, rather than resolved
from the previous selection. `DragDetect` receives the actual input window;
left-button movement still enters Windows' native caption-move loop.
A renderer without `CS_DBLCLKS`
recognizes double clicks itself, but a consumed first window-gesture press
prevents that recognition. FileSpacer recognizes the second press using the
Windows time and position limits, then requests native Properties through
`IFolderView2::InvokeVerbOnSelection`. It does not replay mouse presses.
The preparation preserves the renderer's focus/selection-mark behavior and
Ctrl selection; Shift ranges use public folder-view selection because the
modern MSAA provider rejects extended selection. Groups 15, 16 and 24 exercise
the actual gestures and compare Properties, selection, focus and selection mark
against the same Shell view with FileSpacer gestures disabled.

<a id="readiness-restoration-and-lifetime"></a>

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

<a id="mixed-filefolder-sorting"></a>

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

<a id="selected-name-background-and-scrolling"></a>

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



<a id="optional-selection-capabilities"></a>

## Optional selection capabilities

Snap uses public Windows APIs for both prediction and placement. The optional
private grid arithmetic was retired after Release measurements showed no useful
benefit over the public path; slow bulk placement on Windows 10 occurred in both
paths. Commit `d93f4dee` is the last complete implementation of that capability.
The separate optional selection and range capabilities are unchanged.

`ShellIconGrid` prepares one operation per Icon-view gesture. A hidden,
single-item Windows ListView uses the actual view's image list, font, spacing,
control module and DPI. The image list is shared with `LVS_SHAREIMAGELISTS` and
retained through its public COM interface; the font is copied for the gesture's
lifetime. No icon is rendered. Public `LVM_SETITEMPOSITION32`,
`LVM_ARRANGE(LVA_SNAPTOGRID)` and `LVM_GETITEMPOSITION` provide native rounding on
this one item. No folder-wide alignment, position snapshot or restoration is
needed to prepare it, and preparation does not grow with the folder's item
count. Geometry is recreated per gesture to reflect zoom and DPI changes.

View presets and wheel steps pass the same nominal icon sizes as Explorer to
`SetViewModeAndIconSize`; Windows chooses the physical image dimensions. Those
dimensions can differ from `GetViewModeAndIconSize`, including nonlinear size
steps above 96 pixels. The public grid shares the actual square image list and
checks that it fits the native pitch. Free-placement image hit targets and the
Snap frame also use the actual image dimensions. Previously stored raw icon
sizes remain unchanged until a new view or zoom choice replaces them.

Only selected destinations are written. Free placement permits overlapping
destinations; other icons are never shifted to resolve collisions. Selection,
focus and persistent arrangement flags are unchanged. The separate
`alignIconsToGrid` command retains the native global operation; its selected-only
form restores stationary positions after that operation. Stress case
`28-apercu-affichage/06-Grille.ps1` checks real public-grid dispatch and compares
final coordinates with native folder alignment over 3,000 items, including
selection, focus, persistence and 60 acknowledged marker motions.

The Placement submenu exposes mutually exclusive Automatic, On a grid and Free
choices. One masked `IFolderView2::SetCurrentFolderFlags` call sets both native
arrangement flags together. List, Details, Tiles and Content do not enable this submenu.
Realignment remains a separate one-shot operation.

Grid zoom keeps occupied cells and holes rather than replaying the old pixel
coordinates: large native size jumps can collapse rows and push collisions into
long horizontal lines. `restoreIconGridPositions` reads public `GetSpacing`
before and after the change and learns the current grid phase from a native item
position of the first item in the current view, independently of whether the
first saved identity still exists. Other saved survivors can retain intermediate
coordinates after a Windows 11 size change. An empty current view needs no
remapping. It maps the saved cells to that geometry in one positioning batch,
with Snap temporarily disabled so intermediate batch collisions cannot displace
later items; the original flag is restored even if positioning fails. Both wheel
changes and committed/hovered View modes use this operation. Free placement
restores stored coordinates instead of remapping cells. Positions are normalized
to integer 96-DPI units, so a restore at a fractional scale can round a physical
pixel. Native size changes suppress state capture
until the intermediate layout has finished; unchanged menu opening adds no
spacing query or database access.

The drag preview runs throughout native automatic/grid placement and only while
S is held in free placement. For Automatic, `ListView_GetInsertMark` supplies the
Shell's actual insertion decision; cached spatial ranks and selected-prefix counts
locate the dragged anchor after the selected items are removed and reinserted.
The icon rectangle offset comes from the native view. No insertion hit-test,
private function or Shell item enumeration is added on mouse movement. A stale
layout or missing native insertion mark suppresses the frame. Native insertion
feedback and drag/drop behavior are preserved. Grid placement rounds the drag
delta using the current grid spacing and phase. Small icons also use
this permanent-placement preview; free S-drag prediction remains Icon-view only.

Free-placement drag/drop distinguishes the native image rectangle from the
surrounding item cell and label. A release on a target image retains the native
file/folder drop; a release in the surrounding space repositions icons. S reserves
the gesture for icon placement even over a folder, permits overlap, and alone
shows the destination frame. Releasing and repressing S updates the frame without
requiring mouse motion. Ctrl/Alt gestures without S retain their native effects.

The drop-target decorator exposes `IUnknown`, `IDropTarget` and the native
`IObjectWithSite` contract when available. Debug builds log `DROP INTERFACE
masked=<IID> target=item|background` if a requested interface is refused by the
decorator but accepted by its native target. The observation does not relay an
unknown interface or change COM identity; an empty trace describes the requests
actually exercised, not every possible IID.

The provider decorates native namespace drop targets with `IIconDropPolicy`, bound
through a separate optional `IIconDropPolicySite` contract. Cached targets retrieve
the current per-view policy at DragEnter. Native target handlers receive the same
data, allowed-effect mask, key state and `IObjectWithSite` context. Negotiation
stays live while placement is reserved: returning no effect during hover can
leave the Shell's target cache rejected after S is released. The final Drop
decision refuses a reserved placement, and the gesture ends the Shell drag as
cancellation before applying icon coordinates. The callback keeps a weak gesture
reference and a separate shared final decision. Mouse release freezes that
decision before the completion message is posted; cancellation freezes a veto.
A late native callback therefore does not depend on a finalized gesture or its
window. Active callbacks reject stale generations and other threads, and the
per-view policy is detached on stop.
The original `IMixedFolder` contract remains unchanged for older provider DLLs.

The input hook snapshots native pixel coordinates once after the drag threshold.
Only native control reads occur there. On a placement release it converts the
message to Escape, ending the Shell drag before a file operation; a posted window
message then applies only the selected IDs with `SelectAndPositionItems` through
the existing position-batch helper. Selection and focus are retained. Coordinate
capture and restoration are distinct from persisted settings; transient layouts
are excluded from state capture. S outside the source view cancels the transfer.

The S preview reuses the existing grid rounding and current public view spacing;
it does not activate the global grid or reorganize stationary items. Free-preview
collision rejection is removed. The input target supplies the native list without
child discovery or subclassing. Native image rectangles are cached per gesture;
viewport changes rebuild only the visible hit-test cache. Hover and release
use this geometry rather than the last provider callback's target, so a cached
background decision cannot override a later folder-image drop. Suppressed native
drop highlighting is restored in place when S is released. Mouse motion does not
enumerate the Shell folder or extract images. The icon-sized grey frame has an
opaque outline and a light-grey interior at 50% opacity. Its layered surface is
allocated lazily when shown, not on ordinary free drags. Native automatic/grid
previews retain their own insertion/collision checks. Free S-preview eligibility
remains Icon view; Small icons retain the existing native Snap fallback.

A free-placement S press is consumed through its matching release, including
repeats after the drop. Once positioning is attempted, the message hook and drop
policy are detached and retained final decisions become neutral; only keyboard
repeat suppression may remain. Deactivation stops the session unless an accepted
placement is already queued after button release. That placement survives both
deactivation and `WM_CANCELMODE`; deactivation immediately retires the keyboard
hook and held-S state because the matching release can reach another process.
The deferred handler applies the accepted coordinates and detaches the remaining
policy and hooks. During an active gesture, only `WH_KEYBOARD` changes the held-S
state; the message hook handles mouse motion, completion and cancellation.
The later release only removes keyboard interception;
it cannot apply the drop or schedule its save twice. Cancellation and closure
remove the frame. Temporary drop-highlight changes never modify selection/focus.

`ShellViewOperations` is the operation boundary for optional private behavior.
The application and `ShellViewSelection` ask for clear, complete-set selection or
grid alignment without inspecting capability descriptors or selecting a backend.
The adapter owns public fallback, qualified binding and native notification
handling. Read-only drag rounding, capability-readiness notifications and the
isolated worker entry also pass through this boundary. Only this adapter and
`ShellSelectionDiscovery` include the private runtime header; image-specific
layouts, instruction validation, CFG checks and cache schemas stay in
`ShellSelectionRuntime.h` and discovery. Diagnostic capability observations stay
available to qualification/tests, outside production operation dispatch.

`ShellViewOperations` starts with an immediately usable public clear path.
`ShellSelectionDiscovery` asynchronously reads the capability cache; it never
waits for symbols or filesystem I/O on the UI thread. A qualified private path
can be published once in the current process. Each operation retains its
binding, so publication cannot change an operation already in progress.
Existing folder windows receive a posted notification and bind the new
capability on their UI thread; navigation/closure and view-generation guards
still apply. A rejected binding or failed safety check retains the public path.
Explicit clearing uses the private path from 128 selected items, like the other
accelerated operations; a smaller selection is cleared by the public call alone.

The cache under `%LOCALAPPDATA%\FileSpacer\ShellSelection-v1` contains relative
addresses/member offsets, not absolute pointers. Its key includes the loaded
Shell32 PDB GUID, age, PE image size and qualification-schema revision. Every
process validates cached data against its loaded system image, recognized
operands and CFG targets. Discovery
only runs when no usable decision exists for this image/schema; it then removes
the records of other images from the product cache. A saved
unsupported decision avoids repeated discovery; transient failures back off for
24 hours before a later launch may retry. The same failure three times in a
row for one image, or three attempts in a row that never finished, is saved as
an unsupported decision. A failed symbol download is never counted.

An isolated native worker obtains the matching Microsoft PDB through HTTPS and
uses system DbgHelp and bounded instruction recognition. No development tools,
prepared build profiles, executable-memory patches or CFG exemptions are used.
The worker qualifies actual invisible Shell views with 256 isolated files in all
nine configurations, including selection caches, identities, retained snapshots,
focus, follow-up actions and refresh. Only a successful qualification atomically
publishes the cache. Concurrent processes share a single producer and receive
cache-publication notifications without polling the UI. A watchdog terminates
only the disposable worker, after three minutes in one phase: the Shell image
and the control image are each downloaded and qualified on their own budget.

The download is subject to the **Settings > Integration** option, on by default.
Without it no worker is started and only a decision already in the cache is
used. When a discovery started by a process ends in an adopted decision, that
process reports the download once in the notification area.

This optimization uses private COM interfaces, a native entry callback and an
undocumented selection-array count write. Public native clear performs its own
allocation cleanup; FileSpacer does not free another component's allocation.
Unknown signatures, unsupported CFG policies and elevated processes use public
operations. CFG does not establish a public ABI contract: do not bypass a failed
validation or enable an unqualified image. Tests of foreign DLL bytes do not
establish runtime compatibility with their Windows version.

The host regression case `18-vue-shell-native/04-SelectionCache.ps1` exercises
the production adapter, asynchronous publication, persistent positive/negative
decisions, transient failure backoff, concurrent discovery and a 135-operation
real Shell matrix with 129 files. The separate stress suite exercises 135 distinct
operations with 5000 files and records private-capability diagnostics. Its views
are invisible; this is not presentation
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

Ordinary deselection clicks stay on the native input path. After a verified
global deselection notification reports zero selected items, the same binding
can batch-release the old native subscriptions, forward the original notice,
invalidate selection snapshots and cancel stale delayed activation. There is
no background-click preclear hook. Missing capability leaves native handling.

An independently qualified native selected-index range can also accelerate
explicit clearing, including free Tiles. Its matching control symbols, loaded
instructions, ownership and CFG targets are checked and cached separately;
absence retains the existing manager capability or the public path. Case
`06-SelectionRange.ps1` covers qualification, cache faults and late publication.

Cases `05-SelectionAll.ps1` and `06-SelectionAllVisible.ps1` exercise the production
adapter from empty and partial selections in nine configurations, plus a forced
public selection fallback. They verify identities, retained snapshots, selection
data objects, sorting, focus and subsequent operations. Case 06 uses visible
800-by-400 views and checks sampled icon positions. API-return timings do not
establish composed-frame presentation latency or full-application parity with
Explorer.

<a id="large-selection-reads-in-free-layouts"></a>

## Large-selection reads in free layouts

Guarded `nativeViewSelection` calls use a public snapshot for ungrouped free
Icon/SmallIcon views: complete selections of at least 128 items and partial
selections of at least 1024 items. Partial capture uses `GetSelectedItem`, with
an explicit check of index zero, and rechecks the selected indices to detect
same-count replacement. The helper reads selected child IDs and positions once,
sorts cached coordinates, starts at the focused item only when it is selected,
and constructs an ordinary array with `SHCreateShellItemArray`. Final all/selected
counts and focus must still match after index verification. Coordinate ties,
smaller/empty selections, grouping, other modes and unavailable methods retain
native `GetSelection`; cancellation rejects the stale operation.

This helper lives in `ShellViewSelection`, together with the native-array reader
and its existing mixed-provider conversion. The pure folder/item adapters remain
in `MixedFolder`. The declaration keeps its existing call form; callers without
a lifetime/generation guard keep native access. Folder-window status, selected
item, Peek and default-command calls supply that guard. Returned arrays own
their snapshots and preserve the provider's native-item conversion.

This optimization uses public APIs, has no symbol-discovery dependency and keeps
no persistent item cache. It does not populate the Shell's own selection cache
or accelerate its independent commands. `SelectionArray` tests the production
reader, exact ordered identities, attributes, data-object round trips, retained
arrays, geometry, focus, rejection and cancellation. The host `Partial` scenario
of `Run-SelectionGestures.ps1` additionally checks large subsets, same-count
replacement and late focus changes, with a qualified fixture cache supplied
through `FS_GESTURE_CACHE` when needed. `PublicFallback` separately
measures public-only clear/restoration and the following array read against
Explorer at equal viewport dimensions.

<a id="persistent-viewport"></a>

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

Database format 8 stores viewport snapshots in the nullable `scroll` column,
separately from view and icon blobs. Closing updates an existing folder only;
viewport saving ignores the layout lock because scrolling is navigation. Rekeying carries the snapshot;
global reset, deletion, expiry and quota eviction discard it with the folder state.
Tests `07-fenetres/08-DefilementPersistant` and the independent real-Shell
`ScrollState` experiment cover this behavior and its incremental cost.

<a id="activation-and-external-folders"></a>

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

<a id="tests-and-references"></a>

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
