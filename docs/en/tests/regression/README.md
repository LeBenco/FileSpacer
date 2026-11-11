<a id="regression-tests"></a>

# Regression tests

The reference launcher is `tests.cmd`, run from CMD at the repository root.
It builds the Debug application with MSVC/NMAKE and selects independent cases
from this checkout. Application sources, binaries and test helpers must come
from the same revision. See the [build guide](../../build.md) for dependencies.

<a id="run-locally"></a>

## Run locally

Close FileSpacer before starting. UI tests require an unlocked interactive
Windows x64 desktop, PowerShell 5.1 and an NTFS temporary directory. Leave the
keyboard and mouse available to the test. Close Control Panel before selecting
its external-window test. Internal tests do not need physical input, although
some exercise real Windows or COM objects.

```cmd
tests.cmd -Category Internal
tests.cmd -Pattern "12-navigation-spatiale\*"
tests.cmd -Category UI -Pattern "16-redimensionnement-alt\03-*"
tests.cmd
```

During development, run new and affected cases. `tests.cmd` without filters runs
the functional regression suite; the GitHub regression workflow also supports it.
Stress tests are a separate, explicitly selected suite and never run by default.
Do not run a native test a second time when the selected suite already includes it.

| Option | Meaning |
| --- | --- |
| `-Category Internal\|UI\|All` | Filter by the category declared in each case; default `All`. |
| `-Pattern "group\case-pattern"` | PowerShell wildcard matched against the relative case path; default `*`. Combine with `-Category`. |
| `-Scope Application\|All` | Both select the application suite; default `All`. |
| `-Suite Regression\|Stress` | Select one disjoint catalogue; default `Regression`. |
| `-TimeoutSeconds N` | Per-case deadline, 30–3600 seconds; default 300 for regressions and 900 for stress. A timeout is a failure. |
| `-RunDirectory "path"` | Store or copy reports to a new directory; existing reports are never overwritten. |
| `-VM "name"` | Run selected cases in a configured Hyper-V client. |

To inspect the selection **without building or running tests**, call the
PowerShell runner directly. `tests.cmd` performs a build before forwarding its
arguments, including `-ListOnly`.

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\regression\Run-Regression.ps1 -Category UI -Pattern "30-tri-melange\*" -ListOnly
```

The JSON output lists case paths, categories and scopes. Use it rather than a
fixed catalogue count. Some compatibility cases prepare fixtures from Git tags;
keep the required Git history available when running them.

The UI runner temporarily keeps the system/display awake and suspends the
screen saver, then restores both even after failure. It does not change the
power plan or unlock Windows. `desktop-activity.json` records restoration.

<a id="run-in-hyper-v"></a>

## Run in Hyper-V

```cmd
tests.cmd -VM AgentWin10 -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

The host builds the application and selected native/C# helpers before transfer.
The client verifies their hashes and runs the same scripts and assertions, with
no compilation in the guest and no fallback to the host desktop. Selection
precedes package preparation. See the [Hyper-V guide](../client/README.md) for
one-time setup, desktop requirements and transport diagnostics.

<a id="run-on-github"></a>

## Run on GitHub

GitHub Actions uses the [regression workflow](../../../../.github/workflows/regression.yml)
for this same application suite. It can be launched on demand; ordinary branch
pushes and pull requests do not trigger it.

Before preparing Windows, the workflow fingerprints the application sources and resources,
dependencies, build inputs, test harness, category and pattern. An exact successful result
available in GitHub's cache is reused: documentation, screenshots, `install.nsi` and release-note
changes alone do not rerun the suite. The summary links the original execution and commit;
packaging still uses the selected revision. Changed inputs or missing, evicted or invalid
evidence require a new execution. GitHub's cache branch/ref visibility rules apply.

In **Actions > Test FileSpacer regressions > Run workflow**, choose `ref`
(branch, tag or commit), `category` and `pattern`. `All` / `*` runs the complete
application suite. The workflow must exist on the default branch for manual
dispatch. The selected revision must contain the workflow and runner it needs;
tests are not taken from a different checkout.

The workflow resolves the revision once, builds Debug on `windows-2022`, and
runs the prepared cases on its interactive desktop. Its `-CI` mode is restricted
to GitHub-hosted runners and cannot be combined with `-VM` or `-Prepared`. Cases
run as the same interactive user with a limited, medium-integrity token;
preparation and worker checks verify access to `Shell.Windows()` and the actual
token. COM registration is per user. This environment does not replace tests
on Windows 10 or on another required display/provider configuration.

The workflow publishes no installer or offline help. The [release
workflow](../../../../.github/workflows/release.yml) reuses it as its first job, within
the release run, then builds documentation and the NSIS package from the validated
SHA. A successful package build is not a real installation or upgrade test.

The Actions summary gives the tested SHA, totals by category and each failure's
exit code and first error. Logs, JSON reports, screenshots and
`regression-summary.md` are retained for 14 days in
`regression-<run>-<attempt>`, including failed runs. Worker and cleanup failures
remain invalidating even when individual cases passed.

<a id="find-the-relevant-cases"></a>

## Find the relevant cases

Case directories are grouped by behavior under `cases/`. Names and paths are
stable selectors; they need not be translated to use this English guide.

| Area | Groups |
| --- | --- |
| Harness and prepared-artifact contracts | `00-banc` |
| SQLite, retention, quota, saved view state and history | `01`–`02`, `06`–`09` |
| Menu/lifetime, STA and dimension handling | `03`–`05` |
| Update manifests and session restoration | `10`–`11` |
| Spatial navigation, preferences and layouts | `12`–`14` |
| Move/resize gestures, closing during interaction | `15`–`17`, `24-raccourcis` |
| Native view persistence and Shell commands | `18-vue-shell-native`, `19-commandes-shell` |
| Load, shell notifications and external Control Panel | `20-validation-elargie` |
| Activation click, locked layout and selected-name rendering | `21`–`23` |
| Search/history and Everything | `25-recherche-integration` |
| Window placement and transient View-menu previews | `26-placement-fenetres`, `28-apercu-affichage` |
| Default-manager associations | `29-menu-gestionnaire` |
| Local help and resource-language selection | `30-documentation`, `31-langues` |
| Mixed sorting and provider identities | `30-tri-melange` |
| Title menu and Peek integration | `33-menu-titre`, `35-apercu-peek` |

These are coverage entry points, not a record of executed tests. Inspect each
case's setup and assertions before deciding whether it covers a change.

<a id="reduce-redundant-tests"></a>

### Reduce redundant tests

Compare behavioral obligations, not case names or past PASS results. Record the
initial state, action, observations and environment before replacing a case.

- **Implication** (`X => Y`): under the stated preconditions, X establishes every
  obligation of Y. The reverse need not hold. Exact visible identities imply
  their count; a matching count does not establish identities. Keep X and move
  any missing transition from Y into it before removing Y.
- **Equivalence** (`X <=> Y`): establish both implications under the same
  preconditions. Keep one representative only when neither case observes a
  distinct failure. For a complete enumeration and unique expected names,
  exact-name equality is equivalent to exact-name equality AND the expected
  count, so the extra count assertion is unnecessary.

These are scoped arguments about the checks, not proofs that a passing finite
test implies correctness for every input. A stronger-looking action, more items,
or a more complex environment alone establishes neither relation. In particular,
multiselection need not cover single selection; inactive-window behavior need
not cover active-window behavior. If a relation depends on several retained
cases, all of them are required. Follow implications transitively only when
their preconditions remain compatible; a cycle is not evidence of equivalence.

A test may also be retired because its cost and maintenance outweigh its
regression value. This is an explicit reduction of the obligations, not an
implication claim. The exhaustive forbidden-shortcut truth tables have been
retired on that basis. Keep representative page/Registry round trips, input
capture, exchange, defaults, real geometry and cancellation checks instead.
Run functional logic once, independently of language. Only checks of translated
text, accessibility, layout, mnemonic collisions or resource selection vary by
language. In particular, shortcut and context-menu logic is no longer repeated
in English and French, and PowerToys exclusions are not repeated per translation.

The following reductions retain the original search obligations:

| Retained case | Replaced case / check | Relation and preserved boundary |
| --- | --- | --- |
| `13-preferences/05-FiltresSuccessifs.ps1` | `03-RecherchePendantSaisie.ps1` | Its exact-identity checks imply live filtering, no-match results and full restoration on clearing. Direct nonempty-result clearing was added before removal; same-cardinality query changes and native notifications remain. |
| `13-preferences/01-FenetresOuvertes.ps1` | `04-RechercheExplicite.ps1` | The initial disabled-search stage retains typing, submit, clear-without-submit and explicit clearing on the original two-item fixtures, in both processes. Later live enable/disable propagation remains independently checked. |
| Exact-identity assertions in both retained cases | Count-only search assertions and their unused helper | One-way implication from identities to counts; equivalence only between exact identities and the conjunction of those same identities with their implied count. |

No equivalence between independent cases is assumed. Keep ordinary reopening
(`07/08`) separate from the closed-window-history command (`07/05`), hidden
selection views (`18/05`) separate from presented views (`18/06-SelectionAllVisible`),
and filtered persistence (`13/06`) separate from successive filters (`13/05`).
Those pairs observe different entry points, rendering or saved-state boundaries.
Likewise, mixed-provider wrappers share scenario code with filesystem tests but
exercise a different provider; they are not redundant executions.

Validate each retained case alone and through the prepared runner when changing
its dependencies. Deleted cases are removed from discovery, not reported as
inferred PASS results. A failed, blocked or unexecuted representative never
validates a replaced obligation. `-Category` and `-Pattern` still filter the
remaining catalogue normally; use the retained path above for a deleted case.

<a id="stress-suite"></a>

### Separate stress suite

```cmd
stress-tests.cmd -VM AgentWin10
stress-tests.cmd -VM AgentWin10 -Pattern "18-vue-shell-native\04-*"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\regression\Run-Regression.ps1 -Suite Stress -ListOnly
```

The stress catalogue lives in `tests/stress/cases`, outside regression discovery.
It reuses the same runner, isolation, host compilation and VM transport. The
prepared selection, result entries and summary record `Suite`; a prepared worker
uses that saved suite, never the other catalogue. No `All` suite combines them.
Local stress reports live under `build/stress`; the existing VM transport keeps
its `build/regression/vm-<id>` report location and records the suite in its package.

Stress includes 5,000-item SQLite recovery, saved-layout redraw, load/restarts,
View-menu cancellation, clear/select-all selection matrices, repeated concurrent
Preferences requests, the comparative slow-column probe and column/frame drag
CPU measurements. Functional selection regressions use 129 items to retain the native 128-item
threshold for partial and complete selections; concurrency
uses one simultaneous pair plus the initialization, owner-close and abandoned
mutex boundaries. Large and repeated variants run only through the stress suite.
Production selection matrices use one pass per distinct combination rather than
three timing repetitions: 135 operations for 129 items and 135 for 5,000 items.

Managed helper source bundles are compiled once per identical ordered contents
within a prepared package, with the package's fixed compiler, references and
options. Each case retains its own manifest entry and hash checks. Native helper
argument combinations continue to be compiled once per package as before.

Selection stress cases emit `private-diagnostics.json`, including:

- separate clear, select-all and optional range capability observations;
- discovered function RVAs, qualification reasons and module identity evidence;
- the original structural `.public` and transient `.attempt` decision reasons,
  with checksums, including the first required private symbol that blocks discovery;
- observed per-view dispatch, backend use and retained stdout/stderr evidence.

`AVAILABLE`, `PARTIAL` and `NOT_OBSERVED` describe private-capability coverage,
independently of the functional PASS/FAIL result. Observations include deliberately
degraded test arms identified by their source path. A public fallback PASS does not
certify private acceleration. A zero RVA means unresolved in the descriptor, not
proof that the function is absent from Windows. A symbol reported missing by the
worker was looked up against the matching PDB; an ABI/CFG/identity rejection and
a transient symbol-download failure remain distinct. System DLL versions/hashes
are labeled as system copies, separately from the loaded probe's identity.

<a id="native-view-persistence"></a>

### Native view persistence

`18-vue-shell-native/02-Persistance.ps1` builds the production host, settings,
selection, icon-position, filter and SQLite code against a real Shell view. It
checks restoration, refresh, filtering, serialized data and view destruction /
recreation. Readiness follows public navigation/enumeration signals; a watchdog
may fail a test but cannot declare the view ready.

For a standalone diagnostic of this harness:

```cmd
test-native-state.cmd --diagnostic
```

It initializes MSVC x64 and uses `/Z7`, avoiding a shared compiler PDB. It is an
alternative invocation of this test, not an additional full-suite check.

<a id="external-applications"></a>

### External applications

The real Everything case in group 25 needs an installed, detectable copy of
Everything, with no interface open in the tested desktop session. Its service
in session 0 may remain running. `FILESPACER_EVERYTHING_EXE` can select an
installation, but the Detect action must also be able to find it. The detected
path is stored as UTF-16 in `everything-detected.txt`.

The case launches a private copy of the real executable with its own INI,
fixture directory and index. Everything reads `run_as_admin` beside its
executable even with `-config`; keep that bootstrap isolated. The installed INI
and elevation choice must remain unchanged. Assertions read displayed queries
and result identities for both scopes, not merely process creation. To check
absence before installation, `FILESPACER_EXPECT_EVERYTHING_ABSENT=1` enables the
corresponding detection assertion in case 02.

Peek launch/protocol tests and native-menu tests do not establish real Peek or
Nilesoft rendering. Inspect their fixtures and use the actual external program
for visual interoperability acceptance. Missing prerequisites do not validate
the integration.

<a id="mixed-sorting"></a>

### Mixed sorting

Group `30-tri-melange` checks ordering, preference changes, notifications,
persisted identities and the provider contract. It reuses the existing Shell
command/navigation scenarios with the provider enabled. Keep those shared
gestures and assertions in one implementation. Filtering, free icon positions,
renaming and deleting the displayed folder are important identity boundaries;
an order-only assertion does not cover them.

<a id="add-or-change-a-case"></a>

## Add or change a case

Place a case under `cases/NN-topic/NN-Name.ps1`. Its first line must be exactly
`# Test-Category: Internal` or `# Test-Category: UI`. Use Windows PowerShell 5.1
and C# 5 / .NET Framework 4.8 for managed helpers. Parse scripts and compile
helpers before a UI run. Avoid assigning PowerShell automatic variables.

Reuse `common/` for process handling, native builds, physical input and results.
Keep dependencies explicit: VM preparation inspects scripts, resolves helper
paths and precompiles their exact argument combinations. A new dynamic path or
build command may need a corresponding preparation change. Verify both direct
execution and prepared execution when changing this boundary.

Observe the action's real outcome: item identity, effective setting, geometry,
selection and focus where relevant. A matching count alone is insufficient.
Do not repeat a failed action, force focus after it, or change an assertion just
to obtain PASS. Injected failures cover deterministic error/lifetime paths;
they complement real-Shell tests rather than replacing them.

<a id="reports-isolation-and-diagnosis"></a>

## Reports, isolation and diagnosis

Local reports live under `build/regression/<date>-<id>/`. Keep `results.json`,
`summary.json`, per-case stdout/stderr, application exits and captured evidence
together. VM runs add deployment/session/restoration evidence; see the
[transport report layout](../client/README.md#reports-and-diagnosis).

PASS applies only to cases actually completed. FAIL, BLOCKED, NOT_RUN, a missing
summary, a timeout, a crash or failed cleanup prevents a complete success.
Preserve the first error and cleanup errors separately. Report build, functional
tests and static analysis as separate checks.

The runner backs up and restores the `/test` Registry namespace and test SQLite
database. Never reset production data for a test. Between cases it removes only
test Jump List identities under `Software\FileSpacer\Test\JumpLists`; identities
created during one case survive for that entire case. Otherwise Shell
`CommitList` / `FlushFileBuffers` work can accumulate across unrelated fixtures.
`case-fixture.json` records this cleanup. External windows must be identified and
closed individually; never terminate Explorer to clean up a Control Panel test.

Useful pitfalls when diagnosing UI failures:

- UI Automation exposes Details entries as `DataItem` in a `DataGrid`.
- Keep Ctrl physically pressed through the observed result of Ctrl-drag.
- Check the actual hit target and cursor coordinates; an intended click is not
  proof of delivery. Use the shared input helpers for disconnected VM consoles.
- End an excluded resize gesture inside the rename editor. A right-button
  release outside it can legitimately open the Shell context menu later.
- A double-click can close the source under spatial navigation policy. Configure
  the intended source-window policy in the fixture.
- Keep paths short when a Win32 tool reports error 206. Native harnesses use
  `/Z7` to avoid shared-PDB failures.

For search diagnosis, `set FS_SEARCH_TIMINGS=1` enables real-read durations in
`search-observations.jsonl`; stress case 20/01 also writes `load-phases.jsonl`. These
logs do not alter deadlines or assertions. Concurrent accessibility inspection
can substantially change load timings. Compare builds under the same observer,
fixture and environment conditions; load timings are not universal performance
thresholds. The harness does not stop external inspection tools.
