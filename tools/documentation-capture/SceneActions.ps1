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
    $expected=[CaptureDesktop]::Resource($script:UiExecutable,(Get-UiId 'IDS_UNLOCK_WINDOW_STATE'),$script:CaptureResourceLanguage).Split("`t")[0]
    Assert-Ui ($entry.Count-eq1 -and $entry[0].Label.Replace('&','').Split("`t")[0]-ceq$expected.Replace('&','')) ('The actual menu must offer Unlock before recording a locked layout. Expected: '+$expected+'; observed: '+($entry.Label-join', '))
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
        $label=[CaptureDesktop]::Resource($script:UiExecutable,(Get-UiId 'IDS_TITLE_MENU_RESTORE'),$script:CaptureResourceLanguage).Split("`t")[0]
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
function CaptureIconView([IntPtr]$Window) {
    $state=Use-UiView $Window {param($view) @{Mode=$view.Mode;Size=$view.IconSize}}
    Assert-Ui ($state.Mode-eq1 -and $state.Size-eq48) 'The screenshot must show the actual medium-icons view.'
    $headers=@([FileSpacerRegression.Native]::Children($Window,'SysHeader32')|Where-Object {[FileSpacerRegression.Native]::IsWindowVisible($_) -and [FileSpacerRegression.Native]::Bounds($_).Height-gt0})
    $root=[Windows.Automation.AutomationElement]::FromHandle($Window)
    $modernHeaders=@($root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)|Where-Object {$_.Current.ClassName-eq'UIColumnHeader' -and !$_.Current.IsOffscreen -and $_.Current.BoundingRectangle.Height-gt0})
    Assert-Ui ($headers.Count-eq0 -and $modernHeaders.Count-eq0) 'Icon view must not contain visible Details column headers.'
    return $state
}
function LockedTitle([IntPtr]$Window) {
    Place $Window 340 220 680 480
    # The locked state is prepared outside any animation; no product data is used.
    EnsureCaptureLocked $Window
    Activate-Ui $Window
    $view=CaptureIconView $Window
    Start-Sleep -Milliseconds 500
    $frame=[CaptureDesktop]::Frame($Window)
    $point=Point 1560 80
    [FileSpacerRegression.AltDragInput]::Move($point)
    Start-Sleep -Milliseconds 250
    $file=Join-Path $script:captures 'locked-title.png'
    [CaptureDesktop]::Region($file,$frame.Left,$frame.Top,480,220)
    [void]$records.Add(@{File=$script:language+'/locked-title.png';Language=$script:language;Left=$frame.Left;Top=$frame.Top;Width=480;Height=220;ViewMode=$view.Mode;IconSize=$view.Size;VisibleColumnHeaders=0;BinarySha256=(Get-FileHash -LiteralPath $script:UiExecutable -Algorithm SHA256).Hash;Nilesoft=$false})
}

function CaptureColumnHeaders([IntPtr]$Window) {
    $nativeHeaders=@([FileSpacerRegression.Native]::Children($Window,'SysHeader32'))
    if($nativeHeaders.Count-eq1){
        $headers=@([FileSpacerColumnTests.ColumnsUi]::Headers($nativeHeaders[0])|Sort-Object Left)
        Assert-Ui ($headers.Count-eq4) 'The native Details header must expose four columns.'
        return $headers
    }
    $root=[Windows.Automation.AutomationElement]::FromHandle($Window)
    $items=@($root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)|Where-Object {($_.Current.ControlType-eq[Windows.Automation.ControlType]::HeaderItem -or $_.Current.ClassName-eq'UIColumnHeader') -and !$_.Current.IsOffscreen -and $_.Current.BoundingRectangle.Width-gt0}|Sort-Object {$_.Current.BoundingRectangle.Left})
    if($items.Count-ne4){
        $root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)|ForEach-Object {$c=$_.Current;'{0}`t{1}`t{2}`t{3}`t{4}'-f $c.ControlType.ProgrammaticName,$c.ClassName,$c.AutomationId,$c.Name,$c.BoundingRectangle}|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'column-accessibility.txt') -Encoding UTF8
        throw ('The fictional folder must expose four actual Details column headers; observed '+$items.Count)
    }
    return @($items|ForEach-Object {$_.Current.BoundingRectangle})
}
function DetailsColumns([IntPtr]$Window,[string]$Folder) {
    $fixtures=Get-Content -LiteralPath (Join-Path $source 'details-columns.json') -Raw -Encoding UTF8|ConvertFrom-Json
    $files=$fixtures.($script:language)
    foreach($name in @($files.Long)+@($files.Medium)) {[IO.File]::WriteAllText((Join-Path $Folder $name),'Fictional documentation example.')}
    Set-UiPreference 'DetailsColumnsEnabled' 1
    Set-UiPreference 'DetailsColumnsDeferred' 1
    Use-UiView $Window {param($view)$view.Mode=4}
    Wait-UiViewMode $Window 4
    Place $Window 340 220 1100 500
    # The new folder starts with filling enabled. Disable it before the recording.
    [void][FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr](Get-UiId 'IDM_FILL_COLUMNS'),[IntPtr]::Zero)
    Wait-UiViewMode $Window 4
    Wait-Ui {Use-UiView $Window {param($view)$view.Count-eq11}} 'all fictional names appear'
    Wait-Ui {Use-UiView $Window {param($view)@([FileSpacerRegression.Native]::Children($view.Window,'SysListView32')).Count-eq0}} 'ordinary Details view before enabling full-width columns' 300
    $textWidths=@($files.Long|ForEach-Object {[Windows.Forms.TextRenderer]::MeasureText($_,[Drawing.SystemFonts]::IconTitleFont).Width})
    $longest=[int]($textWidths|Measure-Object -Maximum).Maximum
    Assert-Ui ($longest-ge440 -and $longest-le700) 'Long filenames must be meaningful with a normal-width Name column.'
    $initialName=[int][Math]::Round($longest*0.30)+28 # Leave room for the native icon and cell inset.
    $desired=@($initialName,($initialName*2),100,80)
    for($index=0;$index-lt4;$index++) {
        $headers=CaptureColumnHeaders $Window
        $header=$headers[$index]
        $from=Point ([int]$header.Right-1) ([int]($header.Top+$header.Height/2))
        $to=Point ($from.X+$desired[$index]-[int]$header.Width) $from.Y
        SlowDrag $Window $from $to
    }
    $initialHeaders=CaptureColumnHeaders $Window
    OpenRecordedMenu $Window 'IDM_VIEW_MENU'
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    $entry=@($popup.Entries|Where-Object {$_.Command-eq(Get-UiId 'IDM_FILL_COLUMNS')})
    Assert-Ui ($entry.Count-eq1 -and ($entry[0].State-band8)-eq0) 'The actual View menu must show filling unchecked.'
    $menuBounds=[FileSpacerRegression.Native]::Bounds($popup.Window)
    CloseMenu $Window
    $frame=[CaptureDesktop]::Frame($Window)
    $area=[Drawing.Rectangle]::Intersect([Drawing.Rectangle]::FromLTRB([Math]::Min($frame.Left,$menuBounds.Left)-24,[Math]::Min($frame.Top,$menuBounds.Top)-24,[Math]::Max($frame.Right,$menuBounds.Right)+24,[Math]::Max($frame.Bottom,$menuBounds.Bottom)+24),[Windows.Forms.SystemInformation]::VirtualScreen)
    foreach($bounds in @($frame,$menuBounds)){Assert-Ui ($area.Contains([Drawing.Rectangle]::FromLTRB($bounds.Left,$bounds.Top,$bounds.Right,$bounds.Bottom))) 'The complete folder and View menu must fit the capture.'}
    Wait-UiViewMode $Window 4
    Wait-Ui {Use-UiView $Window {param($view)$view.ContentVisible -and $view.Count-eq11}} 'Details contents restored after menu preflight' 300
    $initialHeaders=CaptureColumnHeaders $Window
    Write-Host ('Details capture: '+$area+'; initial widths: '+(@($initialHeaders|ForEach-Object Width)-join','))
    Record $Window 'details-columns' {
        Start-Sleep -Milliseconds 1500
        OpenRecordedMenu $Window 'IDM_VIEW_MENU'
        Start-Sleep -Milliseconds 900
        $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
        $entry=@($popup.Entries|Where-Object {$_.Command-eq(Get-UiId 'IDM_FILL_COLUMNS')})
        Assert-Ui ($entry.Count-eq1) 'The visible menu must contain the actual per-folder column command.'
        $rect=$entry[0].Bounds
        ClickPoint $Window (Point (($rect.Left+$rect.Right)/2) (($rect.Top+$rect.Bottom)/2)) 1000
        Wait-UiViewMode $Window 4
        Wait-Ui {Use-UiView $Window {param($view)@([FileSpacerRegression.Native]::Children($view.Window,'SysListView32')).Count-eq1}} 'full-width Details view after the visible menu activation' 300
        $filled=CaptureColumnHeaders $Window
        $available=Use-UiView $Window {param($view)$list=@([FileSpacerRegression.Native]::Children($view.Window,'SysListView32'));[FileSpacerRegression.Native]::ClientBounds($list[0]).Width}
        Assert-Ui ([Math]::Abs(($filled|Measure-Object Width -Sum).Sum-$available)-le2) 'The actual columns must fill the folder viewport.'
        Assert-Ui ($filled[0].Width-gt$initialHeaders[0].Width -and $filled[0].Width-lt($textWidths|Measure-Object -Minimum).Minimum*0.80) 'At least two long filenames must still have more than twenty percent hidden after filling.'
        Start-Sleep -Milliseconds 1500
        $header=$filled[0]
        $from=Point ([int]$header.Right-1) ([int]($header.Top+$header.Height/2))
        $to=Point ($from.X+$longest+40-[int]$header.Width) $from.Y
        MovePointer $from
        Start-Sleep -Milliseconds 600
        [FileSpacerRegression.AltDragInput]::BeginConfigured($Window,$from,0,0)
        try {MovePointer $to 1800;Start-Sleep -Milliseconds 500}finally{[FileSpacerRegression.AltDragInput]::End()}
        Wait-Ui {$headers=CaptureColumnHeaders $Window;$headers[0].Width-ge$longest+35} 'the actual manual drag exposes the complete long names' 300
        $after=CaptureColumnHeaders $Window
        Start-Sleep -Milliseconds 1500
        MovePointer (Background $Window)
        [void]$records.Add(@{Scene='DetailsColumns';Language=$script:language;Files=@($files.Long)+@($files.Medium);GlobalEnabled=1;UpdateOnRelease=1;BeforeWidths=@($initialHeaders|ForEach-Object Width);FilledWidths=@($filled|ForEach-Object Width);AfterWidths=@($after|ForEach-Object Width);LongNameTextWidths=$textWidths;CommandId=(Get-UiId 'IDM_FILL_COLUMNS');OptionHoverPauseMilliseconds=1000;ContinuousPointer=$true;SourceExecutableSha256=(Get-FileHash -LiteralPath $script:UiExecutable).Hash})
    } $area
}
