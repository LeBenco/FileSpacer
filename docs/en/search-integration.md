# Search, history and Everything

Start in `SearchBox`, `FolderWindow::applyNameFilter`, `ShellViewFilter` and
`Everything`. For interaction details, see the
[user guide](user-guide.md#search-and-history).

## Local filtering and suggestions

SearchBox separates explicit submission from its 150 ms typing debounce. The
local filter is applied through the Shell; suggestions use a nonactivating
Win32 popup and native owner-drawn list box. The edit retains focus. There is
one history store and one entry builder for Everything and recent queries.

No suggestion is selected until keyboard navigation or a mouse press. Hover
alone cannot select Everything; editing clears a previous selection. Enter with
no selected suggestion remains local. Escape dismisses without clearing text.
Tab, focus loss, owner movement/deactivation and destruction dismiss the popup.
Keep its strings accessible and its bounds inside the monitor work area.

Only a successful explicit local submission, recent-query selection or external
launch records a nonblank query. Typing alone must not fill the history.

## Launching Everything

`EverythingEnabled` defaults to false. `EverythingPath` is empty for automatic
detection; `EverythingScope` selects the current filesystem folder and its
descendants (0) or everywhere (1). Detection checks both Registry views, App
Paths, voidtools installation entries and standard program locations.

Pass one `-search` argument. Folder scope prepends a quoted path ending in `\`
and groups the query with `<…>` to preserve OR precedence. The query itself
remains Everything syntax. Its embedded quotes use Everything's triple-quote
convention, distinct from Windows CRT quoting of the executable name.
`CreateProcessW` receives a separate absolute executable path and writable
Unicode command buffer, without a command interpreter.

Process creation is only launch success, not proof of indexing or usable
results. On successful launch, clear the local filter; on failure, preserve
the query/filter and report the error.

## History persistence

`SearchHistoryLimit` defaults to 5 and accepts 0–20. The SQLite
`search_history(sequence,key,query)` table is separate from view state and
closed-window history. Sequence numbers order submissions reliably even within
the same second. State-save coalescing defers writes; normal closure flushes them.

Changing the limit trims every stored folder transactionally, including closed
folders. Notifications carry the applied limit and normal/test namespace, so
delayed messages cannot resurrect a cleared cache or modify production data
from a test window.

History follows path-identity changes. The `folder_state` deletion trigger
removes it on confirmed deletion, qualified expiration, reset or quota eviction.
Writes require an existing folder state and must not recreate an expired folder.
An access failure alone is not deletion; the store's retention policy applies.

## Everything icon loading

The suggestion uses the configured executable's primary icon, not a bundled
logo. A Windows thread-pool worker resolves the path, reads metadata and maps
resources with `LoadLibraryExW` data/image-resource flags. It does not execute
the module's imports. `LoadIconWithScaleDown` selects the DPI-appropriate image;
release the resource module immediately and retain only the owned `HICON`.

Each later opening checks path, creation/modification timestamps and size in
the background. Settings/DPI changes invalidate the per-field memory cache.
An update preserving all those values is not detected. Painting takes a
nonblocking cache lock, and a missing icon leaves the action usable. Closing the
field cancels publication without joining the worker; shared state and handles
remain valid until it finishes.

## Tests and references

Group `25-recherche-integration` separates storage/argument checks, native view
behavior and the real Everything application. Follow its
[prerequisites and isolation rules](tests/regression/README.md#external-applications)
before running the external case. Compare displayed query/result identities,
including descendants and a match outside folder scope; process creation or a
matching result count is insufficient.

- [Everything command-line options](https://www.voidtools.com/support/everything/command_line_options/)
- [Everything search syntax](https://www.voidtools.com/support/everything/searching/)
- [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)
