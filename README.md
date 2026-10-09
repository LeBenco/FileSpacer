# FileSpacer

FileSpacer is a file manager for Microsoft Windows with a fully spatial interface:
folders open in separate windows that retain their position, size, and display
settings. By default, opening a folder closes the window it was opened from,
avoiding the accumulation of windows and keeping the desktop uncluttered.

Windows File Explorer does not provide all the behaviors needed for a consistent,
efficient spatial workflow. FileSpacer aims to provide them:

- Each folder is represented by a single window, permanently and unambiguously tied to that folder.
- Folders retain their spatial state: window position, size, and view settings.
- A folder window follows its folder when it is moved or renamed.
- When a folder is deleted, its window closes.
- Windows' Auto-arrange feature can be turned off, allowing icons to be positioned
  freely. Their positions are saved with the folder settings and restored the next
  time the folder is opened.
- By default, opening a folder closes the source window. This behavior can be
  reversed in Settings; holding Ctrl reverses it for an individual operation.

FileSpacer is a fork of [ChromaFiler](https://github.com/vanjac/chromafiler),
created by J. van't Hoog. It started as a small fork to remove the Miller-column
approach, which I am not particularly fond of, and quickly grew into a project of
its own. Many thanks to vanjac: his work provided an excellent foundation!

## Build and use

Windows 10 or later, x64. The interface includes English and French resources.
See [BUILD.md](BUILD.md) for the MSVC/NMAKE build and NSIS installer, and the
[user guide](docs/user-guide.md) for navigation and maintenance commands.

## License and privacy

FileSpacer is distributed under the [GNU General Public License v3.0](LICENSE).
Copyright (c) 2026 LeBenco, for the FileSpacer modifications.
Original ChromaFiler code: Copyright (c) 2023 J. van't Hoog.

See [third-party notices](THIRD_PARTY_NOTICES.md) and the
[privacy policy](docs/privacy-policy.md).
