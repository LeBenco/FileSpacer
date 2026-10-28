# Documentation screenshots

These PNGs are stored in the repository, alongside the documentation. Markdown
uses relative paths, so GitHub displays them directly and the website generator
copies the same files into the GitHub Pages build. No separate image host or
Git LFS setup is needed.

| File | Purpose |
| --- | --- |
| `overview.png` | A single folder window with freely positioned demo items. |
| `window-menu.png` | The title menu, open folders and the saved-layout lock. |
| `search.png` | Local search history and the optional Everything action. |
| `shortcuts.png` | Settings with the movement action expanded. |

The current series shows FileSpacer 1.1.10 on a French Windows 10 guest, with
fictional English folder names and contents. See `provenance.json` for the
capture context, dimensions and checksums.

## Updating a screenshot

Replace the existing PNG under the same name when the corresponding interface
changes. There is no need to recapture unchanged screens for every release.
Keep the app version and capture context in `provenance.json` current.

Use a dedicated VM with fictional folders and files. Prefer an English Windows
display language, since FileSpacer follows Windows. Preserve the real interface;
do not redraw or translate labels inside a screenshot.

Capture only the visible window frame, including its title bar. Keep menus and
suggestions entirely inside that frame. Exclude the desktop, taskbar, shadows,
invisible resize borders and operating-system overlays. A window can be moved
or resized before capture to keep overlays outside its bounds.

Check each PNG at its original size, then run:

```cmd
npm test --prefix website
npm run generate --prefix website
```

The Pages workflow also watches `docs/images/**`, so an image-only replacement
rebuilds the website after it is pushed to `main`.
