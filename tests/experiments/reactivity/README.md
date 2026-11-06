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

The Desktop probe includes an explicit `recover` mode:

```bat
DesktopPlacementTests.exe C:\FileSpacerTests\<run>\output\regression\36-reactivity-prototype\04-DesktopPlacement recover
```

Run recovery in the guest with the original output directory if its watchdog
interrupts the case. The binary backup identifies the original Desktop PIDLs,
positions, selection, flags and the GUID prefix of the owned fixture. Recovery
never closes the Desktop or removes files outside that prefix.
