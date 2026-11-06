# Native Shell reactivity experiments

These probes belong to the experimental `codex/reactivity-hybrid` worktree. They
are separate from the normal regression suite. Run them only through the existing
AgentWin10 transport; compilation uses MSVC on the host.

From a CMD prompt in the project directory:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case CompareDetails
```

The driver temporarily stages its sources as a regression case and removes only
the files it created. Each case creates its own fixture under the guest project
`build` directory and verifies the cleanup target before deleting it.

Required build variants are `build\baseline` (commit `20a03f1`) and
`build\hybrid` (the prototype), each containing `FileSpacer.exe` and
`FileSpacerFolder.dll`. Both use the existing Release NMAKE target with identical
`/O2 /Zi /MT /D FILESPACER_DEBUG` flags. The debug define retains `/test` and
isolates preferences and folder storage. These are optimized test builds.

Cases:

* `CompareDetails`: three alternating rounds, 5,000 files, selecting and clearing
  100, 500, 1,000, 2,500 and 5,000 items. Checks the resulting selection counts.
* `CompareList`: the same comparison in List mode, with the requested mode checked
  initially and after every selection/clear pair. Run with `-Case CompareList`.
* `ObserveExplorer`: observes the original FileSpacer build and Explorer through
  public COM interfaces, captures coherent process snapshots, and copies the exact
  relevant module images for later stack analysis. Times collected with snapshots
  are diagnostic measurements, not the primary performance comparison.
* `Placement`: compares public positioning, public `SetRedraw`, the existing
  production redraw guard, and a rejected parent redraw experiment. Checks all
  5,000 coordinates, selection, logical focus and keyboard focus after every move.
* `DesktopPlacement`: a bounded diagnostic on the real Explorer Desktop and the
  production spatial Shell host, using 100 owned icons. It saves the original
  Desktop state before creating its fixture and verifies restoration afterwards.
  This experiment deliberately does not represent a 5,000-item Desktop.
* `ProfilePlacement`: retain the bounded Desktop backup/recovery protocol, record
  actual view geometry, icon size, DPI and flags, and collect five coherent stack
  samples per host while positions alternate. Compare the normal host, a larger
  host and a new host initialized with custom positioning alone. Sampled-loop
  timings are diagnostic; the preceding three calls are measured without PSS.
* `VisualTiming`: the composed-frame autotest. The prepared workstation package
  also runs Details, List and bounded placement with Desktop Duplication.
* `CachedViews`: a controlled comparison of retaining a live spatial browser
  versus recreating it. Two public `IExplorerBrowser` hosts use application-owned
  panels in one top-level window. Checks every one of 5,000 positions, the two
  selected identities, logical focus and keyboard focus after each return. Also
  records the process private-memory delta when the second host is created.

The larger Desktop drag and placement attempts did not produce a valid comparison.
Their watchdog failures and the diagnosed monitor logging error remain recorded
in the worktree's validation artifacts.
Do not interpret them as successful gesture tests or performance results.

`DumpStacks.cpp` uses the documented Microsoft debugger interfaces. Symbol names
are diagnostic observations; no private Shell interface or command identifier is
used in the product. Inspect the recorded symbol type and unwind quality before
drawing conclusions from a stack. Use exact guest images and matching symbols.

The cached-view experiment measures return to an already loaded, unchanged
folder; it does not accelerate the initial restoration. It does not yet validate
background file changes, filtering or production lifecycle handling for two live
hosts, nor bulk selection transfer (its selection contains two items). Its memory
observation is a net process delta for this text-file fixture, not a per-folder
memory guarantee. Timings end at API return and verified state; they do not
measure presentation of the first complete DWM frame.

## Observations on AgentWin10, 6 November 2026

These are controlled experiments, not a Windows performance guarantee. The
5,000-file fixtures contain text files; the three repetitions are alternating
within each experiment. The table uses medians from the final runs.

| Operation | Reference | Prototype | Explorer |
| --- | ---: | ---: | ---: |
| Select 5,000 in Details | 635.480 ms | 85.233 ms | 90.415 ms |
| Clear 5,000 in Details | 913.037 ms | 1.533 ms | 2.132 ms |
| Select 5,000 in List | 1,094.629 ms | 584.610 ms | 848.346 ms |
| Clear 5,000 in List | 1,509.782 ms | 5.709 ms | 7.475 ms |
| Return to spatial layout, two selected items | Recreate: 1,732.901 ms | Retain: 5.096 ms | Not measured |
| Position 16 among 100, public API | Controlled spatial host: 17.813 ms | Not applicable | Desktop: 8.070 ms |

The retained-view run verified all 5,000 coordinates, both selected identities,
logical focus and keyboard focus on each switch. Creating the second live host
added 1,789,952 private bytes in that run. Earlier runs observed about 2.4 to
2.6 MB; allocation history and fixture details matter. Retaining browsers also
retains resources and potentially duplicates work on file notifications.

List improves large selection by 46.6% (1.87 times faster) and large clear by
99.62% (264 times faster), but its smaller selections regress. Median selection
times in the same List run:

| Selected items among 5,000 | Reference | Prototype | Explorer |
| --- | ---: | ---: | ---: |
| 100 | 7.029 ms | 19.448 ms | 29.068 ms |
| 500 | 32.481 ms | 72.873 ms | 103.017 ms |
| 1,000 | 71.450 ms | 122.016 ms | 167.721 ms |
| 2,500 | 319.539 ms | 308.466 ms | 444.053 ms |

This is a measured tradeoff, not an across-the-board List improvement. The List
case reuses the optimized executable identities below, alternates all three
applications, checks `FVM_LIST` (3) throughout and verifies all requested selection
counts and empty selections. Its observer and snapshots are disabled during the
timings. List and Details figures come from separate runs; comparisons within
each row are the relevant before/after measurements.

The product prototype uses the default view options for Details/List in ordinary
filesystem folders, and explicit custom positioning/ordering for spatial modes.
It still recreates the browser when changing these profiles; the retained pair
exists only in `CachedViewTests.cpp`. Menu preview, storage protection, selection
transfer, deferred closure and provider changes remain covered by the targeted
product tests. The main checkout is unchanged.

The original view already uses an owner-data `SysListView32`; its slowdown cannot
be explained as simply lacking virtualization. A coherent original-view stack
sample during clear-selection passes through `CSelectionManager::DeselectItem`,
`_AreSelectedItemsEqual`, `CListViewHost::_OnSelectionChange` and native item-state
notifications. Combined with the measured growth versus selected count, this
supports repeated selection bookkeeping as a cost. An Explorer selection sample
uses the collection path through `CDefCollection` and `CDefView`. Explorer's
roughly 2 ms clear operation was too short for the 35 ms snapshot trigger; no
internal clear-selection algorithm was established.

An optional WinEvent observer saw one selection event plus 4,999 selection-add
events in the original view, versus a few state events in Explorer. This concerns
that channel only, not every UI Automation event. The observer also slows the
original view, so the primary timings disable it. Only stacks with matching PDBs,
exact guest images and useful unwinds are interpreted.

Public `IFolderView2::SetRedraw` did not improve positioning. Final medians for
moving 16 among 5,000 were 1,908.241 ms directly, 1,970.965 ms with public redraw
suppression, and 132.359 ms with the existing guard. For all 5,000 positions, the
existing guard took 1,301.635 ms; freezing the owned parent took 36,272.929 ms.
`WM_SETREDRAW(FALSE)` changes effective visibility, a condition under which the
existing icon guard deliberately declines the view. The experiment logs that
condition before entering the helper. This accounts for losing that optimization;
it is not proof that every internal layout step has been identified. One of the
unchanged runs already saw the child visible again, reinforcing that freezing a
parent is not a dependable substitute for the existing guard. Neither candidate
was added to production. The unchanged-position skip already works (23.244 ms
in the parent experiment).

The bounded Desktop probe verifies the original coordinates, selection and flags
after removing its GUID-owned icons. It shows a smaller gap than the large-folder
case. Desktop geometry, no-scroll behavior and item population differ; it does
not establish a directly portable fast path for 5,000 free positions. The failed
larger probes are excluded from the table. The monitor was corrected both to open
its wait-chain journal and to pump its own STA before joining the ping thread;
otherwise an in-process controlled host could create an artificial timeout.

Public contracts used or considered:

* [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions): explicitly request custom positions and ordering; do not infer these capabilities from a renderer class or `FVO_VISTALAYOUT`.
* [IFolderView::SelectAndPositionItems](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifolderview-selectandpositionitems): native batch positioning.
* [IShellView::UIActivate](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ishellview-uiactivate) and [ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow): focus activation and visibility of application-owned panels in the cache probe.
* [FOLDERFLAGS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-folderflags): `FWF_DESKTOP` and `FWF_NOSCROLL` are Desktop-specific; they are not imported into ordinary folders.

Local evidence under `build/regression`:

* `experiment-comparedetails-4997dc4336c947ae951de19a0f6b4da2` (PASS).
* `experiment-comparelist-3792a00af8484d74abdaaa7b0c031938` (PASS, 90 timed operations, nine verified List sessions).
* `experiment-cachedviews-c55167daa72d4fa8b6983de887cf668c` (PASS).
* `experiment-desktopplacement-a9edf3ae9c504768b74415da2ae08cee` (PASS, Desktop restoration verified).
* `experiment-placement-03d533ca7be64b0bb01bc9604f48d84b` (PASS, every coordinate and selection/focus checked).
* `explorer-observation-vm-01`, `explorer-observation-noevents-vm-01`,
  `explorer-observation-images-vm-01` (PASS, diagnostic captures).
* `hybrid-menu-final-vm` (4/4 PASS), `hybrid-mixed-view-final-vm` (PASS),
  `hybrid-large-layout-vm-01` (PASS, all 5,000 saved positions).
* `hybrid-close-validation-final-vm` (2/2 PASS),
  `hybrid-logical-sort-final-vm` (PASS).
* `hybrid-large-layout-final-vm` (PASS, final Debug binary and all 5,000 positions).

Optimized executable SHA-256 identities used for the final Details comparison:

* Baseline: `B6EC7719091D68752D73D2271C595DB40EA15AC3585E18FC0078C3D5E05DD5AF`.
* Hybrid: `976FD27515B26099762B38420749F9AD5FB63D769E267D25EF62D5CA1A5B933F`.

Offline stacks and exact-image symbol analysis are under `build/observation`.
Validation artifacts are local and intentionally excluded from source control.

## Validation and remaining review

The final Debug application and all native probes build with MSVC `/W4`; the
earlier narrowing warning in `CachedViewTests.cpp` was fixed with the appropriate
explicit enum-to-`UINT` conversion. The debugger reader was rebuilt and validated
against the real captured clear-selection stack. No full regression-suite pass is
claimed.

Static analysis verdict: **REVIEW**. Clang-tidy and Cppcheck both completed for all
six changed/new C++ translation units, including `FolderWindow.h` through
`FolderWindow.cpp`, plus the host and original product units for comparison. No
analysis crashed, timed out or needed reduced ATL coverage. Configuration was
Windows x64, C++14, MSVC/ATL 14.44.35207 and Windows SDK 10.0.19041.0; clang-tidy
used the real headers, and Cppcheck the official Windows, ATL and SQLite models.

No new confirmed defect remains. Items retained for review:

* Clang's enum-range warnings on documented flag combinations (`FOLDERVIEWOPTIONS`
  value 6 and the minidump mask); signed flag operations and Win32 callback casts.
* Cppcheck's always-true/false conditions across opaque Shell callbacks and the
  message pump. Removing these guards would defeat the reentrancy protections.
* Experimental-code style suggestions: intentional `.cpp` reuse, adjacent string
  arguments, `CComPtr` moves, a shadowed name, vector reservation, and copying
  rules for a scoped file wrapper that is never copied.
* Clang's thread exception warning traces the generated copy of captured strings
  at launch; the diagnostic workers catch `std::exception` in their bodies.
  Thread-argument construction occurs in the launching thread. The monitor's
  scope guard also joins it if launch or setup fails.

Cppcheck's mutex `deallocuse` report also occurs on the same code in `20a03f1`;
it was not introduced by this prototype. Full logs and comparison results are in
`build/reactivity/spatial-analysis`. Diagnostic-worker catches, monitor cleanup,
and the debugger mapping's RAII ownership were tightened during analysis; no
product refactoring was performed to silence style suggestions.

The List extension changes only the experiment, its driver and this record.
MSVC `/W4` compiled the probe without new warnings; the application Debug build
also succeeds. Clang-tidy and Cppcheck completed for the changed observation unit
and its Desktop including unit, plus both versions from `40fcc38` for comparison.
No new diagnostic remains after accounting for the renamed comparison function
and diagnostic paths. The earlier REVIEW items above remain unchanged; no product
behavior was changed to hide the measured List regressions. This comparison is
recorded under `build/reactivity/list-analysis`.

## Further placement observation and composed-frame instrumentation

The additional captures in
`experiment-profileplacement-d1c0a82c66104901978bb3f9c3dbc7cb` and
`experiment-profileplacement-5115bbd2397b4788b220b6371fc63c4b` pass, including Desktop
restoration. They confirm that both views use 48-pixel icons at 96 DPI. The Desktop
view is 1,024 x 768; the original controlled view is 834 x 561. Increasing the host
window leaves a 1,024 x 749 public view client in these captures, so the historical
`matched-host` label in their files must not be read as exact viewport matching.
The current diagnostic labels that variant `larger-host`.

Several useful controlled-host stacks go through `CListViewHost::SetItemPosition`,
`_ClearArrangeFeedback`, `SetHeaderFlags` and header-control notifications, including
theme/header invalidation work. Public symbols sometimes assign very large offsets
to nearby functions; those names and broken stack prefixes are not interpreted as
exact algorithms. Useful Desktop samples show list-view painting and label work
outside a `SelectAndPositionItems` stack. This establishes that an API return does
not establish completed presentation; it does not measure the physical display.

Testing `FVO_CUSTOMPOSITION` without `FVO_CUSTOMORDERING` did not remove the feedback
path or improve timings. In the second run, moving 16 among 100 took 9.159 ms on
the Desktop, 18.521 ms in the normal host, 26.550 ms in the larger host and
20.923 ms in the position-only host (three-call medians, before sampling).
Every owned coordinate is verified. No flag or internal header manipulation was
added to the product. Decoded diagnostic stacks are under
`build/observation/placement` and `build/observation/position-only`.

Build the standalone workstation package from CMD with:

```bat
tests\experiments\reactivity\Prepare-VisualPackage.cmd
```

This reuses the existing prepared regression package and MSVC build procedure.
The default output is `build\visual-timing\package`; use a fresh parent directory
for another build. The package contains its native executable and embedded
optimized reference/prototype actors, and needs no compiler on the measurement
machine. Its launcher runs a composed-pixel self-test before any requested
scenario and retains the existing test-state isolation/restoration. See
`MESURES-VISUELLES.txt` for commands, output interpretation and Desktop recovery.

`PresentationTiming.h` uses the public
[Desktop Duplication API](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api),
with [LastPresentTime](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/ns-dxgi1_2-dxgi_outdupl_frame_info)
QPC timestamps and SHA-256 comparisons of the visible view ROI. It records API
duration, first observed pixel change and the beginning of the final continuous
run matching the last quiet image. Per-frame timestamps/hashes and before/final
BMPs are preserved; the launcher creates `presentation-summary.csv`.
Initialization requests one public repaint after duplication is ready, outside
the measured operation. The self-test changes its known background at that point:
the first desktop frame is not assumed to arrive on an otherwise static display.

The ROI must fit the display work area and is checked at nine points for another
covering window. This is an occupancy check, not a proof that every pixel stayed
unoccluded. The workstation must remain free. The image reference is observational:
at least 1.5 s after API return, at least 500 ms of unchanged ROI pixels, and a
5 s maximum after return. It does not claim that all asynchronous work is done.
It covers the existing selection/clear and 16-icon positioning operations, not a
complete first-folder-open or mode-switch benchmark.

No visible change produces no display latency; an unstable or reverted image
produces no final latency. Accumulated frames mark a conservative upper bound.
The instrumentation changes CPU/GPU load and must be kept separate from uncaptured
timings. These are composed-image observations, not monitor scan-out measurements.

Validation: the final native instrumentation compiles with MSVC `/W4`, with the
new shadow warning corrected. The frame-history self-test checks that an early
temporary match is not mistaken for the final continuous image, and that an
unchanged image has no invented latency. AgentWin10 creates the duplication but
supplies zero desktop frames, so GPU/pixel validation fails explicitly rather than
falling back to API time. The subsequent workstation run passed the GPU autotest
on an NVIDIA RTX 2000 Ada Generation Laptop GPU; its unequal Explorer viewport
and delayed infotips motivated the equal-viewport protocol below.
Clang-tidy and Cppcheck cover the changed
observation/Desktop units plus the visual unit, including the new header. Static
verdict remains REVIEW for the documented existing diagnostics and new non-defect
suggestions on the initialized D3D output enum and a loop/algorithm style choice.
No confirmed new defect remains; no product source changed in this extension.

## Equal-viewport workstation comparison

The new launcher defaults to `All`: one composed-pixel self-test, all eight
presentations (extra-large, large, medium and small icons, List, Details, Tiles,
Content), then placement. Each selection presentation uses the same 800 x 400
physical-pixel public `IShellView::GetWindow` rectangle, including its native view
chrome, on the primary monitor at matching DPI. The outer application frames may
have different sizes. Setup adjusts each frame through `SetWindowPos`; dimensions
are checked again for every measured operation. A mismatch fails instead of
silently clipping the larger Explorer view.

`capabilities.csv` records requested/read-back modes, icon sizes and HRESULTs.
An unavailable or differently normalized mode is explicitly `UNSUPPORTED` and
has no timing samples; successful execution must not be interpreted as coverage
of an unsupported presentation. `viewport.csv` records the ROI, full view size,
DPI and parked cursor position. The launcher aggregates these files and timing
samples under a unique `results/comparison-*` directory.

A blank owned topmost window parks the pointer outside the measured rectangle;
the original position is restored on exit. This avoids delayed Shell infotips
without changing user preferences. Placement uses the documented Shell
[MinimizeAll](https://learn.microsoft.com/en-us/windows/win32/shell/ishelldispatch-minimizeall)
and [UndoMinimizeALL](https://learn.microsoft.com/en-us/windows/win32/shell/ishelldispatch-undominimizeall)
commands around the Desktop stage, retaining the visibility check and logging
the covering HWND, class, title and PID on failure. The Desktop cannot be resized:
it uses an 800 x 400 primary-monitor ROI while the controlled spatial host's
whole public view is 800 x 400. The full Desktop geometry remains separately
logged; equal ROI does not imply equal full Desktop size.

`Run-Experiment.ps1 -Case VisualTiming -VisualScenario Protocol` verifies the
eight-view setup, exact geometry, pointer and selection counts in AgentWin10
without GPU capture. `PlacementProtocol` exercises the corresponding Desktop
visibility/restoration and spatial-host setup. The driver selects the requested
default in its owned temporary staged script before normal package hashing,
because host environment variables are not inherited by the VM controller.
These diagnostic scenarios do not provide composed-frame timings.

Final validation (2026-11-06): MSVC Debug and the native `/W4` build pass without
new warnings. AgentWin10's `experiment-visualtiming-f1f31ec2b2114f36a8c17cd58e39b7b3`
passes all 24 application/presentation pairs at 800 x 400, 96 DPI, with exact
mode/icon read-back and 48 selection/clear checks. The final viewport CSVs are
parsed and checked for row count, dimensions and DPI.
`experiment-visualtiming-1c2d3e6a13d041ef902e5d727062965f` passes all six placement
operations with all 100 coordinates checked after each operation; Desktop state
restoration is verified twice and `UndoMinimizeALL` succeeds. Desktop ROI and
host view are both 800 x 400; full Desktop size is separately logged as 1024 x 768.
This validates setup and restoration, not new GPU timings on the VM.

Clang-tidy and Cppcheck finish for all three changed C++ units, with the visual
unit covering the changed capture header and its real Windows/ATL dependencies.
Compared with the preceding analysis, new diagnostics are Cppcheck's `find_if`
style suggestion and `seekOnAppendedFile`. The seek queries the end position
with `ftell` to emit exactly one CSV header; it does not attempt to change the
append write position. The final VM CSVs confirm a single header and the expected
rows. These are retained for REVIEW, together with the previously documented
diagnostics; no confirmed new defect remains. The log-header correction was made
after inspecting the first VM run, then both diagnostic scenarios were rerun.
Logs and the preceding-analysis comparison are under
`build/reactivity/presentation-analysis` and `presentation-analysis-before-v3`.

The Desktop probe includes an explicit `recover` mode:

```bat
DesktopPlacementTests.exe C:\FileSpacerTests\<run>\output\regression\36-reactivity-prototype\04-DesktopPlacement recover
```

Run recovery in the guest with the original output directory if its watchdog
interrupts the case. The binary backup identifies the original Desktop PIDLs,
positions, selection, flags and the GUID prefix of the owned fixture. Recovery
never closes the Desktop or removes files outside that prefix.

## Large spatial selections: investigation on 2026-11-06

The workstation v3 archive stopped during ExtraLarge after 48 display samples
(pointer parking window covered). It does not validate the other seven views.
The first complete common 5,000-item repetition shows a substantial remaining
gap. It must not be described as parity with Explorer. Workstation presentation
measurements and the VM's API timings below are different measurements.

For that common repetition, final observed display latencies are:

| Operation, 5,000 items | Classic FS | Hybrid FS | Explorer |
|---|---:|---:|---:|
| Select | 884.267 ms | 890.853 ms | 90.586 ms |
| Clear | 778.273 ms | 799.982 ms | 30.187 ms |

The corresponding API times are 859.360/857.012/34.581 ms for select and
756.474/764.930/3.410 ms for clear (classic/hybrid/Explorer). Most of this
workstation gap precedes presentation. These are individual samples, not medians.

### Evidence and rejected shortcuts

The controlled runs use the real Shell, a 5,000-file isolated fixture,
800 x 400 view, 96 DPI and ExtraLarge icons (FVM_ICON, 256). Checks cover
selected identities, logical focus and keyboard focus. Qualifying spatial profiles
also check all 5,000 free coordinates; profiles lacking that contract are not
spatial candidates.
No internal HWND manipulation or private Shell command is used.

`SelectionStrategies` compares public batch selection, public SetRedraw, the
documented legacy IShellFolderView selection commands, and folder options 6
(custom position/order), 2 (position), 4 (order), and 0 (default). The default
profile is faster but provides no custom-position contract. Removing custom
ordering while retaining custom positioning does not close the gap. Neither
public redraw suppression nor the legacy bulk command gives a decisive gain.
OLECMDID_SELECTALL is unavailable on the tested view, so it is skipped. The
legacy interface is diagnostic only and is not introduced into the product.

For profile 6, ordinary batch selection/clear medians (3 repetitions, milliseconds)
from `experiment-selectionstrategies-7a22b5d65a414c7db6ae7a70c43a2664` are:

| Items | Select | Clear |
|---:|---:|---:|
| 100 | 4.92 | 3.49 |
| 500 | 53.44 | 33.73 |
| 1,000 | 106.64 | 104.70 |
| 2,500 | 355.03 | 398.57 |
| 5,000 | 1,390.43 | 1,736.16 |

The growth is superlinear in this experiment. This is evidence about the tested
Shell implementation, not proof of a particular internal algorithm.

`SelectionIndexes` also checks index-based calls and forward/reverse orders.
Reverse orders make later calls on the same view much slower (one subsequent
clear reaches 141 seconds). Those contaminated medians are not used as speedup
claims. `SelectionFresh` repeats each candidate in a fresh native process, in
alternating order: 5,000-item medians are 1,868.67/1,922.76 ms for batch,
1,713.61/2,317.03 for forward indexes, 1,936.86/2,418.23 for indexes with public
redraw suppression, and 1,630.34/1,822.81 for a host that never creates view
automation. Index mapping costs another 28–65 ms outside those timings.
The overlapping ranges provide no decisive replacement for the batch path.

`SelectionCallbacks` separately detaches/re-attaches the automation observer:
1,169.85/2,089.10 ms with it and 1,175.75/1,902.35 without it. Removing observers
does not explain the order-of-magnitude workstation gap. A temporary switch to
Details and back (`SelectionMode`) fails the free-coordinate invariant; that
candidate is rejected rather than delivered. Enumerating the current selection
in batches of 128 also gives no measurable improvement and is discarded.

`SelectionKeyboard` compares three fresh-process repetitions of batch selection
with actual Ctrl+A routed through IShellView::TranslateAccelerator. The final run
`experiment-selectionkeyboard-52fbecea8abf403c87af43e8a13a12b3` passes all six
cases: selection medians are 1,328.73 ms for batch and 922.89 ms for Ctrl+A.
The respective ranges are 975.25–1,412.10 and 880.28–970.90 ms. All positions,
selected identities and both forms of focus pass. This gesture measurement
includes input dispatch and is not identical to timing one API call.
Clear uses the same native call in both arms; its differing timings are not an
optimization. FileSpacer already routes the view's keyboard shortcuts to the
Shell (`FolderWindow::handleTopLevelMessage`); no simulated-input selection is
introduced into the application. Initial attempts failed to obtain foreground.
The final test dismisses guest menus with Escape and clicks only an exposed
caption belonging to its own fixture, restoring the guest cursor afterwards.

`VisualTiming -VisualScenario ProfileIcons` records public PSS thread snapshots
during 5,000-item operations, without GPU capture. Exact guest images and symbols
are retained for offline stack inspection. Slow clear stacks enter
`shell32!_AreSelectedItemsEqual`, `CSelectionManager::DeselectItem` and the native
`CListViewHost` selection-change path. FileSpacer's label subclass occurs as a
forwarder; this does not establish that label drawing is the cause. Select stacks
with broken unwinds or large symbol offsets are not used to infer an algorithm.
Sampling perturbs timings, so these profiling times are not benchmark results.

Together, the controlled hosts and stacks point to work inside the native
custom-position view path. The observed renderer classes are not public API
guarantees and are not used to choose an undocumented shortcut. There is no
demonstrated standard bulk-selection fix for the remaining full-selection gap.

### Experimental optimization: restore only selection differences

`src/ShellViewSelection.cpp` retains the exact-same-selection shortcut. For a
changed selection it prepares the complete difference before changing the view,
retains common opaque PIDLs, deselects only extras and selects only missing items
in snapshot order. Pure additions use public GetSelectionState without cloning
the current selection. Disjoint replacements and providers without a usable
selection enumeration retain the original clear/reselect path. Opaque PIDLs
are not decoded: equal bytes can be retained; different encodings are resolved
by the Shell through the original per-item operations.

Logical/keyboard focus and cancellation checks remain in place. Version-1
snapshots remain readable; the scroll correction below adds an optional version-2 payload. Owned deselection PIDLs use unique_ptr with CoTaskMemFree;
an experimental unsafe move of CComHeapPtr was caught by the native test and
corrected before delivery. Batch addition was also discarded after a stale-PIDL
test exposed different behavior; additions retain the original SelectItem calls.

This helps restoring an existing large selection during view changes/reuse.
It does not make selecting or clearing all 5,000 items from scratch faster.
Comparing the entire current/target set adds linear work when the sets differ;
it is not a promise that every replacement is faster.

`SelectionReference.inc` freezes the exact helper from revision 514f6f0.
`SelectionPaired` compiles both implementations into the same process and
alternates their order, restoring the same starting selection before each call.
The cases cover remove one, add one, unchanged, clear all, full selection and
disjoint replacement. Each timed operation is followed by identity, coordinate
and focus checks. `SelectionOptimized` repeats this with MSVC /O2, /MTd and
_ITERATOR_DEBUG_LEVEL=0 for both implementations. This is an optimized helper,
not a full Release application nor a physical display-latency measurement.
The script scopes CL to native-helper compilation and restores its prior value.

Final optimized paired run
`experiment-selectionoptimized-259a47fea98647409453e2136e4b0af9` passes all
36 operations. Medians of three alternating repetitions, API milliseconds:

| Restoration | Reference | Difference-based | Change in time |
|---|---:|---:|---:|
| Remove one from 5,000 | 2,575.23 | 424.32 | -83.5% |
| Add one to 4,999 | 2,573.80 | 212.05 | -91.8% |
| Unchanged 4,999 | 176.80 | 133.01 | same existing path; variability |
| Clear 5,000 | 1,481.09 | 1,313.72 | same native operation; variability |
| Select 5,000 from empty | 1,169.37 | 1,105.98 | same addition path; variability |
| Replace disjoint sets of 2,500 | 847.27 | 883.89 | +4.3% (comparison overhead) |

VM timings vary substantially between runs; the comparison is within the same
process/configuration, not against an earlier VM session. Another paired run
obtained approximately 80% and 94% reductions for the two incremental cases.
Only the incremental reductions are attributed to the new implementation.

`SelectionErrors` checks cancellation before mutation (selection and all positions
retained), then deletes one owned fixture, waits for real Shell refresh, seeds a
known surviving free layout and checks restoration of an old 5,000-item snapshot:
all 4,999 surviving identities and coordinates must remain correct.

Reproduce through the normal VM transport, for example:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionOptimized -TimeoutSeconds 900
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionErrors -TimeoutSeconds 900
```

The previously shipped workstation actors are frozen; they do not include this
new restoration optimization. No new workstation display measurements or package
are implied by the VM API benchmarks.

### Scroll correction and validation

The reopening failure was reproduced before this change at revision 514f6f0:
57.2486546965741 percent before closing, 0 after reopening. The fast Details
view returns E_NOTIMPL for GetItemPosition. The old test required SysListView32;
its replacement uses the independent, public UI Automation ScrollPattern from a
bounded MTA observer in the external test process. It still fails on the old
product and passes on the corrected application, without applying any scroll
correction from the test.

The product takes the documented [in-process IAccessibleEx client route](https://learn.microsoft.com/en-us/windows/win32/winauto/using-iaccessibleex-from-a-client):
IAccessible, IServiceProvider, IAccessibleEx, IRawElementProviderSimple,
GetPatternProvider(UIA_ScrollPatternId), IScrollProvider. No IUIAutomation client
engine, worker thread, polling timer, private window class or synthetic input
is introduced into FileSpacer. Only the public IShellView window and its MSAA
accessibility tree are queried. Discovery is bounded to 128 objects / depth 8
and follows supported interfaces, not names or child-window layout.

When GetItemPosition is unavailable, capture stores both percentage axes and the
current view mode in a version-2 payload (24 additional bytes). The old version-1
anchor format remains unchanged/readable. Restore applies percentages only to
the captured presentation, after selection and focus; it leaves non-scrollable
axes untouched. All COM boundaries retain the view-generation checks. A closed
or replaced generation is abandoned synchronously. Nonfinite or truncated scroll
payloads are rejected before changing the view. No slow-profile fallback occurs.

A percentage represents relative location, not item identity. Folder content,
sort or geometry changes can change the items at that location. That limitation
is accepted in favor of the fast native view and a small synchronous adaptation.
The classic profile does not expose this in-process ScrollPattern in the tested
system; it retains its existing item-anchor path. An incoming percentage whose
new provider does not expose the optional pattern is not applied, rather than
inventing a control protocol or failing the other state restoration.

`ScrollPercent` is a separate external-client feasibility probe. It deliberately
applies a correction after recording the old reopening failure and therefore is
not a product regression-test PASS. On 200 / 5,000 files its capture medians were
8.24 / 5.87 ms, and its correction medians 47.14 / 50.01 ms (3 repetitions;
first external UIA capture 159.77 ms). These include a fresh MTA thread and client
lookup, not the chosen in-process product implementation. CSV numeric output is
now invariant-culture; the first archived run uses French decimal commas.

`ScrollProvider` exercises the real production functions and the in-process
provider on the GUI thread. The final run
`experiment-scrollprovider-d35c07db549d41b4b545d0332ebe0116` passes 3 repetitions
each for vertical Details and horizontal List on 5,000 files. Median complete
snapshot capture / restore: 2.480 / 0.512 ms in Details and 3.155 / 0.674 ms in
List. Each snapshot contains one selected/focused identity. The native provider
itself is the independent oracle for percentage quantization. Selection,
logical focus and keyboard focus are checked, plus cancellation, truncated/NaN
payload rejection, and actual native-view destruction during provider discovery.
The first provider lookup ranged from 11 to 95 ms across these VM runs; warmed
lookups were approximately 0.3–0.6 ms. No universal startup bound is implied.

MSVC Debug and the optimized comparison build (/O2 /Zi /MT with /test enabled)
pass /W4 without new warnings. Targeted AgentWin10 product checks:

* `scroll-provider-reopen-final`: real Details reopen; 57.24865% becomes 57.24784%,
  selection remains empty. The external observer applies no correction.
* `scroll-provider-persistence-final`: the existing real Shell persistence,
  selection, icons, filtering and eight teardown/recreation cases pass, including
  independently assembled version-1 snapshots.
* `scroll-provider-preview-final`: all four view-menu preview, option, keyboard
  and external-file cases pass.
* `ScrollProvider`: the two-axis / lifetime / invalid-data checks above pass.

This is targeted coverage, not a full-suite pass. Filesystem namespace extensions
and another Windows installation still require their own capability validation.

Clang-tidy and Cppcheck complete on every changed C++ unit and the frozen reference,
with the reference .inc covered through its caller. Analyses use Windows x64,
C++14, real MSVC/ATL 14.44.35207 and SDK 10.0.19041.0 headers for clang-tidy, and
Cppcheck's official windows/microsoft_atl/sqlite3 models. No timeout, crash,
parser failure or reduced ATL analysis occurs. Static verdict remains REVIEW:

* New in-process code: clang-tidy suggests moving an ATL CComPtr assignment.
  This is a performance/style suggestion; the explicit retained reference is safe.
* Selection-difference code: signed-mask and three STL-algorithm style suggestions.
* Probe: Cppcheck reports pointerOutOfBounds on the NaN mutation after a vector
  reassignment. The independently copied `saved` vector is checked to be at least
  44 bytes, reassignment restores that size, and the 8-byte write at offset 28
  ends at byte 36. Both native cases execute this check successfully; clang-tidy
  does not report it. This remains documented for REVIEW, without suppressing it.
* Existing documented enum-mask, .cpp reuse, RAII-state and other probe diagnostics
  remain. Before/after analysis confirms the older ignored-HRESULT diagnostic.

The experimental ownership bug and the probe's signed OBJID / enum-to-UINT build
warnings were corrected, then the affected checks were rerun. Logs are in
`build/reactivity/scroll-analysis`; baseline source before the scroll adaptation
is retained separately for comparison. The main checkout remains unchanged.

### Refreshed eight-view comparison after the scroll correction

AgentWin10, 6 November 2026. Both FileSpacer actors use the same optimized
/O2 /Zi /MT configuration with /test isolation. The classic actor is revision
20a03f1; the current actor contains the differential selection and scroll changes
above. SHA-256 of the compared executables:

* Classic: B6EC7719091D68752D73D2271C595DB40EA15AC3585E18FC0078C3D5E05DD5AF.
* Current: C3BB41325CCEFB9C19647690FEFA0A891269B4A211E14C3BBCBA07D59B1FC04E.

Every actor exposes the requested mode and icon size at exactly 800 x 400 pixels,
96 DPI. The table gives three-repetition medians for 5,000 text files. Positive
time reduction means faster than classic; negative means slower. A ratio above
one means slower than Explorer. Decimal commas are used below.

These are synchronous public API times, not composed-frame/display latency or
keyboard gesture timings. select_5000 uses SelectAndPositionItems from an empty
selection; clear_5000 uses the native deselect-all operation. They do not measure
the incremental snapshot-restoration path optimized above.

| View | Operation | Classic ms | Current ms | Explorer ms | Time reduction vs classic | Current / Explorer |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| extra-large | select_5000 | 2435,156 | 2533,197 | 94,618 | -4,0% | 26,77x |
| extra-large | clear_5000 | 1703,110 | 1706,406 | 1,667 | -0,2% | 1023,64x |
| large | select_5000 | 2399,763 | 2504,904 | 179,473 | -4,4% | 13,96x |
| large | clear_5000 | 1553,215 | 1423,114 | 2,946 | 8,4% | 483,07x |
| medium | select_5000 | 2359,499 | 2351,249 | 289,148 | 0,3% | 8,13x |
| medium | clear_5000 | 1309,033 | 1641,449 | 3,892 | -25,4% | 421,75x |
| small | select_5000 | 2203,028 | 1956,653 | 218,749 | 11,2% | 8,94x |
| small | clear_5000 | 1728,637 | 1486,410 | 3,181 | 14,0% | 467,28x |
| list | select_5000 | 973,676 | 820,472 | 784,512 | 15,7% | 1,05x |
| list | clear_5000 | 1380,918 | 7,427 | 8,183 | 99,5% | 0,91x |
| details | select_5000 | 1129,956 | 140,621 | 124,442 | 87,6% | 1,13x |
| details | clear_5000 | 1736,307 | 2,032 | 2,313 | 99,9% | 0,88x |
| tiles | select_5000 | 4163,533 | 3475,972 | 171,559 | 16,5% | 20,26x |
| tiles | clear_5000 | 2053,361 | 1611,871 | 3,188 | 21,5% | 505,61x |
| content | select_5000 | 3572,563 | 3497,878 | 95,307 | 2,1% | 36,70x |
| content | clear_5000 | 1783,432 | 1640,417 | 2,126 | 8,0% | 771,60x |

Details and List approach Explorer, but spatial presentations remain substantially
slower for these bulk operations. The scroll correction does not change those
native selection paths. Small before/after changes in unchanged spatial paths
must not be attributed to that correction; VM timing variability remains visible,
including the slower medium-icon clear. Explorer's very short clears make the
ratios large: absolute spatial clear times here remain approximately 1.4–1.7 s.

Data provenance and failures are preserved explicitly:

* experiment-visualtiming-858d0e76dfcc480aa148f0f65594120f reached its 900-second
  watchdog during Content. It is a FAIL overall, not an eight-view PASS. Its other
  seven views each contain all 90 result rows and nine verified capabilities.
* experiment-visualtiming-e671d046667547b686d79d81f31054fc reran Content alone
  with ContentCpu and passed, with all 90 rows and nine verified capabilities.
* The combined eight-view source contains 720 complete result rows and 72
  supported, matching-geometry capabilities. HRESULTs, observed item/selection
  counts, the three actor repetitions and both executable hashes were checked
  for every view. The unfinished Content portion of the first run is excluded.
* A subsequent largest-only AllViewsApi attempt, VM run
  05e20ef542e545bca0ce41d318518711, stopped after 80.25 seconds with
  "Pointer parking window is covered" during extra-large icons. This is a failed
  benchmark precondition, not a product crash or a successful comparison. Its
  partial timings are excluded from the table. It does not replace either run.

AllViewsApi now limits the common matrix to 5,000 selected items, retaining three
alternating repetitions and all eight views to fit the existing 900-second
watchdog. The ordinary per-view scenarios and ContentCpu retain the five input
sizes. No GPU display timing is measured by these CPU/API scenarios:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case VisualTiming -VisualScenario AllViewsApi -TimeoutSeconds 900
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case VisualTiming -VisualScenario ContentCpu -TimeoutSeconds 900
```

The largest-only routing and initial operations executed before that precondition
failure; its complete eight-view execution is not validated. The two final C++
benchmark units were rerun through clang-tidy and Cppcheck after these routing
changes; both analyses completed, with the documented probe/style REVIEW points.
## Selection internals: measured explanation and standard candidates

Further investigation on 6 November 2026. Product sources are unchanged in this
step. The new SelectionInternalsTests helper runs in fresh processes through the
same dedicated AgentWin10 transport, with the real native Shell and 5,000 owned
files. A test-only subclass on the public IShellView HWND counts forwarded
LVN_ITEMCHANGED, LVN_ODSTATECHANGED and NM_CUSTOMDRAW messages and the time spent
in their original handlers. It does not suppress or rewrite notifications.
Child ListView discovery, raw control messages, vtable addresses and numeric
DefView command IDs are explicitly diagnostic and are not product dependencies.

### What consumes the time

SelectionInternals passed in
experiment-selectioninternals-d7b8d08596934d949199c4f14b5ef71d. For the spatial
profile 6 (CUSTOMPOSITION | CUSTOMORDERING), one diagnostic 5,000-item batch
selection took 2,510.854 ms: 5,000 item-change notifications consumed 1,902.318 ms,
with zero custom-draw notifications during that call. Clear took 1,340.322 ms:
its single global item-change notification consumed 1,245.864 ms, also with zero
custom-draw notifications. One notification can therefore contain the expensive
loop; notification count alone does not describe the algorithm.

The native control was owner-data (LVS_OWNERDATA). A raw global control selection
and the legacy public IShellFolderView::Select each emitted one notification and
selected all identities correctly, but still took approximately 0.8–0.9 s and
left clear at 1.2–1.6 s in that pilot. Disabling animations and scroll tips
(profile 54) retained the same expensive path. IFolderView::SelectItem(-1, ...)
returned E_INVALIDARG: the undocumented negative-index hypothesis is rejected.
Native numeric commands improve selection in that pilot but do not close the
gap; inversion clears one item at a time and is no useful clear replacement.

Offline analysis uses the exact previously captured guest process snapshots and
module images from experiment-visualtiming-219c9590394b482b92528a0cf564b23a,
matching Microsoft PDBs and the Debug Engine. DumpStacks now accepts an optional
Unicode debugger-command argument after the normal dump/thread/symbol-path
arguments. The Explorer snapshot's unwind is insufficient for a useful selection
stack; no execution path is inferred from its broken frames. Its loaded, matching
ExplorerFrame image can still be disassembled and its symbols inspected.

Observed implementations in this Windows build:

* The custom-position renderer uses shell32!CSelectionManager. SelectItem calls
  IsSelected, then _FindSelected, which scans its DSA entries and invokes
  _AreSelectedItemsEqual for each candidate before appending a new entry.
* Its _OnClearSelection loops over selected entries, resolves their view indexes,
  calls _OnSelectionChange / DeselectItem (another linear search), then finally
  calls ClearSelection. This final global clear does not avoid the earlier work.
* Explorer's modern renderer exposes explorerframe!CItemsView and CSelectionState.
  The inspected _DeselectAll clears CItemMap / CSimpleHashTable containers through
  RemoveAll. This is a different selection representation from the legacy DSA.
  The matching ExplorerFrame image is 10.0.19041.6280; Shell32 is 10.0.19041.6456.
  These private implementations explain observations on this build, not future
  Windows contracts or callable product interfaces.

### Direct comparison counts and causal check

SelectionComparisonCounts passed in
experiment-selectioncomparisoncounts-905d4bc6efba48eda8ede2591ea30f49. The driver
requires AgentWin10 and pins the captured Shell32 image to SHA-256
936C33A8283196A070AA6F844A65A29A8F99F760EAE0016754C1CBF17D62BDB7.
The helper verifies the private entry bytes and installs a process-local
breakpoint on the neutral REX prefix of _AreSelectedItemsEqual. The original
instruction remains semantically intact; the original code and protection are
restored on scope exit. No system file or another process is patched.

| Selected items | Spatial select comparisons | Spatial clear comparisons | Modern comparisons in this legacy function |
| ---: | ---: | ---: | ---: |
| 100 | 4,950 | 10,000 | 0 |
| 200 | 19,900 | 40,000 | 0 |
| 500 | 124,750 | 250,000 | 0 |

The measured counts are n(n-1)/2 and n squared. Extending those observed paths to
5,000 predicts 12,497,500 and 25,000,000 comparisons; those 5,000-item counts were
not measured with breakpoints. Breakpoint-instrumented durations are deliberately
excluded from performance comparisons. Public selection arrays, selected
identities, all 5,000 free coordinates and both focus states pass afterwards.

SelectionCausal passed three alternating fresh-process repetitions in
experiment-selectioncausal-58ba9cd321644908bc63f0fff3c28382. A second, strictly
private probe temporarily bypasses IsSelected only for selection from empty,
and DeselectItem only while the native global clear will finish with ClearSelection.
Code bytes are restored before validating the public state. These ablations do
not implement general selection semantics and are not proposed for the product.

| Spatial 5,000-item operation | Original median ms | Private ablation median ms | Time reduction |
| --- | ---: | ---: | ---: |
| Select | 1,126.796 | 380.126 | 66.3% |
| Clear | 1,318.229 | 109.566 | 91.7% |

Every operation retained the expected selected identities, all 5,000 coordinates,
logical focus and keyboard focus. This makes the internal lookup cost causal,
while the remaining latency shows it is not the only cost. It is an experimental
result for this exact binary, not a supported optimization or a display-latency
measurement. The private probes are separate test executables; FileSpacer does
not contain or install their patches.

### Public API candidates and six-view control

SelectionMatrix passed in
experiment-selectionmatrix-8c96acb85952409580d0f983f9b8fbaf. Each view/profile pair
has three fresh-process repetitions, with profile order alternating. Values below
are medians of synchronous API time in a minimal native host at 800 x 400 / 96 DPI,
not the full FileSpacer/Explorer actor comparison above. Free coordinates are
seeded and checked for the four legacy icon modes; Tiles/Content check identities
and focus but are not a placement validation. Profile 0 has no free-position
contract and no free-position check. The first four extra-large diagnostic cases
are excluded from the repeated matrix medians.

| View | Spatial select ms | Modern select ms | Spatial clear ms | Modern clear ms |
| --- | ---: | ---: | ---: | ---: |
| extra-large | 1,470.944 | 54.039 | 1,794.858 | 1.492 |
| large | 1,158.375 | 152.581 | 1,614.035 | 2.653 |
| medium | 1,255.255 | 263.837 | 1,403.996 | 3.597 |
| small | 1,255.431 | 205.179 | 1,274.622 | 2.565 |
| tiles | 3,558.004 | 144.019 | 1,252.749 | 2.559 |
| content | 3,113.931 | 72.574 | 1,272.310 | 4.786 |

The same matrix rejects public batch deselection and clearing via a snapshot
IShellItemArray as improvements: one diagnostic extra-large clear was 1,581.639 ms
through the original call, 1,891.083 via batch deselect, 2,516.119 via the selection
array, and 17,221.978 with its reverse order. They retain the expensive native
manager rather than replacing it.

SelectionAccessibility passed its investigation protocol in
experiment-selectionaccessibility-abd4b738018f4b5486c2c2142e993c96:

* Public IAccessible::accSelect per item preserves identities, coordinates and
  focus, but costs 1,607.141 / 1,697.734 ms (select / clear), versus
  1,217.776 / 1,293.468 for the batch control in that diagnostic run.
* Raw per-item ListView calls also provide no gain: 1,336.911 / 1,789.097 ms.
* The accessible-range candidate selects only 4,993 expected identities because
  the native anchor remains relevant. It is explicitly REJECTED, not a valid
  fast selection. The investigation passing means this rejection was recorded;
  it does not qualify every tested candidate.
* The first two attempts stopped on the newly added selection-array consistency
  assertion for the normal batch control. The native empty GetSelection(FALSE)
  returns ERROR_NOT_FOUND with a null array on this installation. The final
  assertion accepts that observed empty representation only with zero visible
  selected items; nonempty selections still require a matching real array.

SelectionAutoArrange passed three alternating repetitions in
experiment-selectionautoarrange-e5ff360fd7904b00b1e151097ee343be. With automatic
arrangement verified on both profiles, extra-large selection is 2,204.056 ms in
profile 6 versus 52.801 ms in profile 0; clear is 1,331.605 versus 1.400 ms.
The legacy automatic-layout selection produces 4,993 custom-draw notifications
in addition to its 5,000 selection notifications, unlike the free-layout pilot.
Modern GetItemPosition returns E_NOTIMPL. This isolates a standard performance
opportunity for automatically arranged views, without claiming free positioning.

### What can be adapted

The supported candidate is broader use of the modern profile where custom
positioning and ordering are not required. The public FVO_CUSTOMPOSITION contract
must remain explicit when those features are needed; silently selecting FVO_DEFAULT
for every icon view would remove that guarantee. A profile chosen from arrangement
requirements is a concrete follow-up, requiring validation of drag reordering,
retention of saved free coordinates, preview/cancel, focus and lifecycle transitions
before any product change. No such extension is delivered by this investigation.

A native bulk selection can reduce PIDL conversion and per-item notification work,
but the legacy manager still performs quadratic membership checks. FileSpacer's
existing Ctrl+A already reaches the native path. No tested public route removes
the demonstrated quadratic cost while retaining the current free-position view.
The matching native probes and isolated fixture form a reproducible case for a
Windows implementation fix. An optional internal optimization needs a negligible
compatibility check and a native
fallback before mutation when unavailable. Version matching alone does not qualify
its functional, lifecycle or security behavior.

Relevant public contracts:
[FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions),
[LVN_ITEMCHANGED](https://learn.microsoft.com/en-us/windows/win32/controls/lvn-itemchanged),
[IAccessible::accSelect](https://learn.microsoft.com/en-us/windows/win32/api/oleacc/nf-oleacc-iaccessible-accselect),
[SELFLAG](https://learn.microsoft.com/en-us/windows/win32/winauto/selflag),
[GetSelection](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifolderview2-getselection).

The normal Debug application build, the /W4 native helpers and the offline
DumpStacks build complete without new compiler warnings after correcting the
probe's initial macro collision, command constants and GetSelection signature.
No complete regression-suite PASS is implied. New source paths are confined to
the experimental helpers, drivers, entry-point reuse and this investigation log.
Static analysis of SelectionInternalsTests.cpp, SelectionStrategiesTests.cpp and
DumpStacks.cpp completed through both clang-tidy and Cppcheck, Windows x64 / C++14,
with real SDK 10.0.19041.0 / MSVC+ATL 14.44.35207 headers for clang-tidy and official
windows/microsoft_atl/sqlite3 Cppcheck models. All six invocations completed
successfully, without parser failure, timeout or reduced ATL coverage. Verdict:
REVIEW for new probe/style diagnostics (integer/pointer conversions at Win32
callback boundaries, signed flag masks, adjacent string parameters, .cpp reuse,
and Cppcheck's pointer-to-const suggestion). No new confirmed functional defect
is reported. Existing include/enum/RAII-reference diagnostics are comparable to
the earlier analyses. No production C++ file changes in this step. Logs:
build/reactivity/internals-analysis.

The six investigation cases 22–27 pass their recorded protocols. The first probe
compile failed on an include/main macro collision and missing SDK command names;
the next matrix preparation failed on the GetSelection signature. Both were
corrected before VM execution. The two failed accessibility assertions and their
empty-selection diagnosis remain recorded above. The final runs provide the
stated evidence; no failed or rejected candidate is counted as an optimization.

Reproduce from CMD in the experimental worktree:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionInternals -TimeoutSeconds 900
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionMatrix -TimeoutSeconds 900
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionAccessibility -TimeoutSeconds 300
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionComparisonCounts -TimeoutSeconds 300
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionCausal -TimeoutSeconds 300
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case SelectionAutoArrange -TimeoutSeconds 300
```

The two private cases deliberately refuse a different guest Shell image. They
must be re-investigated against matching images and symbols before any RVA or
signature change; changing the hash alone would not validate the instrumentation.

### Optional dispatch: compatibility cost and rejected direct binding

An optimized operation may use a nonpublic mechanism when its absence or failed
compatibility check simply retains the existing implementation. Check overhead
must be negligible. Prefer a fixed, immutable table of typed, compiled operations,
with an independent choice for selection, clear and placement. Cache process-wide
compatibility once, then validate view-specific capabilities at creation and
respect view lifetime/generation. A class hierarchy can carry state if several
backends eventually justify it; changing the dispatch shape does not establish
the safety of an internal call.

Security qualification also applies: no executable address derived from folder
contents, user-controlled identifiers or other untrusted data; no weakening of
process protection for a runtime optimization; no selection of a backend solely
because its code address or signature happens to match. Typed function pointers
to compiled functions and normal C++ virtual dispatch both require correct
signatures and lifetimes. The experimental instruction patches remain isolated
causal probes, not product backends.

A subsequent direct-method prototype tested a two-entry dispatch with the native
clear as its default. An in-memory PE/PDB fingerprint, entry bytes and object
checks selected an exact-build internal ClearSelection binding. The unknown-path
arm retained the original call. No instruction was rewritten by this prototype.
The run experiment-selectiondispatch-3171ba53ca3c476ab873b33493556fc9 is FAIL:

* Forced absence: first check 1.500 microseconds; repeated dispatch choice
  0.046 microseconds. Original selection/clear and all invariants pass.
* Detected candidate: first check 24.300 microseconds; repeated compatibility
  check 1.644 microseconds (1,000 iterations). These are VM samples, not a
  startup guarantee on another machine.
* Direct cache clear followed by native clear returns in 95.184 ms and leaves
  zero visibly selected items, but GetSelection(FALSE) still returns the previous
  5,000-item array. The consistency assertion correctly rejects it.

This demonstrates that cheap detection and fallback alone are insufficient.
The missed Shell cache/notification contract remains unresolved, and the direct
object-offset/method binding has not qualified for production security or lifetime
handling. No improvement is claimed from this failed candidate. Its source and
logs are retained only under ignored build/rejected-selection-dispatch and the
run directory; it was removed from the active experimental drivers. The validated
six-case probes remain unchanged. No product backend or new runtime abstraction
was added for a candidate that fails qualification.

### Remaining functional qualification, 6 November 2026

The affected Debug application regression groups were executed in AgentWin10:

| Group | Final result | Host report directory under build/regression |
| --- | --- | --- |
| 28-apercu-affichage | 4/4 PASS | remaining-preview-20261106 |
| 18-vue-shell-native | 2/2 PASS | remaining-18-vue-shell-native-20261106 |
| 07-fenetres | 7/7 PASS | remaining-07-fenetres-20261106 |
| 14-disposition | 1/1 PASS | remaining-14-disposition-20261106 |
| 17-fermeture-interactions | 1/1 PASS | remaining-close-20261106-r2 |
| 19-commandes-shell | 6/6 PASS | remaining-19-commandes-shell-20261106; remaining-drag-20261106-r2 |

The 21 distinct cases pass; this is not the entire regression suite. The first
closure and drag/drop runs failed before their gestures because their fixtures
required SysListView32. Both now reuse the existing bounded public UI Automation
item/background target helpers. The real right-click/menu/Alt+F4 and move/Ctrl-copy
gestures and their process-exit, bytes, identities and view assertions remain.
The reruns actually exercised those operations. No product source was changed.

The new ArrangementTransitions native probe uses the production ShellViewHost,
ShellViewIcons and ShellViewSelection functions against the real guest Shell.
It does not enable a new product profile policy. Run
experiment-arrangementtransitions-f545afb0f8d44c5883d9c34323f0e75d records:

* All twelve roundtrips between classic free placement and the six modern
  automatically arranged representations pass: extra-large, large, medium,
  small, tiles and content, each with keyboard focus inside and outside the view.
* Every return checks all 5,000 saved coordinates, the two selected identities,
  GetSelection(FALSE), logical and keyboard focus, and rejection/consumption of
  a snapshot from an obsolete view generation.
* The additional full-selection roundtrip preserves all 5,000 selected identities
  and coordinates, the selection array, logical focus and keyboard focus.

This functional fixture explicitly Refreshes each configured view and waits for
a new public EnumDone event before restoring state. A representation change alone
does not guarantee a new enumeration event. Two initial versions of the probe
incorrectly waited for callbacks after a representation-only change and timed
out; those runs are not candidate failures or successful qualifications. The
passing roundtrips establish restoration after controlled reenumeration, not a
refresh-free product transition or a performance result. The application preview
tests above cover the current product policy, not an integrated six-view policy.

The initial reorder attempt in that full run failed its foreground precondition.
The isolated rerun now activates the owned test window by a verified title-bar
click when SetForegroundWindow is refused. A subsequent WinEvent-only gate also
proved unsuitable: Windows does not itself emit the drag/drop start/end events;
the application must do so ([Microsoft event constants](https://learn.microsoft.com/en-us/windows/win32/winauto/event-constants)).
These events remain diagnostics. The final gesture checks observed mouse capture,
release/idle state, the exact selected source identity and unchanged fixture
identities, then compares the visible positions of twenty medium-icon items.
The reference must actually reorder before the comparison can qualify anything.

Final reorder-only run:
experiment-arrangementtransitions-e686ef60ced0414b98a114d02095ad93,
VM run c6f9cea4efeb4eef96765450f426dcfe:

| Automatically arranged medium icons | Capture and release | Source and 20 identities | Visual order changes |
| --- | --- | --- | --- |
| Classic profile 6 | PASS | PASS | yes |
| Modern profile 0 | PASS | PASS | no |

The qualification therefore correctly returns FAIL: the modern arranged profile
loses manual drag reordering that the classic profile supports. This is a real
behavioral difference, not a gesture setup failure. It does not invalidate the
passing saved-free-coordinate roundtrips. Other icon sizes/tiles/content were not
physically reordered in this comparison. Do not integrate a blanket modern
arranged-view policy on the strength of the timing gains and restoration tests.
Preserving this interaction still needs a solution or an explicit product choice.

Reproduce from CMD in this worktree:

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case ArrangementTransitions -TimeoutSeconds 900
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case ArrangementTransitions -ReorderOnly -TimeoutSeconds 300
```

ReorderOnly skips the already validated transition portion; it does not disable
the reference or candidate assertions. The probe deliberately fails while this
interaction differs. No instruction patch, object-offset call, private production
backend, security relaxation or new dependency is introduced.

The Debug application build and the final native helper compile with MSVC /W4
without new compiler warnings. Both clang-tidy and Cppcheck completed on the new
ArrangementTransitionsTests.cpp, Windows x64/C++14, using SDK 10.0.19041.0 and
MSVC/ATL 14.44.35207 for clang-tidy and official windows/microsoft_atl/sqlite3
models for Cppcheck. No timeout, parser failure or ATL coverage reduction.
Static verdict REVIEW: SDK flag-combination enum warnings, signed flag operations,
intentional .cpp test reuse, adjacent scalar parameters, name shadowing and iterator
initialization style; Cppcheck also warns about the fully initialized Mode aggregate members.
No confirmed new functional defect in the probe is left uncorrected. The unchanged
PlacementTests.cpp shadow warning is reported separately by the same include path.
Probe corrections made during validation include the ATL final-class limitation,
the SDK's non-const outer pointer parameter, explicit readiness, window ownership,
activation and mouse-capture evidence. Logs: build/reactivity/transitions-analysis.
The static REVIEW is independent of the intentional candidate qualification FAIL.

### Can ordering alone preserve the fast profile? 6 November 2026

The documented options can be requested independently: FVO_CUSTOMPOSITION=2 and
FVO_CUSTOMORDERING=4. The latter explicitly enables dragging items to reorder
them. However, [IFolderViewOptions](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/nn-shobjidl-ifolderviewoptions)
documents that requesting the older view's capabilities currently selects that
older view; a future Windows implementation may satisfy the same request using
the newer view. A successful flag request is not evidence that the fast engine
was retained. [FOLDERVIEWOPTIONS](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl/ne-shobjidl-folderviewoptions)
is the capability contract; do not substitute FVO_VISTALAYOUT as a shortcut.

ArrangementTransitions -ProfileMatrix now compares all four combinations with
the same existing real twenty-item drag fixture and a separate 5,000-item timing
fixture. Medium icons, automatic arrangement, 800x400 requested view bounds and
the same MSVC /W4 Debug helper are used for every profile. Selection and clear
are timed around the public calls only, with three repetitions. These are VM API
durations, not full presentation timings or a free-workstation comparison.
Selected identities, GetSelection arrays, clear state and logical/keyboard focus
are verified outside the timed regions.

VM run cc0080e78dcb4f92998b1818b6331c2a passes the comparison protocol. Medians:

| Options | Native reorder | Select 5,000, ms | Clear, ms |
| --- | --- | ---: | ---: |
| Position + ordering (6) | yes | 1061.569 | 777.668 |
| Ordering only (4) | yes | 1030.208 | 749.378 |
| Position only (2) | yes | 1074.096 | 821.275 |
| Default (0) | no | 152.665 | 2.461 |

Ordering alone restores the interaction but also the expensive selection path.
The position-only row describes this guest's observed behavior; use the explicit
ordering contract when that interaction is required. No measured combination
provides both the fast operation times and native manual ordering. Matrix PASS
does not overturn the original candidate qualification FAIL; the original strict
reorder assertion remains in the default and reorder-only modes.

The first new driver attempt failed before invoking the native executable on a
PowerShell branch separator; it was corrected. A second dispatch encountered the
VM mutex held by a request from another checkout. Its already prepared package
was subsequently dispatched through the existing Invoke-HyperVRunner interface
once the other controller completed. No competing request or lock was terminated
or bypassed. Authoritative results are under build/regression/vm-cc0080e78dcb4f92998b1818b6331c2a.

```bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Run-Experiment.ps1 -Case ArrangementTransitions -ProfileMatrix -TimeoutSeconds 900
```

ProfileMatrix and ReorderOnly are mutually exclusive. No product policy changes
are delivered by this investigation. Keeping the fast profile for fixed-sort
views while using the classic profile for custom placement/order remains a hybrid
fallback proposal, not a repair of the modern view's native ordering. Switching
the view in the middle of a native drag is not validated and must not be treated
as transparent. Keeping identical gestures and fast custom ordering would require
another prototype, such as explicit order management through a folder provider,
with its drop delegation, persistence, cancellation and performance qualified.

The Debug application and final native helper build without new /W4 warnings.
Both required analyses completed on ArrangementTransitionsTests.cpp with the
same Windows x64/C++14 toolchain and real headers/models stated above. Baseline
logs and source were preserved under build/reactivity/ordering-analysis-before;
final logs are under build/reactivity/ordering-analysis-after. Verdict REVIEW for
new signed flag-expression and adjacent-path-parameter style diagnostics; no new
confirmed functional defect or missing analysis coverage. No private calls,
executable patches or additional dependencies were introduced.
