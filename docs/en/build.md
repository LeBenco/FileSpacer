# Building FileSpacer on Windows x64

FileSpacer targets Windows 10 or later, x64. It uses MSVC/NMAKE and NSIS, not
CMake. Run build and test commands from CMD at the repository root. See the
[developer overview](README.md) for the code map.

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

After a successful release workflow, a separate WinGet workflow submits the
installer to the community catalog. It requires the one-time `WINGET_TOKEN`
secret; submission failures do not undo the GitHub release.
See [WinGet distribution](winget.md) for setup,
review, and retries without rebuilding the application.

Before publication, update both version definitions in `src/resource.h`, add
the matching `CHANGELOG.md` section, and commit the intended changes. From Git
Bash, substitute the intended version:

```bash
bash release.sh X.Y.Z
```

The script requires a clean tree, checks the remote branch/tag, previews the
changelog and asks for confirmation before pushing `main` and the annotated
tag. Check the workflow outcome and both release assets afterward. Failure
summaries/build logs remain workflow artifacts.

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
