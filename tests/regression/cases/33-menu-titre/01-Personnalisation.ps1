# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'TitleMenuUi.cs') -ReferencedAssemblies $references
$failure=$null;$editor=[IntPtr]::Zero;$caseStart=Get-Date
$env:FILESPACER_TEST_NILESOFT='0'
function Button([IntPtr]$Window,[int]$Id){[TitleMenuUi]::Click([FileSpacerRegression.Native]::GetDlgItem($Window,$Id))}
function Editor {
    $page=Set-UiPage 1
    [TitleMenuUi]::Capture($script:UiSettings,(Join-Path $env:FS_REGRESSION_RUN 'preferences-display.png'))
    # A foreign foreground window makes the real click fail as covered: activate
    # the sheet first (caption click when Windows denies the transfer), as other cases do.
    Activate-Ui $script:UiSettings
    Button $page (Get-UiId 'IDC_TITLE_MENU_EDIT')
    $found=@{Value=[IntPtr]::Zero}
    Wait-Ui {$d=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object{[FileSpacerRegression.Native]::GetDlgItem($_,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))-ne[IntPtr]::Zero});if($d.Count-eq1){$found.Value=$d[0];return $true};return $false} 'title menu editor'
    $script:editor=$found.Value
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowEnabled([FileSpacerRegression.Native]::GetDlgItem($script:editor,1))} 'rendered menu editor'
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowVisible($script:editor)-and[FileSpacerRegression.Native]::GetForegroundWindow()-eq$script:editor} 'visible foreground editor'
    Assert-Ui (![FileSpacerRegression.Native]::IsWindowEnabled($script:UiSettings)) 'Preferences must be disabled by the modal editor.'
    Assert-Ui ([TitleMenuUi]::GetWindow($script:editor,4)-eq$script:UiSettings) 'Editor has the wrong owner.'
    Assert-Ui (![FileSpacerRegression.Native]::IsWindowVisible([FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_NS_NOTICE')))) 'NS must not modify the /test editor.'
    return $script:editor
}
function Frame {
    $folders=@(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'FileSpacerMenu-*'|Where-Object{$_.CreationTime-ge$caseStart}|Sort-Object LastWriteTime -Descending)
    foreach($folder in $folders){$file=Join-Path $folder.FullName 'render\frame.tsv';if(Test-Path -LiteralPath $file){return @(Get-Content -LiteralPath $file)}}
    throw 'Rendered native menu metadata not found.'
}
function Copy-Point {
    $lines=Frame;$row=@($lines|Select-Object -Skip 1|ForEach-Object{,$_.Split("`t")}|Where-Object{[int]$_[0]-eq(Get-UiId 'IDM_CAPTION_COPY_PATH')})[0]
    $preview=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_PREVIEW'))
    $rect=[FileSpacerRegression.Native]::Bounds($preview)
    return @{Window=$preview;X=($rect.Left+48+([int]$row[5]+[int]$row[7])/2);Y=($rect.Top+41+([int]$row[6]+[int]$row[8])/2)}
}
function Has-Copy {
    $catalog=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))
    return @([TitleMenuUi]::Catalog($catalog)|Where-Object{$_-match'Copy.*path|Copier le chemin'}).Count-eq1
}
function Stored { $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test');try{return $key.GetValue('TitleMenu',$null)}finally{$key.Dispose()} }
function ActualMenu([IntPtr]$Window,[bool]$CopyPresent){
    Activate-Ui $Window;$id=Get-UiId 'IDM_PROXY_BUTTON'
    $bar=@([FileSpacerRegression.Native]::Children($Window,'ToolbarWindow32')|Where-Object{[FileSpacerRegression.Native]::Message($_,0x419,$id,0)-ge0})[0]
    $rect=[FileSpacerRegression.Native]::ToolbarButton($bar,$id)
    [TitleMenuUi]::ClickAt($bar,(($rect.Left+$rect.Right)/2),(($rect.Top+$rect.Bottom)/2))
    $popup=@{Value=[IntPtr]::Zero}
    Wait-Ui {$items=@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0));if($items.Count-eq1){$popup.Value=$items[0];return $true};return $false} 'actual title popup'
    $ids=[TitleMenuUi]::MenuIds($popup.Value)
    Assert-Ui (($ids-contains(Get-UiId 'IDM_CAPTION_COPY_PATH'))-eq$CopyPresent) 'The real title menu did not apply the validated composition.'
    [FileSpacerRegression.Native]::Keys($Window,0x1b,$false,$false)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($popup.Value)} 'actual title popup closed'
}
try {
    Initialize-Ui 'title-menu-editor'
    $folder=Join-Path $script:UiRoot 'Menu';[void][IO.Directory]::CreateDirectory($folder)
    $window=Open-UiFolder $folder 0
    $ownerProcessId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$ownerProcessId)
    $ownerProcess=Get-Process -Id $ownerProcessId
    $ownerHandle=$ownerProcess.Handle
    Assert-Ui ($ownerHandle-ne[IntPtr]::Zero) 'Cannot retain the owner process handle.'
    [void](Show-UiSettings $window)
    [void](Editor)
    Assert-Ui (!(Has-Copy)) 'An action already in the menu must be absent from the catalog.'
    $point=Copy-Point;$rect=[FileSpacerRegression.Native]::Bounds($script:editor)
    # Outside the entire dialog, rather than over the catalog: the last requested gesture.
    [TitleMenuUi]::Drag($point.Window,$point.X,$point.Y,($rect.Right+24),($rect.Top+60),$false)
    Wait-Ui {Has-Copy} 'outside drop removes the row'
    Assert-Ui ($null-eq(Stored)) 'Editing must not persist before OK.'
    Button $script:editor 2;Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'cancel editor'
    Assert-Ui ($null-eq(Stored)) 'Cancel must keep the original composition.'
    [void](Editor)
    $point=Copy-Point;$rect=[FileSpacerRegression.Native]::Bounds($script:editor)
    [TitleMenuUi]::Drag($point.Window,$point.X,$point.Y,($rect.Right+24),($rect.Top+60),$true)
    Assert-Ui (!(Has-Copy)) 'Escape must not remove a dragged row.'
    $point=Copy-Point
    [TitleMenuUi]::Drag($point.Window,$point.X,$point.Y,($rect.Left+10),($rect.Top+60),$false)
    Wait-Ui {Has-Copy} 'drop on dialog background removes the row'
    Button $script:editor 1;Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'commit editor'
    $data=Stored;Assert-Ui ($null-ne$data) 'OK did not save the menu.'
    $entries=@();for($i=4;$i-lt$data.Length;$i+=4){$entries+=[BitConverter]::ToUInt32($data,$i)}
    Assert-Ui ($entries-notcontains0) 'Removed action remains in persisted composition.'
    Press-UiSettings 5
    ActualMenu $window $false
    [void](Show-UiSettings $window)
    [void](Editor)
    Assert-Ui (Has-Copy) 'Saved composition was not loaded.'
    $catalog=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))
    $label=@([TitleMenuUi]::Catalog($catalog)|Where-Object{$_-match'Copy.*path|Copier le chemin'})[0]
    $from=[TitleMenuUi]::CatalogPoint($catalog,$label)
    $preview=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_PREVIEW'))
    $target=[FileSpacerRegression.Native]::Bounds($preview)
    [TitleMenuUi]::Drag($catalog,$from[0],$from[1],($target.Left+100),($target.Top+45),$false)
    Wait-Ui {!(Has-Copy)} 'add a catalog action to the native menu'
    Button $script:editor 2;Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'cancel added action'
    [void](Editor)
    Assert-Ui (Has-Copy) 'Cancelling an added action changed the stored composition.'
    Button $script:editor (Get-UiId 'IDC_TITLE_MENU_DEFAULT')
    Wait-Ui {!(Has-Copy)} 'restore defaults in editor'
    [TitleMenuUi]::Capture($script:editor,(Join-Path $env:FS_REGRESSION_RUN 'editor-native.png'))
    Button $script:editor 1;Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor)} 'save defaults'
    Press-UiSettings 5
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($window)) 'Owner was not reenabled.'
    ActualMenu $window $true
    # Closing the calling folder must cancel the nested editor as well.
    [void](Show-UiSettings $window);[void](Editor)
    $sheet=$script:UiSettings
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($window)-and!$ownerProcess.HasExited) 'Owner disappeared before its deliberate close.'
    $currentProcessId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$currentProcessId)
    Assert-Ui ($currentProcessId-eq$ownerProcessId-and[FileSpacerRegression.Native]::Class($window)-eq'FileSpacer Item Window'-and$ownerProcess.Path-eq$script:UiExecutable) 'Owner identity changed before close.'
    Write-Host "STEP: closing live owner HWND=$window, PID=$ownerProcessId with nested editor open"
    Close-UiFolder $window
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:editor) -and ![FileSpacerRegression.Native]::IsWindow($sheet)} 'owner close cancels nested preferences'
    Wait-Ui {$ownerProcess.HasExited} 'owner process exit'
    $ownerExitCode=[TitleMenuUi]::ProcessExitCode($ownerHandle)
    Write-Host ('STEP: owner PID='+$ownerProcessId+' native exit=0x'+$ownerExitCode.ToString('X8'))
    Assert-Ui ($ownerExitCode-eq0) ('Owner process did not exit cleanly: '+$ownerExitCode)
    $script:UiSettings=[IntPtr]::Zero
    Write-Host 'PASS: modal ownership, /test native rendering, filtered actions, outside/background drop removal, Escape, staged Cancel/OK and defaults.'
}catch{$failure=$_;throw}
finally{
    if($editor-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($editor)){[void][FileSpacerRegression.Native]::PostMessage($editor,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
    Stop-Ui -Failure $failure
}
