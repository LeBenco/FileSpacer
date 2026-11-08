# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
function EnsureCaptureLocked([IntPtr]$Window) {
    if(!$script:CaptureLocked) {
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr](Get-UiId 'IDM_TOGGLE_STATE_LOCK'),[IntPtr]::Zero)) 'Lock command could not be posted.'
        [void][FileSpacerRegression.Native]::Message($Window,0,0,0)
        $script:CaptureLocked=$true
    }
    OpenRecordedMenu $Window 'IDM_PROXY_BUTTON'
    $menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    $entry=@($menu.Entries|Where-Object {$_.Command-eq(Get-UiId 'IDM_TOGGLE_STATE_LOCK')})
    $expected=[CaptureDesktop]::Resource($script:UiExecutable,(Get-UiId 'IDS_UNLOCK_WINDOW_STATE')).Split("`t")[0]
    Assert-Ui ($entry.Count-eq1 -and $entry[0].Label.Replace('&','').Split("`t")[0]-ceq$expected.Replace('&','')) 'The actual menu must offer Unlock before recording a locked layout.'
    $script:CaptureTitleMenuBounds=[FileSpacerRegression.Native]::Bounds($menu.Window)
    CloseMenu $Window
}
function OpenCaptureEditor([IntPtr]$Window) {
        OpenRecordedMenu $Window 'IDM_CONTEXT_MENU'
        $menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
        $script:CaptureToolbarMenuBounds=[FileSpacerRegression.Native]::Bounds($menu.Window)
        $entry=@($menu.Entries|Where-Object {$_.Command-eq(Get-UiId 'IDM_SETTINGS')})
        Assert-Ui ($entry.Count-eq1) 'The actual toolbar menu must contain FileSpacer Settings.'
        $rect=$entry[0].Bounds
        ClickPoint $Window (Point (($rect.Left+$rect.Right)/2) (($rect.Top+$rect.Bottom)/2))
        Wait-Ui {
            $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object {@([FileSpacerRegression.Native]::Children($_,'SysTabControl32')).Count-eq1})
            if($dialogs.Count-eq1){$script:UiSettings=$dialogs[0];return $true};return $false
        } 'settings opened through the real menu item'
        $tabs=@([FileSpacerRegression.Native]::Children($script:UiSettings,'SysTabControl32'))
        Assert-Ui ($tabs.Count-eq1 -and [FileSpacerRegression.Native]::Message($tabs[0],0x1304,0,0)-ge2) 'The actual settings tab control must contain Display.'
        $rect=[CaptureDesktop]::TabItem($tabs[0],1)
        ClickPoint $script:UiSettings (Point ($rect.Left+$rect.Width/2) ($rect.Top+$rect.Height/2))
        $page=[IntPtr][FileSpacerRegression.Native]::Message($script:UiSettings,0x476,0,0)
        $edit=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_TITLE_MENU_EDIT'))
        $script:menuStarted=Get-Date
        ClickControl $script:UiSettings $edit
        Wait-Ui {
            $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object {[FileSpacerRegression.Native]::GetDlgItem($_,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))-ne[IntPtr]::Zero})
            if($dialogs.Count-eq1){$script:editor=$dialogs[0];return $true};return $false
        } 'real localized title-menu editor'
        Wait-Ui {(MenuRows).ContainsKey((Get-UiId 'IDM_CAPTION_COPY_PATH'))} 'rendered menu preview'
}
function Customize([IntPtr]$Window) {
    Place $Window 340 220 720 700
    EnsureCaptureLocked $Window
    # Measure real localized dialogs and menus before recording; cancel all edits.
    OpenCaptureEditor $Window
    $frames=@([CaptureDesktop]::Frame($Window),[CaptureDesktop]::Frame($script:UiSettings),[CaptureDesktop]::Frame($script:editor),$script:CaptureTitleMenuBounds,$script:CaptureToolbarMenuBounds)
    $left=[int]($frames|Measure-Object Left -Minimum).Minimum-32
    $top=[int]($frames|Measure-Object Top -Minimum).Minimum-32
    $right=[int]($frames|Measure-Object Right -Maximum).Maximum+32
    $bottom=[int]($frames|Measure-Object Bottom -Maximum).Maximum+32
    $area=[Drawing.Rectangle]::Intersect([Drawing.Rectangle]::FromLTRB($left,$top,$right,$bottom),[Windows.Forms.SystemInformation]::VirtualScreen)
    foreach($frame in $frames){Assert-Ui ($area.Contains([Drawing.Rectangle]::FromLTRB($frame.Left,$frame.Top,$frame.Right,$frame.Bottom))) 'All actual menus and dialogs must fit the capture.'}
    [void][FileSpacerRegression.Native]::PostMessage($script:editor,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'preparation editor closes without changes'
    $script:editor=[IntPtr]::Zero
    [void](Set-UiPage 0)
    Press-UiSettings 5
    Record $Window 'customize-title-menu' {
        # Every pointer transition inside this action uses MovePointer/ClickPoint.
        OpenRecordedMenu $Window 'IDM_PROXY_BUTTON'
        Start-Sleep -Milliseconds 2000
        ClickPoint $Window (Background $Window)
        Wait-Ui {$null-eq[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)} 'initial menu dismissed by clicking empty folder background'
        $otherFolders=@([FileSpacerRegression.Native]::Top('FileSpacer Item Window',[NullString]::Value,0)|Where-Object {$_-ne$Window -and [FileSpacerRegression.Native]::IsWindowVisible($_)})
        Assert-Ui ($otherFolders.Count-eq0) 'Dismissing the menu must not open another folder.'
        OpenCaptureEditor $Window
        $catalog=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))
        $label=[CaptureDesktop]::Resource($script:UiExecutable,(Get-UiId 'IDS_TITLE_MENU_RESTORE')).Split("`t")[0]
        $source=[CaptureDesktop]::CatalogItem($catalog,$label)
        $target=MenuPoint (Get-UiId 'IDM_CAPTION_COPY_PATH')
        $target.Y-=8
        SlowDrag $script:editor (Point ($source.Left+$source.Width/2) ($source.Top+$source.Height/2)) $target
        Wait-Ui {(MenuRows).ContainsKey((Get-UiId 'IDM_RESTORE_LAYOUT'))} 'restore action added by dragging'
        $source=MenuPoint (Get-UiId 'IDM_REOPEN_WINDOW')
        $rect=[FileSpacerRegression.Native]::Bounds($catalog)
        SlowDrag $script:editor $source (Point ($rect.Left+$rect.Width/2) ($rect.Bottom-35))
        Wait-Ui {!(MenuRows).ContainsKey((Get-UiId 'IDM_REOPEN_WINDOW'))} 'reopen action removed by dragging'
        ClickControl $script:editor ([FileSpacerRegression.Native]::GetDlgItem($script:editor,1))
        Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'editor changes accepted with the visible OK button'
        ClickControl $script:UiSettings ([FileSpacerRegression.Native]::GetDlgItem($script:UiSettings,1))
        Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:UiSettings)} 'settings accepted with the visible OK button'
        OpenRecordedMenu $Window 'IDM_PROXY_BUTTON'
        $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
        $ids=@($popup.Entries|ForEach-Object {$_.Command})
        Assert-Ui ($ids-contains(Get-UiId 'IDM_RESTORE_LAYOUT') -and $ids-notcontains(Get-UiId 'IDM_REOPEN_WINDOW')) 'The displayed menu must show the actual saved composition.'
        [void]$records.Add(@{Scene='CustomizedMenu';Language=$script:language;ActualCommandIds=$ids;InitialPauseMilliseconds=2000;FinalPauseMilliseconds=2000;ContinuousPointer=$true})
        # Record holds this final menu for two seconds before stopping.
    } $area
    Place $Window 340 220 720 440
    Activate-Ui $Window
    OpenRecordedMenu $Window 'IDM_PROXY_BUTTON'
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    Shot 'customized-title-menu' @($Window,$popup.Window)
    CloseMenu $Window
}
function LockedTitle([IntPtr]$Window) {
    Place $Window 340 220 680 480
    # The locked state is prepared outside any animation; no product data is used.
    EnsureCaptureLocked $Window
    Activate-Ui $Window
    Start-Sleep -Milliseconds 500
    $frame=[CaptureDesktop]::Frame($Window)
    $point=Point 1560 80
    [FileSpacerRegression.AltDragInput]::Move($point)
    Start-Sleep -Milliseconds 250
    $file=Join-Path $script:captures 'locked-title.png'
    [CaptureDesktop]::Region($file,$frame.Left,$frame.Top,480,220)
    [void]$records.Add(@{File=$script:language+'/locked-title.png';Language=$script:language;Left=$frame.Left;Top=$frame.Top;Width=480;Height=220;BinarySha256=(Get-FileHash -LiteralPath $script:UiExecutable -Algorithm SHA256).Hash;Nilesoft=$false})
}
