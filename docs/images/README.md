# Documentation screenshots

These PNGs and GIFs are stored in the repository, alongside the documentation.
Markdown uses relative paths, so GitHub displays them directly and the website generator
copies the same files into the GitHub Pages build. No separate image host or
Git LFS setup is needed.

| File | Purpose |
| --- | --- |
| `spatial-windows.png` | Three non-overlapping windows with different geometry, view modes and icon sizes, including a free layout. |
| `selected-name.png` | A long selected filename displayed in full. |
| `everything-search.png` | A FileSpacer query alongside the actual Everything results. |
| `overview.png` | Earlier single-window overview, retained for reference. |
| `window-menu.png` | The title menu, open folders and the saved-layout lock. |
| `search.png` | Local search history and the optional Everything action. |
| `mixed-sorting.png` | Folders and files sharing one alphabetical order, with the default navigation bar visible. |
| `display.png` | Earlier Display settings capture, retained for reference. |
| `locked-title.png` | A close-up of the locked folder's title and padlock badge. |
| `move-window.gif` | Alt + left drag to move a folder window. |
| `resize-window.gif` | Alt + right drag to resize a folder window. |

The English documentation and English website use the unsuffixed series, with
fictional English folder names and contents. Each translated website substitutes
the corresponding localized series: `-fr`, `-de`, `-it`, `-es`, `-pt` or `-el`.
Markdown keeps the original image references; the generator selects and copies
the localized files. See `provenance.json` for the original capture context,
dimensions and checksums.

The original English captures use the existing en-US resources, selected at startup in a
disposable documentation build of FileSpacer 1.1.10 because the VM's active
Windows session remains French. The source UI strings and controls are unchanged.

The original French screenshots, including `overview-fr.png`,
`window-menu-fr.png` and `search-fr.png`, are used by the French website.
Their capture context and checksums are in `provenance-fr.json`.

The title close-up also has a French copy, `locked-title-fr.png`.

The animations also have French copies, `move-window-fr.gif` and
`resize-window-fr.gif`. They show the VM's Windows wallpaper around the window
so that its movement and changing size remain visible.

The Display settings capture also has a French copy, `display-fr.png`.
These two images were recorded from a development build, as described in
`display-provenance.json`. The option is available starting with release 1.2.0;
the capture provenance is retained as its historical record.

The current guide illustrates the actual mixed order with `mixed-sorting.png`.
Its six localized counterparts use translated fictional folder and file names.
All seven show FileSpacer 1.2.3, with the preference enabled and icons
automatically arranged by name. The default path bar remains visible, with
Reload to its right and Quick access visible. Their capture context and checksums are in
`mixed-sorting-provenance.json`.

## Nilesoft Shell comparisons

The `nilesoft-navigation-default.png` / `nilesoft-navigation-adapted.png` and
`nilesoft-main-menu-default.png` / `nilesoft-main-menu-adapted.png` pairs show the
same Modern theme with its default rounded corners and with the recommended
square corners. Each file has a localized copy for every translated website.

See `nilesoft-provenance.json` for versions, dimensions and checksums. Keep the
same menu contents, window geometry and theme in each pair. Change only the
conditional corner-radius rule documented in `../nilesoft-shell.md`.

Use English Windows for new English menu captures when possible. The current
English series uses the application's English resources and real Nilesoft
label rules for native Windows menu entries, applied identically to both
variants on the French VM. No labels were edited in the exported images.
Back up and restore the installed Nilesoft configuration around each session.

## Additional language series

`localization-provenance.json` records the 45 PNGs and 10 GIFs added for German,
Italian, Spanish, European Portuguese and Greek, including checksums, dimensions,
VM run IDs, application version and recorded keystroke frames. The existing
English and French series remain valid for unchanged screens.

These five series were originally captured from FileSpacer 1.2.2 in the documentation VM.
Their mixed-sorting records now identify the updated 1.2.3 captures explicitly.
Disposable builds select the application's real language resources at startup
and retain the real Windows controls. Spanish selects resource ID `040A`
explicitly, matching the resource file's Spanish (traditional sort) declaration.
Fictional filenames, layouts and gesture recordings match the English examples;
the established French examples retain their translated fictional filenames.
Nilesoft's actual label rules localize native Windows menu entries on the French
VM identically in both corner variants. Exported image labels were not edited.

## User-guide scenes, FileSpacer 1.2.3

`user-guide-provenance.json` records 21 real captures: the three new scenes in
English and all six translations, including hashes, dimensions, executable
hashes, VM run IDs, window geometry and verified Everything queries/results.
They use matching resources from released commit `10c4027`; only language
selection is overridden in disposable documentation builds. Older captures
remain available for reference.

The VM's Windows session is French, so Windows-owned breadcrumb labels and
size formatting may remain French. Everything has no European Portuguese
translation; its official Brazilian Portuguese interface is used beside
FileSpacer and fictional names in European Portuguese.

The scene with Everything shows real results launched from FileSpacer. Because
a successful launch clears FileSpacer's field, the same query is entered again
afterwards to make the relationship visible in one screenshot. No image labels
or pixels are retouched. The temporary display resolution, existing default
Windows wallpaper and private drive mapping are restored after each capture.

## Recording an animation

Use ScreenToGif with the cursor and mouse-click highlights enabled. Add an
automatic **Key strokes** task and disable **Ignore modifier keys**: otherwise
Alt pressed on its own is hidden. Keep the overlay visible throughout each
gesture and check the exported GIF, including frames in the middle of the drag.
Use a short loop with pauses before and after the gesture.

## Updating a screenshot

Replace the existing PNG or GIF under the same name when the corresponding
interface changes. There is no need to recapture unchanged screens for every release.
Update the corresponding provenance file with the app version and capture context.

Use a dedicated VM with fictional folders and files. Prefer the target Windows
display language, since FileSpacer follows Windows. Preserve the real interface;
do not redraw or translate labels inside a screenshot.

For single-window static screenshots, capture only the visible window frame, including its
title bar. Keep menus and suggestions entirely inside that frame. Exclude the desktop, taskbar, shadows,
invisible resize borders and operating-system overlays. A window can be moved
or resized before capture to keep overlays outside its bounds. Multi-window
scenes include the smallest rectangle containing their visible frames, with
only the existing Windows wallpaper in the gaps.

Check each PNG at its original size, then run:

```cmd
npm test --prefix website
npm run generate --prefix website
```

The Pages workflow also watches `docs/images/**`, so an image-only replacement
rebuilds the website after it is pushed to `main`.
