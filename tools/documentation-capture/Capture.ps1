# Test-Category: UI
# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
if(!$env:FS_REGRESSION_RUN){throw 'Run this scenario through Invoke-Capture.ps1 in the dedicated test VM.'}
. (Join-Path $ProjectRoot 'tests\regression\cases\25-recherche-integration\SearchIntegration.ps1')
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase,System.Drawing,System.Windows.Forms
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\33-menu-titre\TitleMenuUi.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\15-deplacement-alt\MouseInput.cs') -ReferencedAssemblies $references
Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\28-apercu-affichage\ViewMenuUi.cs') -ReferencedAssemblies $references
Add-Type -Path ([IO.Path]::Combine($ProjectRoot,'build','documentation-capture','reusable-source','CaptureDesktop.cs')) -ReferencedAssemblies System.Drawing,System.Windows.Forms
$source=Join-Path $ProjectRoot 'build\documentation-capture\reusable-source'
. (Join-Path $source 'Capture.Core.ps1')
. (Join-Path $source 'SceneActions.ps1')
$plan=Get-Content -LiteralPath (Join-Path $ProjectRoot 'build\documentation-capture\reusable-plan.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($plan.Scenes-contains'details-columns'){
    Add-Type -Path (Join-Path $ProjectRoot 'tests\regression\cases\37-colonnes\ColumnsUi.cs') -ReferencedAssemblies @('Accessibility.dll',([FileSpacerRegression.Native].Assembly.Location))
}
$names=Get-Content -LiteralPath (Join-Path $source 'scenes.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$displayOverride=Get-WinUILanguageOverride
$windowsUiLanguage=if($displayOverride){$displayOverride.Name}else{[Globalization.CultureInfo]::CurrentUICulture.Name}
$script:CaptureResourceLanguage=$windowsUiLanguage
$sessionLanguage=[Globalization.CultureInfo]::GetCultureInfo($windowsUiLanguage).TwoLetterISOLanguageName
Write-Host ('Windows display language: '+$windowsUiLanguage+'; PowerShell UI culture: '+[Globalization.CultureInfo]::CurrentUICulture.Name)
foreach($requestedLanguage in $plan.Languages) {
    if($requestedLanguage-ne$sessionLanguage){throw ('The signed-in Windows session uses '+$sessionLanguage+', but capture requests '+$requestedLanguage+'. Change the VM display language and sign in again before recording native Shell UI.')}
}
$animations=@($plan.Scenes|Where-Object {$_-ne'locked-title'}).Count-gt0
if($animations) {
    $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot($ProjectRoot))
    Assert-Ui ($drive.AvailableFreeSpace-ge4GB) 'Animation capture requires at least 4 GiB free on the guest drive for ScreenToGif frames and export.'
}
$originalMode=[CaptureDesktop]::CurrentMode()
$originalWallpaper=[CaptureDesktop]::GetWallpaperState()
$records=New-Object Collections.ArrayList
$script:CaptureCleanupErrors=New-Object Collections.ArrayList
$backgroundWindows=New-Object Collections.ArrayList
$captureFailure=$null
$captureComplete=$false
$env:FILESPACER_TEST_NILESOFT='0'
try {
    foreach($element in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.Condition]::TrueCondition)) {
        $pattern=$null
        if(!$element.TryGetCurrentPattern([Windows.Automation.WindowPattern]::Pattern,[ref]$pattern)){continue}
        if(!$pattern.Current.CanMinimize){continue}
        $state=$pattern.Current.WindowVisualState
        if($state-ne[Windows.Automation.WindowVisualState]::Minimized){
            [void]$backgroundWindows.Add(@{Pattern=$pattern;State=$state})
            $pattern.SetWindowVisualState([Windows.Automation.WindowVisualState]::Minimized)
        }
    }
    $preferred=@([CaptureDesktop]::Modes() | Where-Object {$_.Width-eq1600 -and $_.Height-eq1200 -and $_.BitsPerPixel-eq32})
    Assert-Ui ($preferred.Count-gt0) 'The capture storyboard requires a real 1600 by 1200 desktop.'
    [CaptureDesktop]::SetMode($preferred[0])
    $wallpaper=Join-Path $source 'wallpaper.jpg'
    Assert-Ui ((Get-FileHash -LiteralPath $wallpaper -Algorithm SHA256).Hash-eq$plan.WallpaperSha256) 'The approved wallpaper differs.'
    [CaptureDesktop]::Wallpaper($wallpaper)
    [CaptureDesktop]::VerifyWallpaper($wallpaper)
    $recorder=Join-Path $ProjectRoot 'build\documentation-capture\reusable-recorder'
    if($animations -and !(Test-Path -LiteralPath (Join-Path $recorder 'ScreenToGif.exe'))){
        Expand-Archive -LiteralPath (Join-Path $ProjectRoot 'build\documentation-capture\ScreenToGif.zip') -DestinationPath $recorder
    }
    foreach($script:language in $plan.Languages) {
        $failure=$null
        try {
            $script:UiProcesses=New-Object Collections.ArrayList
            $script:UiWindows=New-Object Collections.ArrayList
            $script:UiSettings=[IntPtr]::Zero
            $script:editor=[IntPtr]::Zero
            $script:CaptureLocked=$false
            Initialize-Ui ('documentation-'+$script:language) -FixtureParent (Join-Path $ProjectRoot 'build')
            $script:UiExecutable=Join-Path $ProjectRoot ('build\documentation-capture\reusable-binaries\'+$script:language+'\FileSpacer.exe')
            $script:captures=Join-Path $env:FS_REGRESSION_RUN ('captures\'+$script:language)
            [void][IO.Directory]::CreateDirectory($script:captures)
            foreach($setting in @('KeepSourceWindowOpen','WindowStateLockEnabled','WindowStateLockBadgeEnabled','FullNamesOnSelection')){Set-UiPreference $setting 1}
            foreach($setting in @('WindowStateUnlockBadgeEnabled','WindowStateLockClickEnabled','MixedFolderSorting','EverythingEnabled')){Set-UiPreference $setting 0}
            if($plan.Scenes-contains'details-columns'){
                foreach($setting in @('DetailsColumnsEnabled','DetailsColumnsDeferred')){Set-UiPreference $setting 1}
            }
            $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test',$true)
            try{$key.DeleteValue('TitleMenu',$false)}finally{$key.Dispose()}
            $n=$names.($script:language)
            Assert-Ui ($null-ne$n) 'Unknown capture language.'
            $folder=Join-Path $script:UiRoot $n[0]
            [void][IO.Directory]::CreateDirectory($folder)
            foreach($index in @(3,4,5)){[IO.File]::WriteAllText((Join-Path $folder $n[$index]),'Fictional documentation example.')}
            foreach($index in @(8,9)){[void][IO.Directory]::CreateDirectory((Join-Path $folder $n[$index]))}
            foreach($index in @(6,7)) {
                $image=New-Object Drawing.Bitmap(400,280)
                $graphics=[Drawing.Graphics]::FromImage($image)
                try{$graphics.Clear([Drawing.Color]::FromArgb(220,228,238));$image.Save((Join-Path $folder $n[$index]))}
                finally{$graphics.Dispose();$image.Dispose()}
            }
            # Reopen must have a real closed folder to appear in the menu.
            $recent=Join-Path $script:UiRoot $n[1]
            [void][IO.Directory]::CreateDirectory($recent)
            $recentWindow=Open-UiFolder $recent 0
            Close-UiFolder $recentWindow
            Assert-Ui (![FileSpacerRegression.Native]::IsWindow($recentWindow)) 'The preparation folder must really close before recording.'
            $window=Open-UiFolder $folder 7
            Place $window 340 220 720 440
            # Use the application's real View command, so renderer policies are applied.
            OpenRecordedMenu $window 'IDM_VIEW_MENU'
            $viewMenu=[FileSpacerViewMenuTests.MenuUi]::Active($window)
            Assert-Ui ($viewMenu.Entries.Count-ge6) 'The actual View menu must expose the standard display modes.'
            $mediumIcons=$viewMenu.Entries[2]
            ClickPoint $window (Point (($mediumIcons.Bounds.Left+$mediumIcons.Bounds.Right)/2) (($mediumIcons.Bounds.Top+$mediumIcons.Bounds.Bottom)/2))
            Wait-Ui {$null-eq[FileSpacerViewMenuTests.MenuUi]::Active($window)} 'View menu closed after selecting medium icons'
            Wait-UiViewMode $window 1 48
            [void](CaptureIconView $window)
            $selectionFile=Join-Path $folder $n[3]
            Wait-Ui {Use-UiView $window {param($view)$view.ParsingNames($false)-contains$selectionFile}} 'the actual fixture file appears in the Shell view'
            $actualNames=@(Use-UiView $window {param($view)$view.Names()})
            # Explorer can hide known file extensions; select the actual display name.
            $displayNames=@($actualNames|Where-Object {$_-in@($n[3],[IO.Path]::GetFileNameWithoutExtension($n[3]))})
            Assert-Ui ($displayNames.Count-eq1) 'The fixture must have one unambiguous Shell display name.'
            $selectionName=$displayNames[0]
            Use-UiView $window {param($view)$view.Select($selectionName)}
            # Check the worker's resource language before starting any recorder.
            OpenRecordedMenu $window 'IDM_PROXY_BUTTON'
            $initialMenu=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
            $lockEntry=@($initialMenu.Entries|Where-Object {$_.Command-eq(Get-UiId 'IDM_TOGGLE_STATE_LOCK')})
            $lockLabel=[CaptureDesktop]::Resource($script:UiExecutable,(Get-UiId 'IDS_LOCK_WINDOW_STATE'),$script:CaptureResourceLanguage).Split("`t")[0].Replace('&','')
            @{WindowsUiLanguage=$windowsUiLanguage;PowerShellUiCulture=[Globalization.CultureInfo]::CurrentUICulture.Name;ExpectedLockLabel=$lockLabel;ActualLockLabels=@($lockEntry|ForEach-Object {$_.Label})}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'resource-labels.json') -Encoding UTF8
            Assert-Ui ($lockEntry.Count-eq1 -and $lockEntry[0].Label.Replace('&','').Split("`t")[0]-ceq$lockLabel) ('Initial menu language differs from the Windows resource: expected '+$lockLabel+'; observed '+($lockEntry.Label-join', '))
            CloseMenu $window
            foreach($scene in $plan.Scenes) {
                [CaptureDesktop]::VerifyWallpaper($wallpaper)
                switch($scene) {
                    'move-window' {
                        Use-UiView $window {param($view)$view.Select($selectionName)}
                        Record $window 'move-window'
                        Assert-UiSelected $window $selectionName
                    }
                    'resize-window' {
                        Use-UiView $window {param($view)$view.Select($selectionName)}
                        Record $window 'resize-window'
                        Assert-UiSelected $window $selectionName
                    }
                    'customize-title-menu' {Customize $window}
                    'locked-title' {LockedTitle $window}
                    'details-columns' {DetailsColumns $window $folder}
                    default {throw ('Unknown documentation scene: '+$scene)}
                }
            }
        } catch {
            $failure=$_
            try {[CaptureDesktop]::Desktop((Join-Path $env:FS_REGRESSION_RUN 'capture-first-error.png'))} catch {Write-Warning ('Capture diagnostics: '+$_)}
            throw
        }
        finally {
            $editorFailure=$null
            try {
                if($script:editor-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:editor)){
                    [void][FileSpacerRegression.Native]::PostMessage($script:editor,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
                    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'capture editor closes normally'
                }
            } catch {$editorFailure=$_;Write-Warning ('Capture editor cleanup: '+$_)}
            Stop-Ui -Failure $(if($failure){$failure}else{$editorFailure})
            if(!$failure -and $editorFailure){throw $editorFailure}
        }
    }
    $captureComplete=$true
} catch {$captureFailure=$_;throw}
finally {
    $cleanupErrors=New-Object Collections.ArrayList
    foreach($cleanupError in $script:CaptureCleanupErrors){[void]$cleanupErrors.Add($cleanupError)}
    try {[CaptureDesktop]::RestoreWallpaper($originalWallpaper)} catch {[void]$cleanupErrors.Add($_.Exception.Message)}
    try {[CaptureDesktop]::SetMode($originalMode)} catch {[void]$cleanupErrors.Add($_.Exception.Message)}
    foreach($background in $backgroundWindows){try{$background.Pattern.SetWindowVisualState($background.State)}catch{[void]$cleanupErrors.Add($_.Exception.Message)}}
    if($captureComplete -and !$cleanupErrors.Count -and $animations){
        try {
            $temporaryRoot=[IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build\documentation-capture')).TrimEnd('\')+'\'
            foreach($temporary in @($recorder,(Join-Path $ProjectRoot 'build\documentation-capture\ScreenToGif.zip'))){
                if(!(Test-Path -LiteralPath $temporary)){continue}
                $resolved=(Resolve-Path -LiteralPath $temporary).Path
                Assert-Ui ($resolved.StartsWith($temporaryRoot,[StringComparison]::OrdinalIgnoreCase)) 'The disposable recorder must remain inside this capture package.'
                Remove-Item -LiteralPath $resolved -Recurse -Force
            }
        }catch{[void]$cleanupErrors.Add($_.Exception.Message)}
    }
    @{Date=[DateTime]::UtcNow.ToString('o');WindowsUiLanguage=$windowsUiLanguage;WindowsUiOverride=$(if($displayOverride){$displayOverride.Name}else{$null});PowerShellUiCulture=[Globalization.CultureInfo]::CurrentUICulture.Name;SourceApplicationSha256=$plan.SourceApplication;Nilesoft=$false;WallpaperSha256=$plan.WallpaperSha256;Recorder='ScreenToGif 2.43.2';Captures=@($records);CleanupErrors=@($cleanupErrors)} |
      ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'captures.json') -Encoding UTF8
    if($cleanupErrors.Count){if(!$captureFailure){throw ($cleanupErrors -join ' | ')};Write-Warning ($cleanupErrors -join ' | ')}
}
