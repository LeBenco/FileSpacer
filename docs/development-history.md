# Development history and validation

This handoff consolidates the separate development and validation sessions on
25–26 October 2026. Current code, Git state and published releases take precedence
over the older snapshots retained below. In particular, the initial search
integration was once uncommitted, but it has since shipped in 1.1.4.

## Consolidated status — 26 October 2026

The latest published application is **1.1.7**, tag `v1.1.7`, commit `99ff6f7`.
At the start of this documentation audit, local `main` and `origin/main` both
resolved to that commit and the main working tree was clean. Documentation-only
commits may follow the release tag; they do not change the released executable.
Another session subsequently added an uncommitted installer option. Its pending
validation and a separate folder-icon design discussion are recorded below;
neither is part of the published 1.1.7 installer.

The public release API was checked during this audit: versions 1.1.2 through
1.1.7 are published releases, each with its versioned installer and update
manifest. Local tags resolve as follows:

| Version | Application commit | Delivered scope |
| --- | --- | --- |
| [1.1.2](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.2) | `9550f52` | Optional saved-window-state locking, native lock badge, reopened scroll anchor and simplified crash reporting. |
| [1.1.3](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.3) | `f257a2c` | Configurable window gestures and opaque native labels for single selection. |
| [1.1.4](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.4) | `b222a35` | Everything, persistent per-folder search history, automatic suggestions and cached executable icon. |
| [1.1.5](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.5) | `fe728f2` | Native Alt-double-click correction, stronger physical-input tests and isolated Everything/bootstrap handling. |
| [1.1.6](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.6) | `45f4922` | Ancestor-removal dispatch, unavailable Jump List entries, initial smart placement, maintenance relocation and wider defaults. |
| [1.1.7](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.7) | `99ff6f7` | Exact hybrid placement and the renamed General option. |

The current runner discovers **111 cases: 19 Internal and 92 UI**. The last
complete successful suite recorded below was **109/109 on 1.1.5**. Later work
added group 26 and used targeted checks: 19 Internal and 13 real Shell cases
passed for the hybrid placement. This is not a claim that all 111 cases were
rerun successfully on 1.1.7. Historical failures and external obstructions remain
identified separately.

Detailed coverage is split between this chronology, the [user guide](user-guide.md),
[native view architecture](native-shell-view.md), [single-selection validation](validation-single-selection-labels.md),
[shortcut policy](window-shortcuts.md), [search implementation](search-integration.md),
and [independent placement study](../tests/placement-simulation/RESULTS.md).
The separate client-test worktree and its unfinished isolation checks are recorded
at the end; they are not part of the released application.

## 2026-10-25 — Earlier delivered work: locking and selected labels

Saved-window-state locking shipped in 1.1.2. It is globally optional and disabled
by default. Locking captures geometry and view settings; temporary changes remain
possible, while individual icon positions continue saving. Unlocking captures the
current visible state. Normal retention/quota rules still apply. Global disabling
requires confirmation and clears locks across processes without deleting other
folder state. The Small & Flat CC0 lock icon, native multi-resolution rendering,
DPI/high-contrast handling and accessible state are retained. Group 22 covers the
state transitions and native badge renderer. These are delivered features, not
the undecided prototype described by older planning material.

The 1.1.2 release also includes the Details/List top-row scroll anchor and removes
the obsolete attribution, preference change and automatic restart from crash
reporting, while retaining minidumps/reports. The 24 October native-view migration
and its later fixes remain the architectural baseline.

The later selected-label work shipped with 1.1.3. In Icons mode, an optional
`NM_CUSTOMDRAW` adaptation fills the background of the one actually selected
item, then lets the native control draw the text. Multiple/empty selection,
focus-only items and other modes retain native drawing; no custom text pane or
multi-selection overlay was delivered. The preference updates already-open
windows. The real native-control rendering checks, selection/focus/position
preservation and static **REVIEW** findings are retained in
[the validation report](validation-single-selection-labels.md). Earlier
[drawing probes](native-label-draw-investigation.md) and
[multiple-selection/pane experiments](native-multi-label-investigation.md)
remain investigations, not product features. Group 23 also rejects empty render
captures instead of accepting invalid image comparisons.

These additions complement the historical records below; they do not turn their
limited DPI/theme coverage into a general visual validation.

## 2026-10-25 — Window shortcuts and release 1.1.3

### Shortcut scope and implementation

An independent, disposable Win32 prototype established the interaction before
integration into FileSpacer. The production implementation adds the native
**Shortcuts / Raccourcis** settings tab with exactly two actions: moving and
resizing folder windows. Item Properties is not a configurable action.

- Each action can be disabled independently. Alt, Ctrl and Shift can be combined
  with left, middle or right drag. AltGr maps to Windows' Ctrl + Alt state.
- Middle drag is allowed with any modifier combination, including none.
  Left/right drag requires Alt, preserving ordinary selection and context menus.
  Modifiers match exactly; identical enabled bindings conflict. Disabled actions
  retain their previous modifier/button selection.
- Both sections start collapsed. A checkbox beside the title enables the action
  without expanding it; its visible caption is empty but its localized accessible
  name remains available. The title/chevron opens one shared editor, repositioned
  beneath the selected section. There is no duplicated settings editor.
- The native Windows theme is retained: uniform system background, aligned
  header/content frames and Segoe MDL2 chevrons. Expansion animation follows the
  system animation preference. No WinUI dependency or global widget theme was added.
- Direct input closes on button release, even for an invalid choice. The
  candidate remains visible; a red warning replaces the general explanation in
  a fixed slot, and Apply is disabled. Default resets only the selected action;
  Revert restores this page's last applied pair. Apply/OK/Cancel use the native
  property-sheet contract. Conflicting choices can be exchanged atomically.

`src/WindowShortcuts.h` defines the binding model, defaults and shared validation.
`Settings.h/.cpp` stores both bindings atomically in the versioned
`WindowShortcuts` registry DWORD; malformed data falls back to defaults.
`ShortcutSettingsPage.cpp`, `SettingsDialog.*`, resources and the Makefile
integrate the shared editor and localized page. Existing windows read the
bindings when a gesture begins, including across application processes.

`FolderWindow.*` detects the selected button/modifier combination using the
existing public Shell/MSAA targeting rules. The file-list background and items
participate; rename/text controls, headers, scrollbars, toolbars and menus keep
their normal input. Left-button movement keeps Windows' native move loop.
`WindowResizeGesture.*` supplies one capture engine for middle/right movement
and resizing with any button, retaining native size limits, cancellation and
geometry persistence. The combinations do not have separate action implementations.

The former application-specific Properties command was removed. A genuine
double-click delivers any first click deferred by the window gesture before
letting Explorer handle its normal double-click. Window/view-generation checks
guard reentrant calls. This preserves Explorer's actual target and selection,
rather than retaining the former custom selection behavior.
See [the shortcut implementation note](window-shortcuts.md).

### Executed shortcut checks

Debug MSVC x64 / NMAKE `/W4` builds passed without new warnings.
The final focused `tests.cmd -Pattern 24-*` run passed all three cases:

| Check | Observed result |
| --- | --- |
| 24/01 — native page and storage matrix | 2,304 pairs per language: 1,584 accepted and 720 rejected without changing storage; 86,990 assertions each in English and French. Includes exact matching, disabled actions, corrupt data, conflicts, swaps, Revert, per-action defaults, accessible names and all 24 direct-input choices. |
| Shared capture engine within 24/01 | 96 actual HWND geometry/completion/Escape/cancel scenarios per language; this is separate from Shell integration. |
| 24/02 — real Shell gestures | All 32 enabled action/binding choices on background and an unselected item across two processes: 64 physical gestures. Checks geometry, selection, the other window, live settings, disabled actions and the real modal Apply/Cancel contract. |
| 24/03 — native Properties reference | Target and selection match the view with shortcuts disabled, with defaults, and with Alt + left resize enabled. |

The general regression suite was initially started too broadly and interrupted
at the user's request. Its isolated test registry/database were restored;
that partial suite was not declared passed. Validation was then limited to the
new shortcut cases. In particular, the load timeout encountered there was not
claimed resolved in this session; the later controlled investigation is recorded
[below](#load-investigation-and-observer-correction).

### Shortcut static analysis: REVIEW

clang-tidy and Cppcheck completed on `Settings.cpp`, `SettingsDialog.cpp`,
`FolderWindow.cpp`, `WindowResizeGesture.cpp`, `ShortcutSettingsPage.cpp`
and `ShortcutTests.cpp`, covering modified headers through those units.
Configuration: Windows x64, C++14, Debug/Unicode macros, real MSVC/ATL
14.44.35207 and SDK 10.0.19041.0 headers for clang-tidy; official
`windows`, `microsoft_atl` and `sqlite3` models for Cppcheck, without full
system headers. No parser failure or reduced ATL coverage occurred.

Baseline comparison used commit `00f03b6`. REVIEW findings concern enum sizes,
functions/pointers that could be static/const, Win32 callback parameter
similarity, message pointer/integer conversions, macro bit operations and a
theoretical exception escape in the test main. Existing SDK FOLDERFLAGS
diagnostics were distinguished from new findings. An array-bound warning in
button-description formatting received an explicit range guard; reanalysis
removed that diagnostic. No confirmed new functional defect remained.
These historical results do not cover later edits made by the search session.

### Commits and publication

Commit times below are Europe/Paris (UTC+01:00 on this afternoon):

| Time | Commit | Purpose |
| --- | --- | --- |
| 14:40:38 | `2ad5fc0` | Add configurable window gestures to Shortcuts settings. |
| 14:40:46 | `c795cff` | Test all window shortcut combinations and native Shell gestures. |
| 14:40:48 | `53ac1c2` | Document window shortcut policy and focused validation. |
| 15:23:40 | `f257a2c` | Prepare FileSpacer 1.1.3 release: both version macros and changelog. |

The user subsequently authorized pushing and publishing. `main` and the
annotated `v1.1.3` tag were pushed using the existing `release.sh` workflow.
The version-only Debug build reported 1.1.3 without new warnings; both analyzers
were run on version-header consumers, with unchanged/preexisting diagnostics.
GitHub Actions built Release and NSIS, checked the executable/tag version match,
and published [FileSpacer 1.1.3](https://github.com/LeBenco/FileSpacer/releases/tag/v1.1.3).

The [publication run](https://github.com/LeBenco/FileSpacer/actions/runs/40202034550)
completed successfully. Both `FileSpacer-1.1.3-install.exe` and
`FileSpacer-update.txt` were present. The downloaded installer was 1,303,978 bytes;
its SHA-256 matched the update manifest:
`fbc6c21c268d0977e075a8893a8542c19f5b498fd378d8e11a5b283b132897a2`.
No later search/Everything changes are included in that release.

During the later cleanup, raw shortcut run folders and temporary analyzer
scripts/logs were removed. The useful regression sources, the historical
`build/shortcuts-dev/validation.md` summary and release-verification artifacts
were retained. That earlier summary predates publication; the publication status
above supersedes its original “no push/tag/publication” statement.

## 2026-10-25 — Search and Everything, after release 1.1.3

The following record consolidates the later integration, load investigation
and final no-chevron interaction. Their build/test outcomes belong to that
later working tree, which remained uncommitted and unpublished.

### Scope and implementation

The existing local name filter, instant-search debounce, registry settings,
SQLite folder states and conservative expiration policy were retained. The source
tree and a read-only snapshot of the existing database contained no Everything
integration or search-history storage. That database used schema 4 and contained
`folder_state`, `closed_windows` and SQLite's sequence table; it was not migrated
or edited during this work.

Added the automatic suggestion list in SearchBox, explicit-submission handling and deferred
history saving in FolderWindow, schema 6's separate history table and deletion
trigger in FolderStateStore, settings in Settings/SettingsDialog, the external
launcher/detector in Everything, and matching EN/FR resources. ItemWindow exposes
its existing state key through a protected const accessor. The Makefile links the
new source. The search field keeps its original magnifier and has no chevron.
Focus/click opens the list without taking the edit's focus; Enter remains local
until an entry is explicitly selected. Existing UI tests identify the original
magnifier by its control ID.
Applied history limits travel with settings notifications, including their
normal/test namespace. This prevents delayed notifications from missing a clear
or reduction and later restoring an old cached history, while preserving test
isolation.

Assumptions: current folder with descendants is the default scope, Everywhere is
the other mode, and history records successful explicit submissions, recent-query
reuse and successful external launches. Typing alone never records a query.
Everything uses its documented `-search` parameter and triple quote escaping.
See [the implementation note](search-integration.md) and [user guide](user-guide.md).

### Load investigation and observer correction

The controlled comparison below predates the subsequent no-chevron interaction
change. That change also passed the same load case in 24.5 s; its latest
functional and static checks are recorded in [the validation report](#search-executed-checks).

The 5,000-file timeout was reproduced with both the original application and the
modified application. Concurrent Computer Use accessibility inspection was the
cause of the observed slowdown. After the user authorized temporarily stopping
that shared helper, both builds passed the unchanged load scenario in about
24 seconds. These trials show no material regression in this scenario; they are
not a general performance guarantee.

#### Comparison

The baseline application came from commit `f257a2c`, extracted into the ignored
`build/sp-base` directory. Both applications used the same MSVC/NMAKE Debug
configuration and the same current regression harness, fixtures and assertions.
The baseline's single search button was recognized by the shared helper; the
current build's original magnifier was identified by its control ID. Neither
application's search commands were bypassed.

| Condition | Original application | Modified application |
| --- | --- | --- |
| Active concurrent accessibility inspection | Timeout at 300 s | Timeout at 300 s |
| Helper stopped, original names reader | PASS, 23.9 s | PASS, 24.0 s |
| Helper stopped, corrected names reader | PASS, 24.03 s | PASS, 24.05 s |

Each successful run opened the folder in three successive processes, checked all
5,000 exact names, searched for one result and no result, cleared the filter,
refreshed, and closed normally. The existing watchdog, expected identities and
stability requirements were retained. Production preferences and folder states
were not used: the existing runner isolated and restored its `/test` data.

With the original reader and no helper, the median native read of all 5,000 names
was 612.5 ms for the baseline and 610 ms for the modified build (18 matching
observations each). Folder openings took 0.93–1.13 s. With active inspection,
opening took about 62 s in both builds and the first full names read did not
complete before the outer watchdog, before any search was submitted.

#### Attribution

Sampled application stacks were processing UI Automation's `BulkFetch::Traverse`
and `ListViewSubItemProxy::GetPropertyValue` through the normal message loop.
A bounded UIAutomationCore ETW capture confirmed sustained provider activity.
Read-only Windows Wait Chain Traversal then found two threads of the shared
`codex-computer-use.exe` process waiting through `SendMessage` on FileSpacer's
GUI thread. No cyclic wait was reported. This identifies an active caller rather
than inferring contention merely from the helper's presence.

After the authorized stop, no helper process was observed at the recorded test
phase boundaries and both applications passed, even before changing the names
reader. The application C++ sources were not changed during this investigation.
The tests do not stop accessibility tools automatically.

#### Observer race corrected

A subsequent ordinary search regression exposed `E_BOUNDS` in the existing
`ViewProbe.Names()` test helper. It read a count and then indexed the live view
while asynchronous filtering could shrink that view. The reader now uses public
`IFolderView::Items(SVGIO_ALLVIEW)` / `IEnumIDList::Next`, already used elsewhere
in the helper. It still reads actual Shell identities, propagates genuine COM
errors and checks the same stable result. It does not repeat the search command
or substitute an expected result.

The corrected reader passed the comparison above, all six preference/search
cases and both real history/Everything UI cases. Five additional successive-filter
runs with detailed timing disabled passed in 11.8–13.4 s; the layout case also
passed (12.6 s).

Optional `FS_SEARCH_TIMINGS=1` adds `search-observations.jsonl`; the load case also
writes `load-phases.jsonl` with elapsed time, application CPU and helper presence.
These observations do not alter the test deadlines or pass criteria.

#### Performance investigation record and cleanup

The measurements and diagnoses above summarize actual runs. At the user's
subsequent request, raw logs, ETW capture, process/thread samples, comparison
source/build copies and temporary launchers were removed. This report preserves
their conclusions and measured values; those raw artifacts are no longer
available for inspection. Useful regression sources and installed tools remain.

The first timing-disabled attempts could not create the isolated test registry
key in the sandbox and ran no scenarios. Their outcome was NOT_RUN; the
authorized reruns used the required access. They were not application failures.
The original timeout and observer errors are described above; cleanup does not
change their status or replace them with the later successful runs.

After all test windows had closed, the supported `sky.list_apps` API successfully
restarted the shared Computer Use helper, confirmed by a successful API response
and a new helper process. No helper was left stopped by the investigation.

### Search executed checks

Debug builds used MSVC 14.44.35207 / NMAKE / `/W4` and Windows SDK 10.0.19041.0.
They succeeded without a new warning. No installer or release build was requested.

| Check | Result |
| --- | --- |
| Internal suite | Final complete rerun: 18 passed. The retention test's old schema-5 expectation was updated to schema 6, with added quota/history assertions. |
| Preferences and layout, groups 13–14 | 7 passed, including instant search, successive filters with equal result counts, and preservation of saved state under a filter. |
| Close after queued refresh/search, 07/04 | Passed: eight actual commands followed by normal closure. |
| Native Shell/SQLite persistence, group 18 | Passed. |
| New group 25 | All 5 passed: 2 internal cases plus 3 real UI cases. Includes schemas 4/5, delayed notifications, namespace separation and the no-chevron keyboard/mouse path. |
| 5,000-file load, 20/01 | PASS after isolating concurrent accessibility inspection: original and modified builds both pass in about 24 s, before and after the test-reader correction. Original timeouts are retained. See [the investigation](#load-investigation-and-observer-correction). |

Across the task, 31 distinct selected cases have passed. The no-chevron follow-up
ran 14 relevant cases: all five integration cases, six preference/search cases,
layout, close after queued search, and the load case (24.5 s). The integration
group was repeated after the pointer-selection protection; the new UI case was
repeated after the final lifetime safeguard (7.8 s). Assertions and deadlines
were retained. The full 109-case suite was not run.

The new tests cover separate histories for two folders, actual close/reopen,
newest-first ordering and repeated-query reuse, 0/5/20, invalid 21, trimming closed
folders, every menu-section combination and empty history, local recent-query
reuse with Everything enabled, instant search without recording keystrokes,
transaction rollback, migration, rename, qualified expiration, temporary failure
recovery, confirmed deletion and quota cleanup. Unrelated legacy search data and
existing view/position/icon data survive the storage tests.

Before installation, the detector's explicit absence assertion passed on the real
machine. Everything 1.4.1.1032 x64 was then installed as an official portable copy
in `%LOCALAPPDATA%\Programs\Everything`. Its SHA-256 matched voidtools' published
value and its Authenticode signature was valid. No FileSpacer production setting
was changed to enable it.

The real application test used an isolated Everything INI/database indexing only
fixtures. It checked automatic detection, a manual executable path containing
spaces and an accent, exact visible queries with quotes, backslashes, metacharacters
and a leading switch-like token, both scopes and OR grouping. Actual result names
confirmed descendant matches and exclusion/inclusion of an outside match.
Launch failure displayed an explicit dialog and preserved the local field/filter;
successful launches cleared them. These were real Everything executions, separate
from the internal argument-construction and Win32 error checks.

The existing runner backed up/restored the `/test` registry namespace and database.
Fixtures and the external index were isolated. The complete 109-case suite was not
run; checks were selected for the modified behavior and its existing search paths.

### Search static analysis: REVIEW

clang-tidy 23.1.4 and Cppcheck 2.22.0 both completed on the final modified C++
sources and relevant header consumers: Everything, SearchBox, FolderWindow,
FolderStateStore, Settings, SettingsDialog, ItemWindow, EverythingTests,
SearchHistoryTests and RetentionTests. Production uses C++14; the existing retention
test uses its C++17 build configuration. Windows x64, static Debug CRT and the
appropriate application/storage-test macros were used. clang-tidy had real SDK,
MSVC STL and ATL headers. Cppcheck used its official windows, microsoft_atl and
sqlite3 models, without full system headers. No parser failure or ATL analyzer
crash reduced coverage. No modified header or source remains insufficiently
analyzed.

Baseline comparisons used the original sources and the same analyzer settings.
No confirmed new functional defect remains. The following new diagnostics are
left for review, without changing code merely to silence them:

- Settings: `clang-analyzer-unix.cstring.UninitializedRead` at the existing
  `CopyMemory` helper, newly reached by the empty Everything path default. The
  trace treats `lstrlenW(L"")` as potentially negative; the actual Windows call
  returns zero. Probable false positive; retained as REVIEW.
- SearchBox: `bugprone-branch-clone` for two Tab-navigation branches targeting the
  same button; Cppcheck `intToPointerCast` for documented Win32 child-control IDs
  passed through the `HMENU` parameter. These are stylistic/contract warnings.
- Settings header: `performance-enum-size` on the DWORD-backed EverythingScope,
  matching its registry representation.
- New storage test: constant-default assertion, helper argument similarity and
  theoretical exception escape while reporting a test failure. These are test
  style diagnostics. Existing enum-size and SQLITE_TRANSIENT warnings are
  distinguished from new findings.

There is no analyzer disagreement establishing a new functional defect. The
conservative REVIEW verdict is independent of the successful build and functional
tests. No source change was made solely in response to an ambiguous/style
diagnostic.

The no-chevron follow-up ran both analyzers on SearchBox and FolderWindow plus
ItemWindow and EverythingTests as header consumers, with a snapshot of the
pre-change sources for comparison. The final SearchBox implementation was
reanalyzed after its last change. All runs completed without a parser failure or
reduced ATL coverage. Other modified C++ files retain the complete analyses
reported above, since they did not change in this follow-up. No file is
insufficiently analyzed. Additional REVIEW findings are callback parameter
similarity and pointer/integer round trips required by Win32, Cppcheck's missing
default initializer on Suggestion (every entry is explicitly aggregate-initialized),
and a pointer-to-const style suggestion in the new selection receiver. No new
confirmed functional defect remains.

### Search validation record and cleanup

The build, test and analyzer outcomes above summarize actual executions. The
user subsequently requested removal of temporary files. Raw local run folders,
analyzer output, baseline source copies, dumps, ETW traces, temporary launchers
and download archives were removed. This consolidated record, including
[the load investigation](#load-investigation-and-observer-correction), preserves the results, diagnostics
and comparison conditions; the removed logs are no longer available for inspection.

The performance investigation identified the shared Computer Use helper as an
active accessibility caller by Wait Chain Traversal and ETW, reproduced the
timeout on the original application, and completed controlled comparisons after
its authorized temporary stop. The original extended run remains a failed test;
later PASS results came from complete new runs with unchanged assertions and
deadlines.

The follow-up also corrected an independent race in the C# test reader exposed
by successive filtering: enumerating actual Shell items replaces indexing a live
view using an earlier count. No application C++ changed during that follow-up;
the static-analysis coverage and REVIEW findings above remain unchanged.

The no-chevron follow-up completed all five integration cases (including actual
Everything), six preference cases, layout, close and load, plus both analyzers
and their before/after comparisons. The retained popup UI test checks real
click/focus, Ctrl+F, reopening by Down, caret focus,
typing with instant search, no selection on hover, separator skipping, literal
ampersands, native Enter, Escape without erasing, Tab, mouse recent-query reuse
and closing the owner. The list never launches Everything on focus alone.

Regression sources, including the keyboard/mouse preferences tests, remain in
the repository. Installed analysis tools, the official Everything installation,
final application binaries and existing release archives were retained. Production
folder states and preferences were not reset. This cleanup changed no C++ or test
source, so it did not require another build or analyzer/test run. The historical
REVIEW verdict remains unchanged.

No commit, push, tag or release was created for the search/Everything task.
The previously published shortcut release is recorded in the earlier section.

Proposed changelog line:

> Show optional Everything search and per-folder recent searches automatically
> when focusing the search field, without an extra button, while retaining native search.

## 2026-10-25 — Dynamic Everything icon in search suggestions

The user requested Everything's own icon before the external-search label, with
dynamic extraction only if search responsiveness was preserved. The user then
specified manual visual verification only, rather than rerunning the functional
suites, and retained the cache option once metadata checking proved cheaper.

### Implementation

- `Everything.h/.cpp`: extract the first icon group of the configured executable
  with documented Windows APIs. `LoadLibraryExW` maps data/image resources without
  executing the executable or loading its imports. `LoadIconWithScaleDown` prefers
  an exact requested resource size; the list requests 16 DIP at the existing UI
  DPI. The installed Everything contains a 16 x 16 variant, and the native probe
  confirmed a 16 x 16 result. No icon ID, copied ICO asset or version is hardcoded.
- `SearchBox.h/.cpp`: lazily queue detection/extraction on the Windows thread
  pool when the Everything entry is first shown. A per-field memory cache keeps
  only the owned HICON. Reopening checks the resolved path, creation/modification
  times and file size in the background; unchanged resources are reused. Settings
  or requested pixel-size changes invalidate the cache. Mapping handles are released immediately.
- `FolderWindow.cpp`: pass the current configured executable path to SearchBox
  when creating/updating its settings. Automatic detection also runs off the UI
  thread when no manual path is configured.
- Drawing uses a nonblocking cache lock, reserves the icon's space from the first
  frame and repaints after asynchronous completion. There is no filesystem access
  in the paint/input path, global startup extraction, persistent cache, file
  watcher or version-resource scan. Unavailable icons leave the text actionable.
  A cancelled/closed window is never retained by the worker; shared state owns
  resources until completion. Publication is cancelled on destruction/settings
  replacement, without waiting for file I/O.

A usual executable update refreshes the icon when the list is next opened, even
in an already open FileSpacer window. A change that preserves path, both file
timestamps and size is outside this lightweight invalidation mechanism. No real
Everything upgrade was performed for this follow-up.

Contracts consulted: [resource-only loading](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw),
[native icon-size selection](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-loadiconwithscaledown),
[resource enumeration](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-enumresourcenamesw),
and [thread-pool work](https://learn.microsoft.com/en-us/windows/win32/api/threadpoollegacyapiset/nf-threadpoollegacyapiset-queueuserworkitem).
Signatures were also checked against the local Windows SDK.

### Measured cost and validation

A disposable MSVC x64 probe called the production extraction function with the
same Common Controls v6 dependency as FileSpacer, on the installed local
Everything 1.4.1.1032 executable. The 500 repeated native measurements gave:

| Operation | Median | 95th percentile |
| --- | ---: | ---: |
| Creation/modification dates and size, one GetFileAttributesExW call | 0.0362 ms | 0.0601 ms |
| Extract and release the 16 x 16 icon | 0.4560 ms | 0.8662 ms |

Metadata was about 12.6 times cheaper. The first extraction in the probe process
took 1.72 ms; a 5.89 ms maximum was observed among repeated extractions. These
are measurements on this workstation with its local filesystem/cache, not a
guarantee for uncached disks or network paths, hence the background worker.

An additional 200 samples included the real automatic detector: detection alone
had a 0.1559 ms median; detection plus metadata took 0.1763 ms, compared with
0.5923 ms for detection plus extraction. The cache therefore remains useful even
with automatic path resolution included. The preliminary managed probe's
Common Controls v5 binding issue was isolated to that probe; native v6 extraction
and the built application succeeded.

Debug MSVC/NMAKE `/W4` build: succeeded without a new warning. Functional suites
were **not rerun**, as requested. A temporary setup case reused only the existing
runner's `/test` backup/restoration to open a manual window. It had no automatic
functional UI assertion; its normal exit merely confirmed session cleanup.
Both Computer Use interfaces failed to bind the Debug window with an application
identity error. The human user then explicitly confirmed **the Everything icon
was visible before the label and the test window was closed**. This is a manual
human confirmation, not a claimed screenshot inspection by the coding agent.
The runner exited normally and restored its isolated preferences/database.

### Static analysis: REVIEW

clang-tidy and Cppcheck independently completed before/after comparisons for
Everything, SearchBox, FolderWindow and the ItemWindow header consumer, using
Windows x64/C++14 Debug macros, real SDK 10.0.19041.0 and MSVC/ATL 14.44.35207
headers for clang-tidy, and the official windows/microsoft_atl/sqlite3 models for
Cppcheck. No parsing failure, analyzer crash or reduced ATL coverage occurred;
no modified source/header is insufficiently analyzed. No confirmed new defect
was found. New diagnostics left for REVIEW:

- A resource-enumeration callback's pointer/integer round trip
  (`performance-no-int-to-ptr`), required by the documented LPARAM-style context.
- `IconLoad::pixels` lacks a default member initializer (`uninitMemberVarNoCtor`),
  although its sole construction explicitly supplies the pixel size.
- An explicit shared_ptr copy retaining worker state
  (`performance-unnecessary-copy-initialization`); this is a small ownership/style
  diagnostic, not evidence of an extraction latency or lifetime defect.

Earlier REVIEW findings remain recorded above; they were not silently reclassified.
Temporary measurement/setup code, baseline copies and raw outputs are removed
after recording this summary. The five permanent integration cases and the
109-case suite are retained. No commit, push or release was created.

Proposed changelog addition:

> Show Everything's executable icon beside its search action, with background
> loading and an automatically refreshed memory cache.

## 2026-10-25 — FileSpacer 1.1.4 release preparation

The publication audit fetched the actual remote main branch and tags and reviewed
the complete working-tree diff, including newly added source, tests and documents.
Local main and origin/main both pointed at `f257a2c`; the annotated `v1.1.3` tag
resolved to that commit and the GitHub 1.1.3 release was already published with
its installer and update manifest. There was no pending shortcut implementation
or group-24 test change to publish again. The existing 1.1.3 changelog section is
retained, and 1.1.4 explicitly builds on that keyboard/mouse functionality.

The remaining release scope is Everything integration, independent persistent
per-folder history, the automatic nonactivating suggestion list, native search
preservation, the dynamic cached executable icon, associated resources/tests and
the consolidated development history. Publication uses the existing configured
Git identity, Benco / LeBenco's noreply address, with no assistant co-author.
The authentication associated with the exact repository URL was checked as
LeBenco; no global identity or credential configuration was changed.

Fresh checks on the 1.1.4 working tree:

- Debug and Release MSVC/NMAKE builds and the local NSIS installer succeeded,
  without a new `/W4` warning. Both application executables report 1.1.4. The
  local installer is a pre-publication build; GitHub Actions independently
  produces the assets distributed with the release.
- The selected Internal suite passed all 18 cases, including storage/schema
  migration, retention, native rendering and the two internal search cases.
  Production state/preferences were not used or reset. The 109-case UI-inclusive
  suite was not rerun; earlier real Shell/Everything checks and the user's manual
  icon confirmation remain the functional evidence described above.
- clang-tidy and Cppcheck independently completed all ten required units and
  header consumers: FolderWindow, FolderStateStore, Settings, SettingsDialog,
  SearchBox, ItemWindow, Everything, EverythingTests, SearchHistoryTests and
  RetentionTests. The latter uses its existing C++17 test profile; other units
  use C++14. Windows x64 Debug macros, real SDK/MSVC/ATL headers and the official
  Cppcheck models were verified. Existing units were compared against HEAD
  `f257a2c`. No parser failure, crash, timeout or reduced ATL coverage occurred;
  no modified file remains insufficiently analyzed.

The static verdict remains **REVIEW**, with no confirmed new functional defect.
The previously described empty-string Settings analyzer trace, Win32 pointer/ID
conversions and callback signatures, aggregate-initialization/default-member
warnings, DWORD enum size and shared_ptr ownership/style suggestions remain for
review. The full release scan also reports bounded-vector preallocation,
`std::transform`/const suggestions and test-main exception/reporting style. These
were not used as a reason for unrelated refactoring or suppression. Warnings
about unchanged existing header definitions are distinguished from additions.

The publication is prepared as separate application, test, documentation and
version/changelog commits, following the repository's existing message style.
The existing `release.sh` procedure pushes main and an annotated `v1.1.4` tag;
the repository's GitHub Actions workflow builds and publishes the installer and
update manifest. Publication success and asset integrity must be verified from
the actual workflow/release, not inferred from the successful local build.

## 2026-10-26 — Full regression follow-up and 1.1.5 correction

The initial full 1.1.4 run executed 109 cases: 102 PASS, five FAIL, two BLOCKED,
no NOT_RUN, and no runner restoration error. Its results remain separate from
corrected reruns. The 5,000-item load case passed in 30.3 seconds with the
Computer Use helper present; no helper was stopped for this run. A later full
attempt (29 PASS, one FAIL, 79 BLOCKED) was invalidated by Windows LockApp covering
the test windows. Neither attempt is being reclassified as a successful suite.

Two old Alt-double-click cases expected Properties while the second press was
still held. Observing the sheet after release, as in the existing native reference,
preserved the exact target/path, selection, geometry and file assertions. It
exposed a real product defect: replaying the consumed first button-down while the
second press was held could start a Shell drag and create an unintended shortcut.
The native reference kept `AnchorA;OtherC`, while enabled window gestures produced
`AnchorA;OtherC;OtherC - Raccourci.lnk`. This was reproduced before application
changes with the same second-press motion and observation interval.

`FolderWindow::handleAltMouseMessage` now resolves the pointed item with a public
MSAA selection-only operation and obtains its actual Shell PIDL through
`IFolderView2`. Shell `SelectItem` supplies focus and the selection mark without
replaying a button-down. Ctrl combinations snapshot and restore selection before
applying their selection operation; PIDLs remain individually owned in a fixed-size
array, without moving ATL heap-pointer wrappers. Native Alt+Shift selects the
pointed item, while Ctrl combinations preserve the native multiple-selection
behavior. The real double-click and Properties remain handled by the Shell.
View-generation and lifetime guards remain around reentrant boundaries.

An intermediate MSAA TAKEFOCUS implementation was rejected: a passive modifier-only
trace showed MSAA injecting Alt-up/Alt-down after the real release, leaving Alt
pressed. Sticky Keys was disabled. Selection alone avoided that race but failed
the native logical-focus comparison. The final Shell-focus approach passes the
reference comparison, including focus, selection mark, Properties target/title,
unchanged geometry and exact folder contents. Coverage includes second-press
motion, move/resize bindings, Alt+Ctrl/Shift combinations and an initially selected
Ctrl target. The original gesture assertions were not weakened.

Physical drag/drop and activation tests reuse the existing `NativeTargets` helper
because UI Automation returned no clickable point for visible Details rows and
misidentified empty background. The helper validates the observed native control,
Shell index/count, icon hit and selection before real mouse input. Move/copy bytes,
both views, Ctrl retention and activation-selection assertions remain. Input
guards now report the specific virtual key rather than only requesting release.

The real Everything test exposed an isolation defect with installed
`Everything.ini` setting `run_as_admin=1`: Everything bootstraps that setting beside
the executable even with `-config`. The test retains real installation detection,
then uses a byte-identical official executable copy with a dedicated adjacent
bootstrap INI, fixtures and index. The Unicode-path copy has its own bootstrap.
A SHA-256 comparison checks that the installed INI remains unchanged. This tests
the actual Everything application with isolated manual-path launches, without
changing the installed user's elevation option.

At the user's request, the outer UI test runner now uses a scoped Windows power
request to keep the display/system awake and temporarily disables the screen saver
at runtime. Its final cleanup restores the original state and releases the owned
request, including after a failing case. No saved power plan or desktop preference
is changed; Internal-only runs skip this setup. `desktop-activity.json` records
initial state and restoration. Normal and deliberately failing cleanup probes, and
both passing and failing real UI runs, verified restoration. Manual locking and
security policies remain outside this suspension; the runner does not unlock
Windows. Temporary investigation helpers are not part of the delivered mechanism.

Debug, Release and the 1.1.5 installer build without new `/W4` warnings. clang-tidy
and Cppcheck independently completed FolderWindow and the three version-header
consumers (main, SettingsDialog and Update), with x64 Debug macros, C++14, real
MSVC/ATL 14.44.35207 and SDK 10.0.19041.0 headers for clang-tidy, and official
windows/microsoft_atl/sqlite3 models for Cppcheck. The full source baseline at
`b222a35` was analyzed with the same settings. No parsing failure, timeout, analyzer
crash or ATL coverage reduction occurred; no modified C++ file lacks coverage.

Static analysis is **REVIEW**, with no confirmed new functional defect. New
FolderWindow warnings concern braces, short identifiers, signed positive Win32/MSAA
flag combinations, declarations, implicit boolean conversions, const suggestions,
include-cleaner preferences for transitive SDK definitions, and increased cognitive
complexity (189). They were not used for unrelated refactoring or suppressions.
Cppcheck has no new diagnostic. Its two unchanged Update `deallocuse` messages are
also present in the baseline. Source locations inside lambda descriptions are
normalized so moved lines are not misclassified as new diagnostics.

The final complete rerun executed all 109 cases: **109 PASS, zero FAIL, zero
BLOCKED, zero NOT_RUN, no runner restoration error**. The real Everything case
passed in 12.9 seconds, suggestions in 8.9 seconds, native Properties comparison
in 25.9 seconds and the 5,000-item load in 30.6 seconds. The exhaustive settings
matrix took 156.9 seconds and the 64 real gestures 175.9 seconds, versus 152.5 and
180.1 seconds in the initial run. These are observed scenario durations, not a
standalone performance-equivalence benchmark. Source/test hashes remained unchanged
through the final run. The screen saver was initially enabled and its runtime
state was restored successfully; saved desktop/power preferences were not changed.

Release 1.1.5 is prepared with separate application, tests, documentation and
version/changelog commits under Benco's existing identity, without an added
co-author. Publication uses the existing annotated-tag/GitHub Actions workflow;
its outcome and downloaded asset/manifest hashes are checked separately from the
local build and recorded with the publication report.

A separate, non-delivered badge probe used the actual official Everything
1.4.1.1032 executable with an isolated bootstrap and index. An EFU containing two
files in different folders and one listed folder also exposed synthesized ancestor
folders; `filelistfilename:` still included them. The exact three-member assertion
failed and was retained. This simple EFU route is therefore not validated for exact
badge membership. Everything 1.5 Tags properties are a separate candidate requiring
persistence, refresh and rename/move validation before any badge implementation.

## 2026-10-26 — Default folder window proportions

Change the fallback folder-window size in `src/Settings.h` from 210 × 435 to
580 × 435 at 96 DPI. The height stays at 435 and the outer-window ratio becomes
4:3. Existing configured dimensions and per-folder saved sizes retain precedence;
no preferences or saved geometry are migrated.

The Microsoft [Win32 layout guide](https://learn.microsoft.com/en-us/windows/win32/uxguide/vis-layout#window-size)
recommends content-appropriate default sizing and balanced proportions between
3:5 and 5:3. This is Windows 7-era guidance, not a specific 4:3 requirement.
The user's proposed 4:3 ratio fits that range while preserving the current height.

Debug MSVC 14.44.35207 / NMAKE / Windows SDK 10.0.19041.0 completed with `/W4`
and no warnings. clang-tidy 23.1.4 and Cppcheck 2.22 independently completed
Settings, FolderWindow, SettingsDialog and ItemWindow, covering the modified
Settings header. The baseline source snapshot was taken at `fe728f2` and analyzed
with identical Windows x64, C++14 and Debug settings. clang-tidy used the real
SDK/STL/ATL headers; Cppcheck used the official windows/microsoft_atl/sqlite3
models without full system headers. Static analysis: **PASS**, no new diagnostic,
parsing failure, timeout, crash or reduced ATL coverage. No analysis-driven code
correction was needed.

A fresh fixture folder was opened in the real Debug application with isolated
`/test` registry/database state and no configured window dimensions. The desktop
controller repeatedly rejected the observed window identity, so it could not
complete its own visual inspection. The user inspected the open window, confirmed
correct proportions and controls, and closed it; the application exited with code
0. No resize/reopen interaction was exercised in this manual check. The original
test registry/database were restored and production registry/database hashes
remained unchanged. All 168 regression-source files retained their SHA-256 hashes;
no regression test was modified or run for this focused manual validation.

The changelog records this change under Unreleased. No version change, installer,
tag or release is part of this task.

## 2026-10-26 — Ancestor removal, notification cost and release 1.1.6

The reported defect was confirmed: removing a directory could leave a window
for a descendant open, including a grandchild, while the Jump List still offered
an unavailable last-closed folder. Reconstructing an IShellItem for a removed
path can fail before removal is handled, so the final implementation compares
retained PIDLs instead of depending on that reconstruction.

The delivered `ItemWindow` design has one message-only ancestor receiver and
one Shell-only `SHCNE_RMDIR` registration **per process**. Existing local item
registrations keep their normal filesystem/Shell scope. The receiver selects
strict descendants, takes strong references before dispatch and keeps cloned
PIDLs through reentrant COM comparisons. A registration-generation guard rejects
continuations that became stale. A confirmed removal clears the stored state
key before destruction, so close-time saving cannot recreate the deleted row.
Unrelated and same-prefix sibling windows remain unaffected.

The earlier per-window broad registration was evaluated before this dispatch
design was retained. A separate native subscription study completed 45/45 runs
and 135/135 phases, with 71,100 published directories, 505,600 broad deliveries,
963,800 recipient checks, no lock failures or false matches, and normal producer
and receiver exit codes. It measured native notification cost in an isolated
STA harness; it did not measure the complete FileSpacer UI. Temporary source,
binaries and fixtures were removed. The retained local summary is under
`build/subscription-bench-20261026/`.

`ClosedWindows.cpp` now omits an unavailable latest folder from the Jump List
without treating the expected parsing failure as a product error. The history
entry is retained for a later retry; an older entry is not silently substituted.

Final focused evidence for the dispatch:

- Debug x64 `/W4`: PASS, no warnings.
- Eight real Shell scenarios passed, including direct/ancestor removals at
  depths one and two, Recycle Bin and permanent deletion, recursive filesystem
  removal, preservation of unrelated siblings, four windows in one process,
  child close/reopen, batch close and survival of the parent. Open-folder rename
  also preserved its identity, selection and geometry.
- The separate `07-fenetres/01-Etats` case failed because the RustDesk panel
  covered the maximized window's Minimize button. Its actual obstruction was
  recorded; the target, focus and assertions were not changed to obtain PASS.
  The overall affected run remains failed rather than being relabelled successful.
- clang-tidy and Cppcheck finished with full changed-header coverage, real
  SDK/MSVC/ATL where required, and no parsing failure or ATL reduction.
  **REVIEW** remains for adjacent old/new PIDL parameters and Cppcheck treating
  the generation guard as constant without modelling COM reentrancy. A cloned
  target PIDL was retained across COM before final validation.

Release preparation subsequently passed all 19 Internal cases. Five initial
preparation failures were resolved by putting the standard Windows PowerShell
modules first in `PSModulePath`; tests and expected results were unchanged.
The initial placement implementation and its real Shell checks were also
included in this release, together with the 580 x 435 default size and moving
folder-state maintenance from General to About.

The pending 1.1.6 changes were reorganized into 14 file-scoped commits under the
configured human author/committer identity, in addition to the two wider-default
commits that were already on main. Older local commit IDs in intermediate
validation reports are historical; tag `v1.1.6` resolves to `45f4922` and the
source in that tag is authoritative. Local Debug, Release and NSIS builds passed.
The [publication workflow](https://github.com/LeBenco/FileSpacer/actions/runs/40390773529)
succeeded. The published installer has 1,331,515 bytes and SHA-256
`b25895aff347ba580f1ecec99b529f5032b7b0bcfffd961fa69354eff09e40da`;
the downloaded file, update manifest and GitHub digest agreed. The local
pre-publication installer is a separate build and is not that published artifact.
Local evidence is retained under `build/ancestor-dispatch/` and
`build/release-1.1.6/`.

## 2026-10-26 — Exact hybrid window placement and release 1.1.7

The initial enabled-by-default occupation grid shipped in 1.1.6. It used
ten-unit cells, incremental union counts and summed-area queries, with
transaction-like withdrawal of a source that spatial navigation would close.
The user then requested minimizing the actual covered surface and accepted an
exact hybrid after comparing implementations.

The delivered `WindowPlacementState` retains exact monitor-clipped rectangles.
Updates only record geometry. For a new placement it first checks the preferred
origin and searches for free space with a sweep and coverage tree; if no free
position exists, compressed true edges and weighted union-area tables provide
exact minimization. Existing windows are not rearranged. The preferred location
breaks equal-area ties. Saved folder geometry still takes precedence; minimized,
invisible and cloaked windows and the source being replaced are excluded.
Failed creation can reintegrate the source. Oversized requested dimensions retain
an accessible origin. Allocation failure does not escape the EnumWindows callback
and leaves the existing Windows-placement fallback available.

The option stays in General > Folder navigation, enabled by default. Its text is
**Minimize window overlap** / **Minimiser la superposition des fenêtres**.
The existing preference key is retained. Coordinates use the same Windows
coordinate context as the window rectangles and monitor work area; the study
uses logical dimensions directly, without a separate Windows zoom parameter.

The committed [independent study](../tests/placement-simulation/README.md) is
separate from the automatic regression suite. It manipulates rectangles rather
than windows and retains the former grid in `LegacyGrid.inc`:

- Initial comparison: 324 scenes, six methods, three repetitions. The two exact
  methods and pixel grid obtained the same minimum surface. The ten-unit grid
  was suboptimal in 32 scenes, with no missed free position in that sample.
- Observed general-exact/grid crossovers for dispersed windows were between
  40–60 windows in 1080p and 100–200 in 4K. Earlier 23/59 estimates compared
  candidate-count bounds; they were not measured speed thresholds.
- Adversarial pass: 2,160 scenes, 11 repetitions per method, 20/40/60 windows,
  three target sizes and logical 1080p/4K. Cases force the failed sweep and
  exact fallback or leave exactly one free integer origin. Candidate counts
  reach 55,695 out of the 59,049 bound for 60 windows.
- With 60 blocked windows, the slowest scene median was 0.7203 ms in 1080p
  and 0.5707 ms in 4K. Raw maxima were 5.7044 and 1.1438 ms. The geometry
  behind the isolated 5.7044 ms sample was repeated 100 times: median
  0.3661 ms, maximum 0.7282 ms. The original spike is retained; its cause
  was not instrumented. These are observed calculation costs, not an OS
  scheduling bound or complete folder-opening latency.

The actual product's group 26 was changed to **functional checks only**:
free/blocked execution, geometry updates, withdrawal/reintegration, refresh,
bounds, negative coordinates, oversized dimensions and several-window scenes.
It contains no timing measurement or independent optimality oracle. The real
Shell case also checks the localized setting name, default, Cancel/Apply,
About maintenance, cross-process placement, minimized windows, spatial source
closure and saved-position priority. All 19 Internal cases and 13 targeted real
Shell cases passed. Earlier Computer Use identity failures were separate from
this native project harness; no FileSpacer defect was established from them.

Debug and Release builds passed without warnings. Both static tools completed
the changed source/header consumers. The product verdict is **REVIEW** for two
Cppcheck aggregate-initialization warnings on `PlacementChoice::area/distance`;
all construction sites initialize the fields, and clang-tidy has no new product
diagnostic. The historical study's deterministic-seed, callback/cast,
aggregate and test-entry-point/style findings are documented separately.
No modified C++ file lacks analysis. Version-header analysis introduced no
new diagnostic; unchanged Update `deallocuse` messages also occur in its baseline.

Release 1.1.7 comprises 18 commits, grouping files only when their complete
messages match. Benco's configured identity is both author and committer, with
no added co-author. `release.sh` pushed `main` and annotated tag `v1.1.7` at
`99ff6f7`. The [workflow](https://github.com/LeBenco/FileSpacer/actions/runs/40401354272)
built and published the installer and update manifest successfully. The
downloaded installer has 1,332,573 bytes and SHA-256
`3830366c64264a2a3afdc75e6e26273b0922e167a33ad794874f77a062103107`;
the manifest's `01010700` version, size, hash and GitHub digest were verified.
Evidence remains under `build/hybrid-placement/`,
`build/placement-worst-cases/` and `build/release-1.1.7/`.

## 2026-10-26 — Separate client-test worktree: partial validation

The separate `codex/client-smoke` worktree is based on 1.1.5 commit `fe728f2`.
Its connection launcher and `tests/client` prototypes remain untracked in that
worktree. They were not merged into `main`, committed with the placement work,
or included in releases 1.1.6/1.1.7.

The Windows Sandbox smoke test passed using a Release executable and precompiled
.NET helper prepared on the host: a real folder and `probe.txt` were observed,
the window was resized programmatically from 210 x 435 to 900 x 600, and it exited
normally with code zero. Neither Visual Studio nor `cl.exe` was present in the
guest. This validates that limited client scenario, not physical input or the
complete regression suite on a clean client.

The Alt-right-drag experiment worked behind other host windows. Pointer
non-interference remains **REVIEW**: some trials contained manual host activity,
and another observed unexplained host pointer movement. Minimized Sandbox was
**BLOCKED** by unavailable guest input/foreground conditions; later input failed
with access denied. No assertion or focus repair changed that into PASS.

The later Hyper-V access launcher waits for its helper's real exit and report;
four simulated launcher outcomes passed. The last retained real connection
report is **BLOCKED** with rejected credentials. No FileSpacer tests were started
by that connection probe. Client execution with VMConnect closed, guest physical
input and transfer of the full suite remain unvalidated. This audit did not
launch a VM, enter credentials, rerun tests or alter that worktree.

## 2026-10-26 — Local installer option and folder-icon proposal

A separate session added an optional, initially unchecked default-file-manager
component to `installer/install.nsi`. Its French and English descriptions explain
that it opens folders and drives in FileSpacer, can also be enabled later in
Settings > Integration, and can be reversed there. Selecting it also selects
the required context-menu component; it uses the same three per-user folder/drive
associations as the existing settings code. This remains an **uncommitted local
change**. The session reported a successful Debug build with no recompilation;
NSIS compilation and visual inspection of the bilingual pages remain pending.
At the user's request that session is waiting for this documentation work to
finish. Its installer changes are preserved outside the documentation commits.

Another session considered a folder-icon editor based on `desktop.ini`: reference
the original icon or copy it under a renamed hidden file in the target folder.
The design proposal favours an independent utility exposed through a Shell
command, with preview and reset-to-default controls. No implementation was made,
and actual Shell/menu integration was not validated. This is a design discussion,
not a delivered FileSpacer feature.

## Documentation audit boundaries

This consolidation checked local Git/tag history, published release metadata,
retained delivery/analysis summaries, the current source contracts and the
separate client worktree. The runner's actual category discovery was counted:
111 cases, including the previously omitted group-23 catalogue row. Current
guides were corrected; historical 97/100/101/109-case runs keep their original
scope. No build or test was rerun merely to update this documentation.

Remaining limits are explicit: no full 111-case PASS on 1.1.7, the obstructed
Minimize-button case, the existing static REVIEW findings, incomplete additional
theme/DPI/provider coverage, unfinished per-file badge investigation and the
client-isolation work above. Features and later passing runs do not erase those
historical failures or turn the prototypes into shipped behavior. The local
installer change likewise awaits its own compilation and visual validation.
