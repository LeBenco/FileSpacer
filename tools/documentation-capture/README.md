<a id="documentation-capture"></a>

# Documentation capture

These scripts record the actual FileSpacer UI in the dedicated `AgentWin10` VM,
using the existing regression-package transport and ScreenToGif 2.43.2. They do
not assemble frames, edit out pauses or composite application windows.

Wait until the VM and the application build are available. Use the checkout
containing the validated Debug build as `ProjectRoot`, even if these capture
scripts live in another checkout. Preparation reads its actual application,
native helper DLL and regression helpers. It copies the validated application's
executable and DLL unchanged for each requested language and verifies their hashes.
The Windows display language selects the application's existing translations.
It writes only generated files under that checkout's `build/` directory.

<a id="run"></a>

## Run

Use CMD with the project's MSVC/Windows SDK environment available for preparing
the regression helpers. Build the desired Debug application first. The existing protected
Hyper-V runner must be configured and the VM must have an unlocked graphical
session. Do not use VMConnect or run another VM test at the same time.
Animations require at least 4 GiB free on the guest drive after package transfer;
ScreenToGif stores full-resolution intermediate frames during recording.

From the checkout containing these scripts, first capture one language:

```cmd
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\documentation-capture\Invoke-Capture.ps1 -ProjectRoot C:\Users\Benco\Documents\FileSpacer -Languages fr
```

Run one language at a time, in a Windows session using that display language.
`-Languages` is required and accepts exactly one language per invocation.
Application resources alone do not translate native Shell menus or column
headers. Change the VM's display language and sign in again between languages.
The scenario checks `Get-WinUILanguageOverride`, falling back to the current UI
culture when no override is configured. The setting and PowerShell's own UI culture
are recorded separately, since PowerShell can fall back to another language.
Reference labels explicitly use the capture's verified Windows display language,
not PowerShell's culture or the user's fallback list. They are read directly from
that language's compiled string table in the unchanged application executable.
The actual title-menu language is checked before starting any recorder, and
`resource-labels.json` retains the exact Unicode values for diagnosis.
The check rejects a display-language setting that differs from the requested
capture language. An override can be pending until sign-in, so a fresh session
and visual review of native Shell labels are still required.
`-Scenes locked-title` captures
only the lock close-up. The default scenes include the menu demonstration, moving,
resizing and the close-up. Files are collected by the existing regression runner
under `build/regression/vm-<id>/output/regression/99-documentation/01-Capture/`.
Each `captures/<language>/` directory uses the same neutral filenames as
`docs/<language>/images/`. Review the exported files before copying them there.

Use `-Scenes details-columns` with a build that includes full-width Details
columns to record only that feature. The fixture in `details-columns.json`
adds localized medium and long filenames to the same fictional folder.

ScreenToGif's existing portable archive must be present at
`build/tools/ScreenToGif.zip` in the source checkout. Its SHA-256 must be
`42a0b53d67ecae22698b4881ffb2c9c7a9049663f5fbdd2edb779e1971000664`.
Nothing is downloaded or
installed automatically. The VM runs precompiled application and helper files.

<a id="storyboard"></a>

## Storyboard

The menu animation starts with the folder window and the cursor at its center.
The pointer moves to the title menu, opens it, and leaves the original menu
visible for two seconds. It then clicks an empty part of the folder to dismiss
the menu and follows the visible path **… > FileSpacer Settings > Display >
Customize title bar menu**. It drags **Restore the locked layout** into the preview,
drags **Reopen the last closed window** out, accepts both dialogs with their
visible buttons, and shows the modified menu for two seconds.

Every pointer transition during recording is interpolated. The dialogs open in
their natural positions. There are no cuts, silent settings commands or instant
window repositioning within the recorded sequence. Preparation and export happen
outside the recording. Moving and resizing also start with the pointer at the
center of the folder window and preserve its actual selection.

The Details-columns scene starts with filling disabled for the folder and a
normal-width Name column that reveals about 30% of the long names. It enables
filling through the visible View menu, holding the pointer over the option for
one second before clicking. It then pauses and drags the Name divider to
show the complete names. At least two names remain more than 20% hidden between
activation and the manual drag. The global permission and update-on-release
option are enabled before recording. The window and measured View menu define
the capture area. Native column bounds and text measurements are recorded for
review, along with the actual command and executable hash. Review the exported
GIF to confirm the visible text, pauses and continuous pointer path.

The recorder finishes startup before F7 begins recording with the folder active.
This keeps the recorder launch and the initial focus change outside the captured sequence.

Before the menu recording, its actual localized menus and dialogs are opened,
measured and cancelled. Their frame union plus 32 pixels defines the capture
region. The recorded sequence then repeats the complete visible path. Native
tab and list-view bounds locate controls when accessibility omits their items.
Before clicking a button, the script reads its native bounds again after moving
the pointer. If localized preview rendering has moved the button, the pointer
visibly follows its new center before a single click is sent.
It also waits for dialog owners to be enabled again after their child dialogs close.

A fictional folder is opened and closed during preparation so **Reopen** is
really available in the original menu. No production history is used.

ScreenToGif records the cursor and click highlights. Its key-stroke task includes
injected modifier keys so Alt is visible during the mouse shortcuts. Inspect the
exported GIF to confirm the overlay, the continuous cursor path, the complete
dialogs and both menu pauses. Recording settings alone do not prove the resulting
overlay is visible. The 90-second recorder limit is a watchdog, not an editing
operation.

After a GIF decodes successfully and ScreenToGif closes normally, its temporary
frame cache is removed before the next recording. Failed recordings also remove
their frame cache after the recorder closes, keeping diagnostic screenshots and
logs. An application that cannot close leaves an explicit cleanup error.
After a successful complete scenario, the guest copy of ScreenToGif and its
archive are removed too. The original host archive remains available for reuse.
At the end of each documentation capture run, the guest launcher removes that
run's disposable input package after the worker exits and its state is restored,
including when the scenario fails. It verifies the run identity and path first.
Other test packages are unaffected. Original packages remain on the host, and
exported captures, reports and state backups remain in the separate output folder.

The lock close-up captures the upper-left 480 × 220 pixels of an actual locked
folder window, including its new left-hand padlock, the title and part of the
window below. It does not reuse the old centered title crop.
The icon view is selected through the application's actual View menu. Before
capturing, the scenario verifies its mode, icon size and absence of visible
Details column headers. No Shell flags or image pixels are changed to hide them.

<a id="wallpaper-and-isolation"></a>

## Wallpaper and isolation

`wallpaper.jpg` is the same approved background used by the existing screenshots:
[Unsplash source](https://images.unsplash.com/photo-1599148521725-dbd1ee6c0817),
under the [Unsplash license](https://unsplash.com/license). Its SHA-256 is
`8950e941ecb7805266b09cd472325fd5d849820ae34bec76f2161bfe5f76a714`.
The file is stored locally, so capture needs no remote image service. The image
license is separate from the scripts' GPL-3.0-only license.

The scenario checks the active wallpaper's file contents and Fit mode before
each scene, minimizes other application windows, and checks that the exposed
area belongs to the desktop. It restores those windows, the previous wallpaper
and the display mode afterward.
Fixtures use fictional localized filenames. The regression harness isolates
FileSpacer preferences and test state, restores them on exit, and preserves
failure artifacts. Nilesoft Shell is disabled for these recordings. Files are
published to the documentation only after visual review.
