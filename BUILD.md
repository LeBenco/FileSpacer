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

The runner locates MSVC with `vswhere` and invokes the same NMAKE targets directly
because its Visual Studio edition differs from the local Build Tools installation.

Update checking downloads the manifest from
`https://github.com/LeBenco/FileSpacer/releases/latest/download/FileSpacer-update.txt`.
The workflow generates it from the built executable and the renamed NSIS
installer, using ASCII without a BOM and LF line endings:

```text
FSUP
01000500
https://github.com/LeBenco/FileSpacer/releases/tag/v1.0.5
https://github.com/LeBenco/FileSpacer/releases/download/v1.0.5/FileSpacer-1.0.5-install.exe
<64 lowercase hexadecimal SHA-256 characters>
<installer length in bytes>
```

The eight hexadecimal digits encode the four version components, one byte each
(major, minor, patch, revision). Each component must be between 0 and 255. URLs
must match the version and this repository. The final two lines are generated
with `Get-FileHash -Algorithm SHA256` and the installer file length. They must not
be edited independently of the executable uploaded to the release.

Older clients still read the first three lines and open the release page. The
new client also accepts old three-line manifests, offering the release page
when installer metadata is absent. Direct installation needs all six lines.
GitHub's latest-release URL selects stable releases; prereleases are not offered
by the updater. A release must contain the manifest for checks to succeed.

### Update interface and lifecycle

Opening Settings > About checks asynchronously. The existing check button becomes
“Install FileSpacer vX.Y.Z” when installable metadata for a newer version is
available; otherwise it remains available to check again. A status label reports
checking, no update, availability or errors. Weekly checks retain their notification.

Installing asks for confirmation that all windows will close after the download.
Pending Settings changes are applied, then a separate modeless progress dialog
opens and Settings closes, allowing folder windows to remain usable. Cancel or
the progress window's close button cancels without closing any folder window.
Workers keep the process alive until their bounded network operation and cleanup
finish, even if their dialog has been destroyed.

The client verifies exact byte length and SHA-256 before starting a temporary
copy of its executable as an installation relay. The relay verifies the file again
and holds it without write/delete sharing. It requests a cooperative exit of all
processes using that installed executable, including recovery helpers and
headless COM servers, and waits at most 60 seconds. Each process closes its own
windows normally, preserves the existing saves, suspends new COM activations and
finishes its workers. The application never calls `TerminateProcess`.

A signaled installation gate prevents new application launches during shutdown
and installation. Failure to shut down leaves setup unstarted and restores COM
activation for remaining instances; some windows may already have closed. The
relay launches the existing interactive NSIS installer (including its normal UAC
prompt) and releases the gate after the installer exits. Cancellation of UAC or
setup can therefore require reopening FileSpacer manually.

Implementation is contained in `Update.cpp`, `UpdateManifest.cpp` and
`UpdateInstaller.cpp`. `ProcessLifetime.cpp` is a generic application exit endpoint.
Settings, main and folder creation only provide small lifecycle/UI hooks. The
folder-state, navigation, reopen history and Jump List implementations are unchanged.
Temporary installer data is removed on completion/error/cancellation; the mapped
relay image is cleaned on a later update check once it is at least 24 hours old.

### Parser regression tests

From an initialized x64 MSVC CMD prompt at the repository root:

```cmd
if not exist build mkdir build
cl.exe /nologo /EHsc /W4 /std:c++14 /I src src\UpdateManifest.cpp tests\update_manifest_test.cpp /Fe:build\update-manifest-test.exe
build\update-manifest-test.exe
```

The test is also portable C++14 and can run under AddressSanitizer/UBSan. It covers
legacy and installer manifests, wrong hosts/versions, malformed checksums, size
limits, truncation and 50,000 deterministic malformed inputs.
