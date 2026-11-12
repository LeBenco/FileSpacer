<a id="changelog"></a>

# Changelog

<a id="unreleased"></a>

## Unreleased

- Show only the "Open in..." command for the other file manager in folder context menus.
- Add a smooth dotted-outline animation when opening folders, with global enable/disable and duration controls in General preferences.

<a id="141--2026-11-09"></a>

## 1.4.1 — 2026-11-09

- Explain folder state database errors with a copyable diagnostic, access to the data folder and process identification when available. Offer a confirmed reset with a complete backup when the database cannot be loaded or recovered.
- Avoid unnecessary search history writes when applying unchanged preferences, so an unavailable folder state database does not block this step before an automatic update.
- Fix toolbar resizing remaining frozen after opening its menu or Preferences, including in Details view with deferred column resizing.

<a id="140--2026-11-09"></a>

## 1.4.0 — 2026-11-09

- Preserve saved free icon positions when the Shell reports an empty completion before loading the folder contents.
- Keep the View menu and free icon placement available while saved folder state is temporarily unreadable.
- Load the Quick access button icon directly from the Windows Shell.
- Add automatic column resizing in Details view, with optional live updates.
- Add File Explorer shortcuts and spatial adaptations (Ctrl+N and Ctrl+Backspace).
- Massively speed up reducing large selections by clicking an item or the background in classic Shell views, with automatic silent fallback when the private Windows capability is unavailable.
- Speed up reading complete and large partial selections in freely positioned icon views through public Windows APIs.
- Toggle the global Windows thumbnail setting from the View menu.
- Restore the scroll position when reopening a folder window.
- Place new windows using the normal size and position of existing FileSpacer windows, including maximized, minimized and snapped windows.
- Massively accelerate Select All and clearing selections of thousands of files in classic Shell views. This optimization uses private Windows functions whose availability and behavior are not guaranteed. If these functions are missing or incompatible, FileSpacer automatically and silently falls back to public functions, without the performance gains.

<a id="132--2026-11-04"></a>

## 1.3.2 — 2026-11-04

- Add Polish, Dutch and Ukrainian to the application, installer, website and user and developer documentation.

<a id="131--2026-11-03"></a>

## 1.3.1 — 2026-11-03

- Fix default file-manager opening selecting another application instead of FileSpacer.

<a id="130--2026-11-02"></a>

## 1.3.0 — 2026-11-02

- Record detailed installer operations, command failures and full documentation-cleanup exceptions in persistent diagnostic logs.
- Fix concurrent folder-state access, preserve edits during delayed recovery and reduce redraws when restoring large free-icon layouts.
- Fix stray window movement and unintended context menus after interrupted window gestures.

- Add optional PowerToys Peek previews and defer conflicting window gestures to Grab And Move.
- Customize the title bar menu in seven languages, with drag and drop, defaults and native or Nilesoft Shell appearance.
- Add saved-layout actions, fit-to-content, closing descendants or other windows, named folder groups and stable Nilesoft commands.
- Fix help cleanup failures blocking uninstallation and VM tests misidentifying several Explorer processes as separate desktop sessions.
- Fix seams between green icon faces and refresh multilingual guides and menu demonstrations.

<a id="123--2026-10-30"></a>

## 1.2.3 — 2026-10-30

- Smooth the installer and uninstaller sidebar logo with high-quality image scaling at the current display size.
- Preserve quoted elevation arguments so an all-users update starts its silent worker instead of reopening the installer.
- Update the originating installation by default and restore windows with the exact executable reported by setup, including when personal and all-users installations coexist.
- Fix mixed sorting identities, overlays and folder notifications. Keep free icon positions and use native views when the provider is unavailable or incompatible.
- Fix Preferences uniqueness across processes, shortcut validation, Everything paths, keyboard mnemonics and regional translations.
- Fix folder closing, activation clicks and View menu preview restoration.
- Fix Jump List refreshes and installer diagnostics. Preserve user help files during upgrades and uninstallation.
- Bundle multilingual offline help through F1 and About, and refresh localized setup guides and Nilesoft Shell references.

<a id="122--2026-10-28"></a>

## 1.2.2 — 2026-10-28

- Fix mixed sorting using the custom folder provider in free icon layouts. Use the native provider for free placement and switch providers when committing a sorted layout, preserving saved icon positions, selection and focus.

<a id="121--2026-10-28"></a>

## 1.2.1 — 2026-10-28

- Reduce transient generic icons when opening folders with mixed sorting. Prepare the visible icons in the Windows cache before revealing the native view, with a bounded wait for slow icon handlers.

<a id="120--2026-10-28"></a>

## 1.2.0 — 2026-10-28

- Add an optional mixed sort for files and folders in filesystem directories, under Display → Files, while retaining the native Windows view and saved folder state. The option is off by default.
- Add German, Italian, Spanish, Portuguese (Portugal) and Greek interface translations alongside English and French.
- Render the installer's title and subtitle as larger, DPI-aware Windows text with ClearType smoothing.
- Expand the online user guide with English and French screenshots, window gesture demonstrations and Nilesoft Shell menu comparisons.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.10...v1.2.0)

<a id="1110--2026-10-27"></a>

## 1.1.10 — 2026-10-27

- Request Windows confirmation only when installing or uninstalling for all users, and apply personal integration choices to the user who started setup.
- Show the complete version history in a scrollable What's new panel beside the installation choices on the first installer screen.
- Fix upgrades resetting installer component choices. Restore the saved selections and preserve the current default file manager during silent upgrades.
- Use the same FileSpacer sidebar, large logo and light-gray and white palette on every installer and uninstaller screen. Give the installer a distinct icon composed from the application's native bitmaps and a separately optimized download-arrow badge at every size, retaining a thin yellow rim at the folder's right and bottom edges.
- Fix shutdown crashes caused by eager initialization of thread-local folder data on late Shell worker threads.
- Offer Open in Windows File Explorer in folder context menus when FileSpacer is the default file manager, and Open in FileSpacer otherwise. Update the menus when changing the default, after installation and at startup.
- Fix network server and share shortcuts opening in Windows File Explorer when FileSpacer is the default file manager. Update the installed Folder Context Menu component and select FileSpacer as default again to enable network integration.
- Enlarge the About tab's application icon to 48 pixels at 100% scaling, with text positioned beside it.
- Keep maximized window titles fully visible and stretch the toolbar across the available width.
- Allow regression tests to run in a configured Hyper-V client while compiling the application and test helpers on the host.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.9...v1.1.10)

<a id="119--2026-10-27"></a>

## 1.1.9 — 2026-10-27

- Preview display modes while hovering over the View menu, with progressive restoration when cancelling and a default-on option under Display settings.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.8...v1.1.9)

<a id="118--2026-10-27"></a>

## 1.1.8 — 2026-10-27

- Replace the application icon and README logo with a projected tesseract design and an 11/20 purple-to-green cube edge ratio, retaining optimized small icon sizes.
- Clarify English and French settings labels and group window-locking options together.
- Add an option to hide the padlock badge on locked folder windows without changing their saved state locks.
- Use native Windows theme backgrounds throughout settings pages and shortcut controls.
- Explain window placement and taskbar grouping with standard Windows hover tooltips beside their options.
- Tie settings to the folder window that opened them and discard pending edits when it closes. Confirmed folder-state maintenance closes settings without applying pending changes.
- Reorganize the README around spatial navigation and features, with research references, a latest-release link, dedicated build and Nilesoft Shell guides, and expanded user documentation.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.7...v1.1.8)

<a id="117--2026-10-26"></a>

## 1.1.7 — 2026-10-26

- Minimize new-window overlap with exact geometry, first looking for free space. Rename the General option to "Minimize window overlap".
- Offer an optional default file manager choice in the English and French installer, with instructions to enable or reverse it later in FileSpacer's Integration settings.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.6...v1.1.7)

<a id="116--2026-10-26"></a>

## 1.1.6 — 2026-10-26

- Fix folder windows remaining open when an ancestor folder is deleted or moved to the Recycle Bin.
- Fix Jump Lists offering to reopen folders that are no longer available.
- Reduce overlap when placing folder windows without a saved position, ignoring the source being replaced during spatial navigation. Enable this by default with an option under General > Folder navigation.
- Move folder-state maintenance from General to About.
- Open new folder windows at a balanced 4:3 default size (580 × 435 at 96 DPI), retaining the existing height and honoring configured dimensions and each folder's saved size.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.5...v1.1.6)

<a id="115--2026-10-26"></a>

## 1.1.5 — 2026-10-26

- Fix Alt + double-click followed by mouse movement creating an unintended shortcut. Preserve native Properties, selection and focus without replaying a held button press.
- Correct full regression coverage for native double-click timing and physical mouse targets, and compare Properties, selection, focus, selection anchor, geometry and folder contents with window gestures disabled and enabled, including Ctrl/Shift combinations.
- Isolate the real Everything test executable and its bootstrap INI so the installed user's administrator setting cannot redirect test searches or trigger elevation. Check that the installed settings stay unchanged.
- Keep the desktop awake and temporarily suspend the screen saver during UI regression runs, restoring its original runtime state during cleanup, including after test failures.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.4...v1.1.5)

<a id="114--2026-10-25"></a>

## 1.1.4 — 2026-10-25

This release builds on 1.1.3, which already provides configurable keyboard/mouse gestures for moving and resizing windows.

- Add optional external Everything search with automatic detection, a manual executable path, current-folder/subfolder or everywhere scope, and explicit launch errors.
- Show Everything and recent searches automatically when entering the existing search field, without an extra button. Keep native search, instant filtering and local recent-query reuse.
- Remember five recent searches per folder across sessions by default, configurable from zero to twenty independently of Everything. Preserve existing folder/search data and clean history with the existing conservative folder-state policy.
- Display Everything's executable icon at the appropriate size, with background extraction and a memory cache refreshed when its executable changes.
- Add isolated storage, migration and real Shell/Everything regression coverage. Make the Shell test reader robust while successive filters change the view.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.3...v1.1.4)

<a id="113--2026-10-25"></a>

## 1.1.3 — 2026-10-25

- Add a Shortcuts settings tab to configure or disable window move/resize gestures using modifier keys and left, middle or right mouse buttons, with direct input capture and conflict validation.
- Leave Alt + double-click Properties handling to the native Explorer view.
- Restore full labels on opaque backgrounds for a single selected item in Icons view. Multiple selection retains native Windows rendering, and the preference now describes this scope.
- Add exhaustive shortcut configuration tests and real mouse gesture coverage, with French and English settings resources.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.2...v1.1.3)

<a id="112--2026-10-25"></a>

## 1.1.2 — 2026-10-25

- Add optional locking of a folder window's saved geometry and view settings from its title menu. Temporary changes remain possible, and individual file icon positions keep saving normally.
- Show a Small & Flat padlock badge on locked folder icons, with native multi-resolution ICO rendering, DPI scaling and high-contrast colors.
- Confirm global disabling of window state locking, clear locks for open and closed folders, and resume normal saving across processes.
- Extend regression coverage for lock transitions, persistence failures, Shell restoration and native badge rendering.
- Reopening a closed window (Ctrl+Shift+T, Jump List) scrolls Details and List views back to the row that was at the top when the window was closed.
- Crash diagnostics keep writing a minidump and a report to the application's local data. The attribution to the former full-name rendering, its automatic preference change and restart are removed.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.1...v1.1.2)

<a id="111--2026-10-24"></a>

## 1.1.1 — 2026-10-24

- Restore the selection-preserving activation click: the first click on the empty background of an inactive folder window activates it without clearing the selection (preference re-enabled).
- Fix a crash when a folder window is closed while the Shell view is refreshing (Refresh command, name filter, Refresh/New verbs).

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.1.0...v1.1.1)

<a id="110--2026-10-24"></a>

## 1.1.0 — 2026-10-24

- Refactor folder views to use Windows Shell implementations through public COM APIs instead of adapting internal Shell controls.
- Keep spatial window state, selection/focus, icon positions and filtering connected to the production SQLite store and public Shell events.
- Preserve native file commands and FileSpacer Alt move/resize and Properties gestures.
- Open Control Panel in its native Windows window while preserving the source folder and selection.
- Remove automatic origin-folder selection when navigating to a parent or ancestor for ergonomic reasons: subsequent Ctrl selections must not accidentally open both folders.
- Accept three behavior differences in this release: custom full selected names with opaque backgrounds, selection preservation on activation click, and exact reopened scroll offsets are unavailable. Disable the two inactive Settings controls while retaining their saved preferences.
- Streamline regression coverage around the production native Shell configuration. Retire exploratory profile comparisons, embedding probes and migration-only reporting.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.8...v1.1.0)

<a id="108--2026-10-22"></a>

## 1.0.8 — 2026-10-22

- Move folder windows with Alt + left drag and resize them with Alt + right drag from the main file area, preserving the file selection.
- Keep the classic Shell view enabled independently of the selected-item full-name display preference.
- Apply Nilesoft Shell theming to the view-mode menu.
- Avoid long shutdown delays caused by passive Shell icon caches while retaining the lifetime references acquired by explicitly invoked Shell commands.
- Isolate Shell view hosting and extend regression coverage for Alt gestures, menu reentrancy and Shell/STA lifetime.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.7...v1.0.8)

<a id="107--2026-10-20"></a>

## 1.0.7 — 2026-10-20

- Select the originating child folder when opening a parent or ancestor, preserving selection in already-open windows.
- Fix folder name filtering when submitting, changing or clearing a query.
- Restore the native Windows Quick access icon and center the Quick access and configuration buttons.
- Remove temporary diagnostics and use Benco in project credits.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.6...v1.0.7)

<a id="106--2026-10-11"></a>

## 1.0.6 — 2026-10-11

- Automatically restore open folder windows after an in-app update, preserving their saved positions, sizes and view settings.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.5...v1.0.6)

<a id="105--2026-10-11"></a>

## 1.0.5 — 2026-10-11

- Check for updates asynchronously from the About page, with an adaptive installation button and status text.
- Download updates in a separate cancellable progress dialog while folder windows remain usable.
- Verify installer size and SHA-256 before gracefully closing FileSpacer and starting setup.
- Publish installer download metadata alongside each release.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.4...v1.0.5)

<a id="104--2026-10-11"></a>

## 1.0.4 — 2026-10-11

- Add a title-bar command to close open parent-folder windows.
- Reopen closed folders from the title-bar menu, Ctrl+Shift+T or taskbar Jump Lists, restoring their saved view, selection and scroll position.
- Add Explorer Quick access folders with native Shell icons to taskbar Jump Lists.
- Show the reopening command only when the last closed folder is not already open, using its name and native icon.
- Exclude the current window from the title-bar window-switching list.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.3...v1.0.4)

<a id="103--2026-10-11"></a>

## 1.0.3 — 2026-10-11

- Restore manual update checks and optional weekly checks using FileSpacer GitHub releases, disabled by default.

- Publish the update manifest alongside each release installer.

- Preserve native Shell handling of Alt+double-click and Alt+Enter on folders.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.2...v1.0.3)

<a id="102--2026-10-11"></a>

## 1.0.2 — 2026-10-11

- Apply Nilesoft Shell themes to the title-bar folder menu independently of file selection, preserving native Shell commands.
- Allow separate Nilesoft customization of the title-bar and toolbar menus.
- Center the title-bar drop-down arrow.
- Document Nilesoft Shell configuration, theme application and menu filtering.
- Credit Benco in copyright notices.
- Allow the release workflow to replace the installer of an existing release.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.1...v1.0.2)

<a id="101--2026-10-09"></a>

## 1.0.1 — 2026-10-09

- Enable native minimize and maximize controls and fix caption buttons in maximized windows.
- Save and restore maximized state separately from normal window geometry, with automatic database migration.
- Use native taskbar animations and per-folder taskbar relaunch properties.
- Apply taskbar grouping preferences to newly created windows and clarify this behavior in Settings.
- Allow deletion of open folders and remove their saved state.
- Stop the application and its recovery helper from locking folders through their working directory.
- Preserve absolute folder paths and debug test mode during crash recovery.
- Restore normal filename labels immediately after deselection when full-name display is enabled.
- Release cached Shell interfaces when folder windows close and correct Shell notification handling.
- Support manual release publication and include the version in installer filenames.

[Full changes](https://github.com/LeBenco/FileSpacer/compare/v1.0.0...v1.0.1)

<a id="100--2026-10-09"></a>

## 1.0.0 — 2026-10-09

- Initial FileSpacer release for Windows 10 or later, x64, based on ChromaFiler.
- Provide spatial navigation with one window per folder and persistent window position, size, view settings and icon positions stored in SQLite.
- Follow folder moves and renames and activate an existing window when its folder is opened again.
- Close the source window by default when opening a folder or its parent, with a configurable default and Ctrl to reverse the behavior.
- Add a breadcrumb and editable path bar, plus a folder name filter with optional filtering while typing.
- Provide configurable toolbar elements, a bottom status bar and taskbar grouping preferences.
- Add optional experimental full-name display for selected items.
- Provide English and French interfaces, with English as the fallback.
- Add crash recovery and maintenance commands to reset preferences or saved folder state.
- Remove Miller-column navigation, the built-in text editor, preview windows and the floating tray.
- Provide MSVC/NMAKE build scripts, an NSIS installer, automated GitHub releases and project documentation.

[Release history](https://github.com/LeBenco/FileSpacer/commits/v1.0.0)

