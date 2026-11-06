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
