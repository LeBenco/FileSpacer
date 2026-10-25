# Development history and validation

This handoff records the work in chronological order: window shortcuts and
release 1.1.3 first, then search/Everything integration and its follow-ups.
Both sessions took place on 25 October 2026. Their order is established by the
search comparison's baseline, commit `f257a2c`, which already contains the
published shortcut implementation.

At consolidation, `main` is at `f257a2c`: the shortcut work is committed and
published, while the later search changes remain local and uncommitted.
The entries below describe historical checks on their respective revisions,
not a new validation of the combined working tree. Current code and Git state
take precedence over these snapshots.

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
