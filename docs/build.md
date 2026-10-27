# Building FileSpacer on Windows x64

Target: Windows 10 or later, x64. Use Visual Studio 2022 Build Tools with MSVC,
ATL, and a Windows SDK. No IDE is required. This project uses MSVC/NMAKE; the
installer uses NSIS.

## Requirements

- Visual Studio 2022 Build Tools, installed in its standard location under
  `%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools`.
- Inkscape, normally at `C:\Program Files\Inkscape\bin\inkscape.com`.
- Python with Pillow, included in the Windows Inkscape distribution. The build
  uses `python.exe` beside `inkscape.com` by default; set `ICON_PYTHON` to use
  another Python interpreter with Pillow installed. NumPy is not required.
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

The Makefile runs `generate-icon.py` to regenerate `src\res\FileSpacer.ico` with
ten sizes (16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 pixels). For 16 to 48
pixels, the script draws the folder using the optical geometry and colors in
`FOLDER_MASTERS` and `draw_folder`: edges fit each pixel grid, the back is flat,
and the front has a simplified vertical gradient. It reads the nine projected
tesseract polygons and their colors from `src\res\FileSpacer.svg`, rasterizes
them on one shared grid at 32 times the target size, and reduces them with Box
filtering. The logo coordinates are already in the SVG coordinate system; an
absent matrix transform is treated as the identity. These small variants are
recalculated from the script and SVG, without stored PNGs.

The SVG includes the original 4D vertices, their 3D projection and the camera
parameters in its `tesseract-scene` metadata. The purple-to-green cube edge ratio
is 11/20 in 3D, before the final 2D projection; the camera uses a 45-degree azimuth,
30-degree elevation and distance of six outer-cube edges. The ICO generator reads
the resulting 2D polygons, preserving the accepted geometry at every icon size.

Inkscape renders the four larger sizes directly from the SVG. ImageMagick packs
32-bit images with alpha, with lossless PNG compression for the 256-pixel image.
The script checks all ten decoded RGBA images against its generated PNGs before
replacing the ICO and removes its temporary files. Changes to the SVG, generator
or Makefile regenerate the icon; a clean build uses the same generation path.
The small-folder optical masters are specific to the centered 212 x 212 artwork:
update the script when changing that folder design or its coordinate system.

`build.cmd installer` always repackages the current Release executable and
accompanying notices; it does not launch the installer.

## SQLite

SQLite 3.53.4 is bundled in `third_party\sqlite`. The unmodified upstream
amalgamation is compiled separately as C and linked statically into FileSpacer.
No SQLite DLL or command-line tool is required, and the build does not download
SQLite. See [the SQLite notes](../third_party/sqlite/README.txt) for its origin and archive hash.

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

`tests.cmd` runs locally by default. Add `-VM AgentWin10` to execute in the
configured Hyper-V client, while keeping application and test-helper compilation
on the host. Existing `-Category`, `-Pattern`, `-TimeoutSeconds` and
`-RunDirectory` options also apply to VM runs. For example:

```cmd
tests.cmd -VM AgentWin10 -Category UI -Pattern "16-redimensionnement-alt\03-*"
```

An unconfigured VM fails explicitly. There is no automatic local UI fallback.
See [the regression guide](../tests/regression/README.md) for setup and reports.

From the project root, close FileSpacer and Control Panel windows, then run:

```cmd
tests.cmd
```

This is the reference regression suite, including one real native
Shell/SQLite persistence test in the production spatial configuration. It
uses the same MSVC/NMAKE build above. Do not also run the native launcher
when the full suite already ran group 18.
The external Everything test requires a closed, installed copy of Everything;
its fixtures and index are isolated. See the test guide for its prerequisites.

For a targeted run, use `-Category` or `-Pattern`; see
[the test guide](../tests/regression/README.md). The retained native test can
also be invoked alone as `test-native-state.cmd --diagnostic`. It uses `/Z7`
and initializes MSVC x64 itself; no shared compiler PDB is required. The
exploratory profile comparisons and Control Panel embedding probes have
been retired. Their sources remain in the migration branch history.

FileSpacer uses the public Shell integration described in
[the architecture note](native-shell-view.md), which distinguishes the remaining
native-view limits from behaviors restored since the initial 1.1.0 migration.
