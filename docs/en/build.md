<a id="building-filespacer-on-windows-x64"></a>

# Building FileSpacer on Windows x64

FileSpacer targets Windows 10 or later, x64. It uses MSVC/NMAKE and NSIS, not
CMake. Run build and test commands from CMD at the repository root. See the
[developer overview](README.md) for the code map.

<a id="dependencies"></a>

## Dependencies

- Visual Studio 2022 Build Tools with MSVC, ATL and a Windows SDK. The launcher
  expects `%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools`.
- Inkscape, normally `C:\Program Files\Inkscape\bin\inkscape.com`.
- Python with Pillow. The Windows Inkscape distribution supplies it; the build
  uses the adjacent `python.exe`. `ICON_PYTHON` can select another installation.
- ImageMagick, with `magick` on PATH.
- NSIS, normally `%ProgramFiles(x86)%\NSIS\makensis.exe`, for packaging.
- Node.js 22+, Ruby 3.3 and Bundler when generating offline help; see
  [website and offline builds](website/README.md#shared-website-and-offline-builds).

<a id="build-and-clean"></a>

## Build and clean

Close FileSpacer before building, then run:

```cmd
build.cmd debug
build.cmd release
build.cmd installer
```

`build.cmd` initializes x64 MSVC through `vcvars64.bat` and invokes NMAKE.
Debug uses `FILESPACER_DEBUG`, `/MTd` and the console subsystem. Release uses
`/O2`, `/MT` and the Windows subsystem. Project C++ uses `/W4` and C++14.
Add new translation units to the relevant `OBJECTS` list in `Makefile`.

| Output | Use |
| --- | --- |
| `build\debug\FileSpacer.exe` | Development and regression tests. |
| `build\release\FileSpacer.exe` | Release application. |
| `FileSpacerFolder.dll` beside either executable | Native-view provider for optional mixed file/folder sorting; keep it with the application. |
| `build\FileSpacer-install.exe` | Distributable installer. Packaging does not launch it. |

To override nonstandard tool locations:

```cmd
set "INKSCAPE=D:\Tools\Inkscape\bin\inkscape.com"
set "MAKENSIS=D:\Tools\NSIS\makensis.exe"
build.cmd installer
```

An unchanged build should leave the executable up to date. Requesting the
installer target always repackages the Release artifacts.

```cmd
build.cmd clean
```

Clean removes Debug/Release outputs, the generated ICO and installer, preserving
sources and user data. The next build regenerates the icon.

<a id="generated-artwork"></a>

## Generated artwork

`generate-icon.py` produces the ten frames in `src\res\FileSpacer.ico` from the
SVG and its small-size optical masters. At 16–48 px, the folder is drawn on each
pixel grid and the SVG's projected tesseract polygons are supersampled, then
reduced with Box filtering. Inkscape renders the 64–256 px frames. ImageMagick
packs them; the generator checks decoded RGBA content before atomically replacing
the ICO. NumPy is not required.

The SVG's 2D polygons are authoritative; the generator does not recompute their
projection from its 4D/camera metadata. Changing the folder silhouette or its
212 × 212 coordinate system also requires updating `FOLDER_MASTERS` / `draw_folder`.
Keep the website's [16 px favicon](website/README.md) in sync.

Packaging runs `generate-installer-assets.py` for the download badge, sidebars,
wizard resource DLL and UTF-16 changelog. Generated assets live under
`build\installer-assets`, not in source control. `installer\wizard-ui.rc`
defines the shared setup/uninstaller layout. The x86 `WizardBitmap.dll` uses
GDI+ to scale sidebars to the actual control size, with the NSIS loader as a
fallback. It is extracted only into NSIS's temporary plug-in directory.
To check its Debug build from an x64 developer prompt:

```cmd
nmake /f Makefile CFG=Debug installer-bitmap
```

<a id="sqlite"></a>

## SQLite

The unmodified amalgamation in `third_party\sqlite` is compiled separately as C
and linked statically. No SQLite DLL, CLI or build-time download is needed.
See its [origin and build options](../../third_party/sqlite/README.txt).

SQLite uses `/W3`, `_CRT_SECURE_NO_WARNINGS`, `SQLITE_OMIT_LOAD_EXTENSION` and
`SQLITE_DQS=0`, retaining default thread safety and the application's static
runtime. Release links with `/OPT:REF /OPT:ICF`. From a developer prompt:

```cmd
dumpbin /dependents build\release\FileSpacer.exe
```

There should be no `sqlite3.dll` dependency; Windows system DLLs remain required.

<a id="offline-help-and-installer-contracts"></a>

## Offline help and installer contracts

The installer packages the generated `help` directory. It checks its source
fingerprint/file hashes and regenerates it when absent, stale or damaged.
Node.js must be on PATH (`NODE` can override it); Ruby/Bundler are needed only
for regeneration. The release workflow can transfer verified Linux-generated
help to Windows packaging from the same tested commit. Installed help needs
only a browser, with no server or network connection.

Setup starts as `asInvoker`. An All users installation starts an elevated worker
via `runas` only when installation begins. The initiating parent preserves the
chosen directory, language and components, waits for the worker's exit code,
and applies personal associations under the initiating user's account. The
machine worker must not change the administrator's personal default manager.
The All users uninstaller also requests elevation. Cancellation must retain
the exit-code contract expected by the updater.

`install-info.ini` records scope, language and component choices. Under
`[Components]`, `StartMenu`, `ContextMenu` and `DefaultFileManager` are `0` or `1`.
Upgrades restore these choices; silent upgrades preserve the effective default
association even if it changed after installation. The default-manager choice
requires the context-menu component.

The update relay resolves scope from metadata beside the running executable.
A Registry fallback must match that exact directory in only one scope. After
all sections succeed, the initiating setup writes the UTF-16 `/UpdateResult=`
file with `[Installation]`, `Mode`, `Executable` and `Complete=1`. The relay
validates the executable and scope before restoring windows there. Cancellation
or failure restores the original application; an invalid success result keeps
the saved session and reports an error. Pending sessions must not be removed
by stale-relay cleanup. Group `11-session-maj` covers the application boundary.

Setup logs to `%TEMP%\FileSpacer-install.log`; its machine worker uses
`%TEMP%\FileSpacer-install-machine.log`. `/Log="C:\path\setup.log"` overrides
the initiating log. Each run replaces its log. File-operation exit code 5 is
an installer category, not a Windows error number. Keep the initial failure,
cleanup exception/stack and command exit codes when diagnosing installation;
logging must not change the operation's success/failure state.

An NSIS build verifies packaging, not real installation, UAC, upgrade or
uninstallation. Validate the affected installed paths when changing them.

<a id="releases"></a>

## Releases

The [release workflow](../../.github/workflows/release.yml) runs for a `vX.Y.Z` tag
or a manual dispatch for an existing tag. It first reuses the [application
regression workflow](../../.github/workflows/regression.yml) and requires it to pass,
then builds
offline help and the Windows NSIS package from that selected, validated SHA. It checks
the executable version against the tag and publishes
`FileSpacer-X.Y.Z-install.exe` and `FileSpacer-update.txt` to GitHub Releases.
Ordinary branch pushes do not publish an installer. The workflow uses GitHub's
automatic token; no custom release secret is required.

An exact successful result for unchanged application/test inputs may be reused
without rerunning the suite; see the [regression result cache](tests/regression/README.md#run-on-github).

Distribution is opt-in: by default, a release is published only on GitHub.
When selected, **Distribute FileSpacer** calls a reusable WinGet workflow to submit the
installer to the community catalog. It requires the one-time `WINGET_TOKEN`
secret; submission failures do not undo the GitHub release.
See [WinGet distribution](winget.md) for setup,
review, and retries without rebuilding the application.

When selected, Chocolatey distribution also runs independently after a successful release. It
uses the `CHOCO_API_KEY` secret, builds and smoke-tests the `filespacer` package,
then submits it for community moderation. See [Chocolatey distribution](chocolatey.md)
for setup, validation, and manual retries.

The common coordinator runs after successful publication and checks both queues
hourly, at minute 17. Each channel has one latest pending release and its own
acceptance barrier. If X awaits acceptance, Y is retained; if Z is requested
before X is accepted, Z replaces Y. Once X is accepted, only the retained version
is submitted. Older or replayed requests cannot replace a newer pending version.
Only releases selected for that channel, or explicitly requested manually, enter
its queue. A GitHub-only release never replaces a queued catalog release.

The shared `.github/scripts/distribution-queue.mjs` stores `state.json` in the
orphan branches `distribution-winget` and `distribution-chocolatey`, created on
first use with the workflow's `contents: write` permission. The branches contain
only state; they are not merged into `main`. Blob-SHA checks reject conflicting
writes. Native per-channel concurrency serializes short runs with `queue: max`;
the latest pending VERSION is maintained in state, not by leaving a runner waiting
for moderation. Channel failures do not cancel the other channel or the release.

Migration imports all open `Benco.FileSpacer` PRs. For Chocolatey, it checks each
published stable GitHub release using exact-version Chocolatey CLI searches,
including unapproved uploads. Any imported submissions must all be accepted
before a new version is sent. After migration, submit through the coordinator so
its state also tracks Chocolatey uploads; unrelated manual Chocolatey uploads
outside this workflow are not automatically discovered.

WinGet unblocks after the tracked PR is merged (catalog propagation can take
longer). Chocolatey unblocks only when the official CLI reports that exact
version as approved. Network failures, closed unmerged PRs and uncertain uploads
keep the barrier closed. A reservation is saved before each external upload, so
cancellation or a lost response cannot cause an automatic duplicate. The Actions
summary shows accepted, active and pending versions, with PR links when available.

This layout uses GitHub's [reusable workflows](https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows),
as illustrated by [WhatPulse's package coordinator](https://github.com/whatpulse/external-packages/blob/main/.github/workflows/release.yml).
Persistent branch state follows the same principle as [GitHub's lock action](https://github.com/github/lock),
without adding a dependency. Scheduled checks may be delayed by GitHub, and public
repositories' schedules are disabled after 60 days without repository activity.

Before publication, update both version definitions in `src/resource.h`, add
the matching `CHANGELOG.md` section, and commit the intended changes. From Git
Bash, substitute the intended version:

```bash
bash release.sh X.Y.Z
bash release.sh X.Y.Z --distribute winget
bash release.sh X.Y.Z --distribute chocolatey
bash release.sh X.Y.Z --distribute winget --distribute chocolatey
```

The first command selects no distribution; `--distribute none` makes this explicit.
Repeat `--distribute` to select several channels. `none` cannot be combined with
a channel; repeated channels are deduplicated. The script records the explicit
list in the tag annotation as `FileSpacer-Distribution: winget,chocolatey`
(or `none`), so a release retry retains the selection. Tags without this field
also default to no automatic distribution. If tagging with Git directly, use:

```bash
git tag -a vX.Y.Z -m "FileSpacer X.Y.Z" -m "FileSpacer-Distribution: winget,chocolatey"
git push origin vX.Y.Z
```

The script requires a clean tree, checks the remote branch/tag, previews the
changelog and selected channels, and asks for confirmation before pushing `main` and the annotated
tag. Check the workflow outcome and both release assets afterward. Failure
summaries/build logs remain workflow artifacts.

An existing GitHub release can be distributed later: run **Distribute FileSpacer**,
select `main`, its published tag and `channel=winget`, `chocolatey` or `all`.
A manual dispatch explicitly selects those channels regardless of
the original tag preference, and reuses the published installer and SHA-256.
From a client with GitHub CLI installed:

```bash
gh workflow run distribution.yml --ref main -f channel=winget -f tag=vX.Y.Z
gh workflow run distribution.yml --ref main -f channel=chocolatey -f tag=vX.Y.Z
```

Leave `tag` empty to reconcile existing queues without requesting a release.
If preparation fails before reservation (for example, a missing secret), the
pending release remains eligible for a later check. After reservation, inspect
the upstream PR/package and the failed run before requesting `retry=true`, with
that SAME tag and one channel. This permits another attempt only if the tracked
submission is missing or its WinGet PR was closed without merging; it cannot
bypass an open submission or discard a newer pending release. Never delete state
branches to recover a failure: losing an uncertain reservation can create duplicates.
Chocolatey's `dry_run=true`, with a tag and `channel=chocolatey`, validates the
package and real installation on the runner without reading or writing queue
state, reserving a submission or using the Chocolatey API key.

Do not rerun the release-generation workflow merely to submit to a catalog:
it rebuilds and can replace release assets, invalidating existing catalog checksums.

<a id="tests"></a>

## Tests

Use the [regression guide](tests/regression/README.md) for the same application
suite on a local Windows desktop, in Hyper-V or through GitHub Actions, including
case selection, prerequisites and report interpretation.
For example:

```cmd
tests.cmd -Category Internal
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

Run affected cases while developing. Tests use Debug and isolated test data;
never delete the production database to prepare a run. Read the
[Shell architecture](native-shell-view.md) before changing restoration or view
lifetime.

<a id="documented-exceptions-to-the-design-principles"></a>

## Documented exceptions to the design principles

FileSpacer waits for public events instead of polling on a timer. Two bounded
polling timers are accepted exceptions:

- `src/SettingsDialog.cpp` retries every 50 ms the `Local\` mutex that
  reserves the Preferences dialog while another FileSpacer process holds it
  with a property sheet not yet visible, because no public cross-process event
  announces that sheet and the mutex cannot be awaited from the message loop;
  the retry stops when the remote sheet is found, the mutex is acquired or the
  requesting window closes.
- `src/UpdateInstaller.cpp` samples every 200 ms the update worker's completion
  flag and received byte count, because the worker deliberately holds no window
  handle to post to (the page or progress dialog may be destroyed first) and
  the same tick refreshes the progress percentage; sampling stops at
  completion, cancellation or window destruction.
