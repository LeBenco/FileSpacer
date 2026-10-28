# Documentation screenshots

These PNGs and GIFs are stored in the repository, alongside the documentation.
Markdown uses relative paths, so GitHub displays them directly and the website generator
copies the same files into the GitHub Pages build. No separate image host or
Git LFS setup is needed.

| File | Purpose |
| --- | --- |
| `overview.png` | A single folder window with freely positioned demo items. |
| `window-menu.png` | The title menu, open folders and the saved-layout lock. |
| `search.png` | Local search history and the optional Everything action. |
| `shortcuts.png` | Settings with the movement action expanded. |
| `locked-title.png` | A close-up of the locked folder's title and padlock badge. |
| `move-window.gif` | Alt + left drag to move a folder window. |
| `resize-window.gif` | Alt + right drag to resize a folder window. |

The website and English documentation use the English series, with fictional
English folder names and contents. See `provenance.json` for the capture context,
dimensions and checksums.

The English captures use the existing en-US resources, selected at startup in a
disposable documentation build of FileSpacer 1.1.10 because the VM's active
Windows session remains French. The source UI strings and controls are unchanged.

The original French screenshots are kept as `overview-fr.png`,
`window-menu-fr.png`, `search-fr.png` and `shortcuts-fr.png` for future French
documentation. Their capture context and checksums are in `provenance-fr.json`.
They are not included in the current website build.

The title close-up also has a French copy, `locked-title-fr.png`.

The animations also have French copies, `move-window-fr.gif` and
`resize-window-fr.gif`. They show the VM's Windows wallpaper around the window
so that its movement and changing size remain visible.

## Recording an animation

Use ScreenToGif with the cursor and mouse-click highlights enabled. Add an
automatic **Key strokes** task and disable **Ignore modifier keys**: otherwise
Alt pressed on its own is hidden. Keep the overlay visible throughout each
gesture and check the exported GIF, including frames in the middle of the drag.
Use a short loop with pauses before and after the gesture.

## Updating a screenshot

Replace the existing PNG or GIF under the same name when the corresponding
interface changes. There is no need to recapture unchanged screens for every release.
Keep the app version and capture context in `provenance.json` current.

Use a dedicated VM with fictional folders and files. Prefer an English Windows
display language, since FileSpacer follows Windows. Preserve the real interface;
do not redraw or translate labels inside a screenshot.

For static screenshots, capture only the visible window frame, including its
title bar. Keep menus and suggestions entirely inside that frame. Exclude the desktop, taskbar, shadows,
invisible resize borders and operating-system overlays. A window can be moved
or resized before capture to keep overlays outside its bounds.

Check each PNG at its original size, then run:

```cmd
npm test --prefix website
npm run generate --prefix website
```

The Pages workflow also watches `docs/images/**`, so an image-only replacement
rebuilds the website after it is pushed to `main`.
