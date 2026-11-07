<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

[![Download latest](https://img.shields.io/badge/Download-latest-blue)](https://github.com/LeBenco/FileSpacer/releases/latest) &emsp;
[![Windows 10+](https://img.shields.io/badge/Windows%2010%2B-0078D6?logo=data:image/svg%2Bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0iI2ZmZiIgZD0iTTAgMGgxMXYxMUgwem0xMyAwaDExdjExSDEzem0tMTMgMTNoMTF2MTFIMHptMTMgMGgxMXYxMUgxM3oiLz48L3N2Zz4=)](#system--languages)
[![No Telemetry](https://img.shields.io/badge/telemetry-none-2ea44f)](#privacy)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](#license-and-credits)

FileSpacer is a file manager for Windows built around the [spatial paradigm](https://en.wikipedia.org/wiki/Spatial_file_manager): each folder has a single window that remembers its position, size and view settings. This provides [stable visual landmarks](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), allowing [spatial memory](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) to help you recognize familiar folders at a glance. In a navigational file manager, the same window displays different folders in succession: its position and dimensions alone cannot identify the current folder, so you need to look at its contents to determine where you are. By preserving distinct landmarks for each folder, the spatial approach can help reduce the cognitive load of finding your way around your files. Tabs are incompatible with the spatial model and will never be added.

<img src="docs/en/images/spatial-windows.png" alt="Three non-overlapping folder windows with different sizes, view modes and icon scales, including a free icon layout." width="560">

FileSpacer prioritizes a lightweight and uncluttered experience. To address the accumulation of windows, a recognized drawback of the spatial paradigm, FileSpacer closes the source window by default when a folder is opened. Hold Ctrl while opening a folder to keep the source window open when you want to work with several folders at once. FileSpacer also keeps its interface deliberately understated, unlike some cluttered interfaces that leave just enough room between the buttons to glimpse a file.

<a id="system--languages"></a>**System & Languages:** Windows 10 or later, x64. Available in ten languages: English, French, German, Italian, Spanish, Portuguese (Portugal), Greek, Polish, Dutch and Ukrainian.

<a id="privacy"></a>**Privacy:** **No telemetry, usage tracking or advertising**. Update checks contact GitHub. Weekly checks are optional and off by default. Downloading and installing require confirmation. [Privacy policy](docs/en/user-guide.md#privacy).

**Install:**

- **Manual installation:** download and run <a href="https://github.com/LeBenco/FileSpacer/releases/download/v1.3.2/FileSpacer-1.3.2-install.exe" id="last-release-url">the latest release installer</a>.
- **WinGet:** `winget install --id Benco.FileSpacer --exact`
- **Chocolatey** (administrator terminal): `choco install filespacer -y`

WinGet and Chocolatey packages are awaiting catalog approval.

**Documentation:**

[![Docs](https://img.shields.io/badge/Docs-User%20Guide-blue)](docs/en/user-guide.md)
[![Docs](https://img.shields.io/badge/Docs-Changelog-orange)](CHANGELOG.md)

_FileSpacer is a fork of [ChromaFiler](https://github.com/vanjac/chromafiler), originally created by J. van't Hoog. See [Credits](#license-and-credits) below for details._

## Features

- **One window per folder**. Its position, size, view mode and icon positions [are remembered](docs/en/user-guide.md#window-navigation). Icons can be arranged freely or aligned to the [standard positioning grid](docs/en/user-guide.md#icon-layout).
- **Automatic window organization**. Opening another folder closes the source window by default. Windows without a saved position are placed to [limit overlap](docs/en/user-guide.md#window-placement).
- **Manage windows**. Move or resize windows using [mouse shortcuts](docs/en/user-guide.md#moving-and-resizing). Save and reopen [groups of folders](docs/en/user-guide.md#folder-groups). [Lock window layouts](docs/en/user-guide.md#locking-a-saved-layout).

## Integration

FileSpacer can be set as the default file manager during installation or later in Settings. Files open with their usual Windows applications. Network folders use Windows' usual handling. [Windows integration](docs/en/user-guide.md#windows-integration).

With **Everything**, advanced searches can be launched from FileSpacer's search field, in the current folder and its subfolders or across all indexed files. Results open in Everything. [Integration and usage](docs/en/user-guide.md#everything).

With **Nilesoft Shell**, FileSpacer's menus can use its themes and custom commands. Configuration adjustments are recommended for a consistent appearance. [Integration and configuration](docs/en/user-guide.md#nilesoft-shell).

## Build

[![Publish FileSpacer](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml/badge.svg)](https://github.com/LeBenco/FileSpacer/actions/workflows/release.yml)

Builds use MSVC/NMAKE, Visual Studio 2022 Build Tools with ATL and a Windows SDK,
Inkscape and ImageMagick. NSIS is required for the installer. SQLite is bundled.
See the [build guide](docs/en/build.md).

Regression tests combine PowerShell scenarios with C++ and C# helpers for internal checks and real Windows UI/Shell tests. The [GitHub Actions regression workflow](https://github.com/LeBenco/FileSpacer/actions/workflows/regression.yml) runs this application suite on demand and automatically before a release. The same cases can be run with `tests.cmd` on a local Windows desktop or in a configured Hyper-V VM. See the [test guide](docs/en/tests/regression/README.md) for case selection, prerequisites and results.

## License and credits

### Copyright and License
FileSpacer is distributed under the <img align="absbottom" src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3"> license.<br>
Copyright © 2026 Benco for FileSpacer modifications. The complete license is in [LICENSE](LICENSE) in the source tree, or `LICENSE.txt` in the installation folder. FileSpacer comes without warranty. Redistribution and modification are permitted under that license.

### Fork and Upstream Credits
FileSpacer is a fork of [ChromaFiler](https://github.com/vanjac/chromafiler) (original ChromaFiler code © 2023 J. van't Hoog.), created by J. van't Hoog (vanjac). Many thanks to vanjac for the foundation!

### Third-Party Notices
See [third-party notices](THIRD_PARTY_NOTICES.md) and the [privacy policy](docs/en/privacy-policy.md).
