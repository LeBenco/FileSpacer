# Search integration

The local name filter remains in `FolderWindow::applyNameFilter`. SearchBox
distinguishes explicit submission from its existing 150 ms typing debounce.
The field has only its original magnifier. Focus/click opens a nonactivating
Win32 popup containing a native owner-drawn list box. It reuses the existing
menu-entry builder (Everything, optional separator, recent queries), with no
second history store. The edit retains focus and handles ordinary editing while
the list is visible. No item is selected until keyboard navigation or a mouse
press; stationary-pointer/hover events do not select Everything. Changing the
query clears a previous selection. Enter without a selected item remains local.
Escape dismisses without erasing; Tab, focus loss, owner movement/deactivation
and destruction dismiss the list. The popup is bounded to the monitor work area
and scrolls for longer histories. Colors and fonts use the existing Windows
system settings; the list's real strings remain available to accessibility APIs.

Only a successful explicit local submission, recent-query selection, or external
launch records a nonblank query. This integration shipped in 1.1.4; see the
[user guide](user-guide.md#search-and-history) for its current usage.

The default Everything scope is the current filesystem folder with descendants;
the other choice is Everywhere. FileSpacer passes one `-search` parameter to the
external GUI. Folder scope prepends a quoted folder path ending in `\` and groups
the query with `<…>` to retain the precedence of OR searches. The query remains
Everything syntax. Quotes in the command-line parameter use Everything's triple
quote convention; this differs from the Windows CRT argument quoting used for the
executable name. `CreateProcessW` receives a separate absolute executable path and
a writable Unicode command buffer, with no command interpreter.

Contracts: [Everything command-line options](https://www.voidtools.com/support/everything/command_line_options/),
[search syntax](https://www.voidtools.com/support/everything/searching/), and
[CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).
Everything 1.4.1.1032 is covered by the real application test. No 1.5-only command
switch is required. Launch success means Windows created the external process;
it does not guarantee Everything's indexing or later execution succeeds.

Global options use the existing registry namespace, including its `/test`
isolation: `EverythingEnabled` (default 0), `EverythingPath` (empty = detection),
`EverythingScope` (0 = folder, 1 = everywhere), and `SearchHistoryLimit` (default
5, range 0–20). Detection checks App Paths and the voidtools installation location
in both registry views, then Program Files, Program Files (x86) and the current
user's `LocalAppData\Programs\Everything`.

Schema 6 adds `search_history(sequence,key,query)` to the existing SQLite database.
The migration preserves other tables and columns. An increasing sequence supplies
stable ordering even when several submissions occur in the same second. History is
separate from view state, Shell filters, and the closed-window history. The open
window keeps a bounded recent list; the existing state-save event coalesces writes,
and normal closure flushes pending history. Preference changes trim every stored
folder transactionally, including closed folders. Empty entries are omitted.
Settings notifications carry the applied limit and the normal/test namespace,
so delayed delivery cannot miss a temporary clear or trim and later save an old
cached history. Test notifications cannot clear a normal window's cache.

History moves with path-based identities. A deletion trigger on `folder_state`
also covers confirmed removal, qualified expiration, reset and quota eviction.
History writes require an existing folder state and cannot recreate an expired
folder. The existing rule requiring repeated failures at least one UTC calendar
year apart remains unchanged; a success resets that failure period.

The registered regression group 25 separates storage/menu/argument checks from
the native Shell test and the real Everything test. All UI fixtures, FileSpacer
preferences, databases and the Everything index are isolated by the existing
runner. The real Everything test compares visible queries and result identities,
including a descendant and a match outside the scoped folder.

The Everything suggestion displays the installed executable's primary icon.
On the first list opening, a Windows thread-pool worker resolves the configured
path and maps only its resources (`LoadLibraryExW`, data/image-resource flags).
`LoadIconWithScaleDown` selects the requested 16-DIP image at the existing UI DPI,
preferring an exact resource size. The resource module is immediately released;
only the resulting owned HICON remains in a per-search-field memory cache.

Each later list opening queues a background check of the resolved path, creation/
modification timestamps and file size. An unchanged icon is reused. A normal
executable update refreshes the icon on the next list opening, without restarting
FileSpacer; changing settings or the requested pixel size invalidates the cache. No version parsing,
file hashing, persistent icon file or continuous watcher is required. A change
that preserves the path, both timestamps and size is not detected by this check.

Neither path detection, metadata access nor extraction runs on the UI thread.
Painting uses a nonblocking cache lock; icon arrival repaints the existing row
without moving its text. Missing/unreadable icon resources leave the text/action
usable. Closing the field cancels publication without joining the worker, whose
shared state and owned handles remain valid until it finishes.
