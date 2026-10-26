# Building FileSpacer on Windows x64

Target: Windows 10 or later, x64. Use Visual Studio 2022 Build Tools with MSVC,
ATL, and a Windows SDK. No IDE is required. This project uses MSVC/NMAKE; the
installer uses NSIS.

## Requirements

- Visual Studio 2022 Build Tools, installed in its standard location under
  `%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools`.
- Inkscape, normally at `C:\Program Files\Inkscape\bin\inkscape.com`.
- ImageMagick, with the `magick` command available on PATH.
- NSIS, normally at `%ProgramFiles(x86)%\NSIS\makensis.exe`, for the installer.

In CMD at the project root, close FileSpacer before building:

```cmd
build.cmd debug
build.cmd release
build.cmd installer
```

`build.cmd` initializes the x64 environment with `vcvars64.bat` and invokes NMAKE.
The outputs are `build\debug\FileSpacer.exe`, `build\release\FileSpacer.exe`,
and `build\FileSpacer-install.exe`. Debug uses `FILESPACER_DEBUG`, `/MTd`, and the
console subsystem. Release uses `/O2`, `/MT`, and the Windows subsystem.
Distribute the Release installer.

To override nonstandard tool locations in CMD:

```cmd
set "INKSCAPE=D:\Tools\Inkscape\bin\inkscape.com"
set "MAKENSIS=D:\Tools\NSIS\makensis.exe"
build.cmd installer
```

The Makefile exports `src\res\FileSpacer.svg` to a multi-resolution ICO using
Inkscape and ImageMagick. `build.cmd installer` always repackages the current
Release executable and accompanying notices; it does not launch the installer.

## SQLite

SQLite 3.53.4 is bundled in `third_party\sqlite`. The unmodified upstream
amalgamation is compiled separately as C and linked statically into FileSpacer.
No SQLite DLL or command-line tool is required, and the build does not download
SQLite. See `third_party\sqlite\README.txt` for its origin and archive hash.

SQLite uses `/W3`; project C++ uses `/W4`. `_CRT_SECURE_NO_WARNINGS` applies only
to SQLite. `SQLITE_OMIT_LOAD_EXTENSION` disables extension loading and
`SQLITE_DQS=0` requires standard SQL string literals. Default thread safety is
retained. Both use the same static runtime. Release uses `/OPT:REF /OPT:ICF`.

Running the same command with no source changes should leave the executable up
to date. Packaging remains forced when the installer target is requested.

```cmd
build.cmd clean
```

This removes Debug/Release outputs, the generated ICO, and the installer. It
preserves source files and user data. The next build regenerates the icon.

From an x64 developer prompt, inspect binary dependencies with:

```cmd
dumpbin /dependents build\release\FileSpacer.exe
```

No dependency on `sqlite3.dll` should appear. Windows system DLLs remain required.

## GitHub releases

The release workflow runs only when a new `vX.Y.Z` tag is pushed. It builds Release
and the NSIS installer on `windows-2022`, verifies the executable version against
the tag, and publishes `FileSpacer-X.Y.Z-install.exe` and `FileSpacer-update.txt`
to the fork's GitHub Release. The workflow can also be run manually for an
existing tag.
Ordinary branch pushes do not build or publish an installer. No custom secret is
needed; the workflow uses GitHub's automatic token.

To publish from Git Bash, first update both version definitions in
`src/resource.h` and add the matching section to `CHANGELOG.md`. Commit all
source, test and documentation changes on `main`. For example:

```bash
bash release.sh 1.1.0
```

The script requires a clean working tree, checks the remote branch and tag,
previews the changelog section, then asks for confirmation before pushing `main`
and the annotated tag. The GitHub Actions workflow performs the installer build
and publication. Check that it succeeds and that the release contains both the
versioned installer and `FileSpacer-update.txt`.

## Regression tests

From the project root, close FileSpacer and Control Panel windows, then run:

```cmd
tests.cmd
```

This is the reference regression suite: 111 cases, including one real native
Shell/SQLite persistence test in the production spatial configuration. It
uses the same MSVC/NMAKE build above. Do not also run the native launcher
when the full suite already ran group 18.
The external Everything test requires a closed, installed copy of Everything;
its fixtures and index are isolated. See the test guide for its prerequisites.

For a targeted run, use `-Category` or `-Pattern`; see
[the test guide](tests/regression/README.md). The retained native test can
also be invoked alone as `test-native-state.cmd --diagnostic`. It uses `/Z7`
and initializes MSVC x64 itself; no shared compiler PDB is required. The
exploratory profile comparisons and Control Panel embedding probes have
been retired. Their sources remain in the migration branch history.

Version 1.1.0 uses the public Shell integration described in
[the architecture note](docs/native-shell-view.md), including the three
explicitly accepted behavior differences.
