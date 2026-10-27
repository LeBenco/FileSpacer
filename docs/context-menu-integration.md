# Context-menu file manager switching

The FileSpacer-provided command opens the other file manager. With FileSpacer as
the default, the menu offers **Open in Windows File Explorer**. Otherwise it offers
**Open in FileSpacer**. This applies independently to directories, drives, ZIP
folders, network servers and network shares. A directory's background menu follows
the directory association.

## Implementation

`Settings.cpp` reads the effective `HKEY_CLASSES_ROOT` association, including the
first verb of a space- or comma-separated default-verb list. Synchronization never
changes that association. It only updates FileSpacer-owned menu registrations
under the current user's `Software\Classes` subtree, including when the original
FileSpacer command was installed for all users.

When FileSpacer is the default, its installed `filespacer` verb receives the
documented `ProgrammaticAccessOnly` value. Its command and COM `DelegateExecute`
remain intact. A separate `filespacer.explorer` verb launches
`"%SystemRoot%\explorer.exe" "%1"` (`%V` on a directory background), with the native
Explorer icon and an English or French caption. Resetting the default removes
this verb and the visibility override. Windows-owned verbs are unchanged.

Integration settings synchronize after changing all five associations and retain
independent error reporting. Normal startup synchronizes again; Debug `/test`
startup skips this production integration. There is no registry watcher or resident
helper. The installer calls the same operation through `/sync-context-menus`,
which exits without opening a window or starting crash recovery. Removing the
context-menu component or uninstalling also removes the per-user overrides through
the installer's existing user-hive cleanup.

Unchanged menu values are not rewritten. Changes send `SHCNE_ASSOCCHANGED`;
startup reports synchronization failures, and the installer checks the helper's
exit code.

Microsoft references: [static and programmatic-only Shell verbs](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers)
and [association-change notification](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shchangenotify).

## Network roots

Windows classifies a server root such as `\\server` as `NetServer`, and provides
`NetShare` for network shares. Setting only `Directory`, `Drive` and
`CompressedFolder` leaves server roots using Explorer's inherited `Folder` command.
The installer now registers FileSpacer's existing command and COM delegate for
both network classes; default selection, reset, menu synchronization and uninstall
cleanup cover them too. Failures are reported separately for each class.

Update the installed Folder Context Menu component, then select FileSpacer as the
default again in Integration settings, or choose that option in the updated
installer. Startup synchronization does not change the user's default choices.
The Integration page detects missing network verbs and asks for an installation
update rather than reporting success with incomplete registrations. Portable
startup still installs nothing.

These are the documented [predefined Shell object classes](https://learn.microsoft.com/en-us/windows/win32/shell/reg-shell-exts).

### Network regression validation — 27 October 2026

- The extended case failed against the previous implementation, then passed with
  **484 checks per language** in English and French in `AgentWin10`.
- A real saved `.lnk` targets `\\localhost`. The native Shell resolves it, and
  `BHID_AssociationArray` / `IQueryAssociations` confirm that its server target
  follows the selected manager in both directions under the isolated registry.
  Menus cover both network classes; independent server/share errors and missing
  registrations from an older installation are checked too.
- Debug MSVC/NMAKE `/W4`: successful, no warnings. clang-tidy and Cppcheck completed
  on `Settings.cpp`, `SettingsDialog.cpp` and `ContextMenuTests.cpp`, including
  `Settings.h` and `resource.h`, with the Windows x64 configurations described
  below and an unchanged-source comparison. **REVIEW**: new style suggestions
  only (arrays, constants, braces, nested conditionals and direct includes); no
  confirmed new functional defect, timeout or ATL fallback.
- The installer was not built or run. This validates native shortcut resolution
  and association lookup with isolated registrations; it does not claim a
  physical Desktop double-click or an installed upgrade/uninstall test.

## Validation — 27 October 2026

- MSVC/NMAKE Debug, `/W4`: successful, no warnings.
- `tests.cmd -VM AgentWin10 -Category Internal -Pattern "29-*"`: 324 checks per
  language. The native test
  checks both directions, repeated synchronization, separate class defaults,
  external changes, native verb lists, failure propagation, preserved commands and
  COM delegates, and the English/French captions. It redirects its own registry
  handles to a unique tree and deletes that complete tree afterward.
- Menus are built by the real `SHCreateDefaultContextMenu`, without displaying
  UI. Both `ShellExecuteEx` paths, with and without `SEE_MASK_INVOKEIDLIST`, execute
  an actual test subprocess through the hidden default command. These assertions
  do not constitute a physical double-click test in Explorer.
- The existing VM case `19-commandes-shell\04-MenusContextuels.ps1` passes:
  selected-item and background menus open and cancel, preserving view contents
  and selection. Guest reports confirm restored test state and unchanged
  production preferences/database.
- clang-tidy and Cppcheck completed on `Settings.cpp`, `main.cpp` and
  `ContextMenuTests.cpp`, covering the modified `Settings.h` and `resource.h`.
  Windows x64, C++14, Debug macros, real SDK 10.0.19041.0 and MSVC/ATL 14.44.35207
  headers were used for clang-tidy; Cppcheck used its official Windows, ATL and
  SQLite models without system-header preprocessing. Original application
  sources were analyzed with the same configuration for comparison.
- Static verdict: **REVIEW**. No new confirmed functional defect remains.
  Suggestions remain about trailing return types, braces, constants and includes,
  and Cppcheck flags `lstrlenW` and test logging parameter names. These are style
  observations; no unrelated modernization was performed. An unhandled test-entry
  exception found by Cppcheck was corrected and the analysis rerun. All modified
  C++ files received both analyses; no ATL fallback or analysis timeout occurred.
- No installer was built or executed. NSIS preprocessing stopped at the existing
  `LockedList::AddModule` plugin lookup, before the changed installer sections;
  installation, upgrade and uninstallation remain untested end to end.

The first test-fixture attempt used a volatile registry parent in the dedicated
VM. That prevented a later UI runner from creating stable test keys. The empty
fixture-only parent was verified, recreated as stable and its empty saved test
key restored. The native test now uses its own complete, nonvolatile tree; the
subsequent UI run passed with state restoration confirmed.
