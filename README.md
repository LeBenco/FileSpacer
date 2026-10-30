<p align="center">
    <img height="200" alt="FileSpacer" src="src/res/FileSpacer.svg" />
</p>

# FileSpacer

FileSpacer is a file manager for Windows built around the [spatial paradigm](https://en.wikipedia.org/wiki/Spatial_file_manager): each folder has a single window that remembers its position, size and view settings. This provides [stable visual landmarks](https://www.csse.canterbury.ac.nz/~andy/papers/transforms.pdf), allowing [spatial memory](https://www.microsoft.com/en-us/research/wp-content/uploads/1998/01/p153-robertson.pdf) to help you recognize familiar folders at a glance. In a navigational file manager, the same window displays different folders in succession: its position and dimensions alone cannot identify the current folder, so you need to look at its contents to determine where you are. By preserving distinct landmarks for each folder, the spatial approach can help reduce the cognitive load of finding your way around your files.

To address the accumulation of windows, a recognized drawback of the spatial paradigm, FileSpacer closes the source window by default when a folder is opened. Hold Ctrl while opening a folder to keep the source window open when you want to work with several folders at once.

<img src="docs/images/spatial-windows.png" alt="Three non-overlapping folder windows with different sizes, view modes and icon scales, including a free icon layout." width="560">

FileSpacer keeps its interface deliberately understated, unlike some cluttered interfaces that leave just enough room between the buttons to glimpse a file.

Tabs are incompatible with the spatial model and will never be added.

Windows 10 or later, x64. Available in English, French, German, Italian, Spanish,
Portuguese (Portugal) and Greek.

No telemetry, usage tracking or advertising. Update checks contact GitHub; weekly checks are optional and off by default. Downloading and installing require confirmation. [Privacy](docs/user-guide.md#privacy).

[Download the latest release](https://github.com/LeBenco/FileSpacer/releases/latest)
· [User guide](docs/user-guide.md)
· [Changelog](CHANGELOG.md)

## Features

- **One window per folder**, reused if the folder is already open. Its position,
  size, view settings and icon layout are restored between visits.
  [Spatial navigation](docs/user-guide.md#window-navigation).
- **Arrange icons freely**, with their positions saved and restored for each
  folder. [Icon layout](docs/user-guide.md#icon-layout).
- **Move and resize windows** from anywhere in the file area, preserving
  selection. Mouse shortcuts are configurable; defaults are Alt + left drag
  and Alt + right drag. [Window gestures](docs/user-guide.md#moving-and-resizing).
- **Switch between open folders** from the title-bar menu, close parent
  windows, or reopen the last closed window with Ctrl+Shift+T.
  [Window navigation](docs/user-guide.md#window-navigation).
- **Lock a window's saved layout** while allowing temporary changes to its
  position, size and view settings.
  [Window state locking](docs/user-guide.md#locking-a-saved-layout).
- **Filter the current folder** and reuse its recent searches.
  [Search and history](docs/user-guide.md#search-and-history).
- **Preserve selection when activating a window** by clicking its background.
  [Selection and activation](docs/user-guide.md#selection-and-activation).
- **Reduce overlap between windows** when opening folders without a saved
  position. [Window placement](docs/user-guide.md#window-placement).

## Integration

FileSpacer can be set as the default file manager during installation or later in Settings. Files open with their usual Windows applications; network folders use Windows' usual handling. [Windows integration](docs/user-guide.md#windows-integration).

With **Everything**, advanced searches can be launched from FileSpacer's search field, in the current folder and its subfolders or across all indexed files. Results open in Everything. [Integration and usage](docs/user-guide.md#everything).

With **Nilesoft Shell**, FileSpacer's menus can use its themes and custom commands. Configuration adjustments are recommended for a consistent appearance. [Integration and configuration](docs/user-guide.md#nilesoft-shell).

## Build

Builds use MSVC/NMAKE, Visual Studio 2022 Build Tools with ATL and a Windows SDK,
Inkscape and ImageMagick. NSIS is required for the installer; SQLite is bundled.
See the [build guide](docs/build.md).

Regression tests combine PowerShell scenarios with C++ and C# helpers for
internal checks and real Windows UI/Shell tests. Run them through `tests.cmd`,
locally or in a configured Hyper-V VM.
See the [test guide](tests/regression/README.md).

---

FileSpacer is a fork of [ChromaFiler](https://github.com/vanjac/chromafiler),
created by J. van't Hoog (vanjac). Many thanks to vanjac for the foundation!

Distributed under the [GNU GPL v3.0](LICENSE).
Copyright © 2026 Benco for FileSpacer modifications;
original ChromaFiler code © 2023 J. van't Hoog.
See [third-party notices](THIRD_PARTY_NOTICES.md)
and the [privacy policy](docs/privacy-policy.md).
