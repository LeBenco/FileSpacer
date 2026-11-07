# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'TitleMenuUi.cs') -ReferencedAssemblies $references
$failure=$null;$editor=[IntPtr]::Zero
$env:FILESPACER_TEST_NILESOFT='0'
try {
    Initialize-Ui 'title-menu-retired-entry'
    # Version 1 includes the old slot between surviving actions. These IDs must
    # keep their meaning even when the removed entry is present in preferences.
    $legacy=[byte[]](@([uint32[]](1,0,9,10,11,12,13,14,15)|ForEach-Object{[BitConverter]::GetBytes($_)}))
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\FileSpacer\Test')
    try{$key.SetValue('TitleMenu',$legacy,[Microsoft.Win32.RegistryValueKind]::Binary)}finally{$key.Dispose()}
    $folder=Join-Path $script:UiRoot 'Menu';[void][IO.Directory]::CreateDirectory($folder)
    $window=Open-UiFolder $folder 0
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Activate-Ui $script:UiSettings
    [TitleMenuUi]::Click([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_TITLE_MENU_EDIT')))
    $found=@{Value=[IntPtr]::Zero}
    Wait-Ui {$items=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object{[FileSpacerRegression.Native]::GetDlgItem($_,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))-ne[IntPtr]::Zero});if($items.Count-eq1){$found.Value=$items[0];return $true};return $false} 'title menu editor'
    $editor=$found.Value
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowVisible($editor)-and[FileSpacerRegression.Native]::GetForegroundWindow()-eq$editor} 'visible foreground editor'
    $catalog=[FileSpacerRegression.Native]::GetDlgItem($editor,(Get-UiId 'IDC_TITLE_MENU_ACTIONS'))
    $labels=@([TitleMenuUi]::Catalog($catalog))
    Assert-Ui ($labels.Count-eq9 -and @($labels|Where-Object{[string]::IsNullOrWhiteSpace($_)}).Count-eq0) 'The catalog contains a retired action or an empty slot.'
    Assert-Ui (@($labels|Where-Object{$_-match'Copy.*path|Copier le chemin'}).Count-eq0) 'A surviving saved action was lost from the editor composition.'
    [TitleMenuUi]::Click([FileSpacerRegression.Native]::GetDlgItem($editor,2))
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($editor)} 'cancel editor'
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test')
    try{$stored=$key.GetValue('TitleMenu',$null)}finally{$key.Dispose()}
    Assert-Ui (($stored-join',')-eq($legacy-join',')) 'Reading or cancelling the editor rewrote existing preferences.'
    Press-UiSettings 5
    Activate-Ui $window;$id=Get-UiId 'IDM_PROXY_BUTTON'
    $bar=@([FileSpacerRegression.Native]::Children($window,'ToolbarWindow32')|Where-Object{[FileSpacerRegression.Native]::Message($_,0x419,$id,0)-ge0})[0]
    $rect=[FileSpacerRegression.Native]::ToolbarButton($bar,$id)
    [TitleMenuUi]::ClickAt($bar,(($rect.Left+$rect.Right)/2),(($rect.Top+$rect.Bottom)/2))
    $popup=@{Value=[IntPtr]::Zero}
    Wait-Ui {$items=@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0));if($items.Count-eq1){$popup.Value=$items[0];return $true};return $false} 'existing title composition'
    $ids=[TitleMenuUi]::MenuIds($popup.Value)
    $expected=@('IDM_CAPTION_COPY_PATH','IDM_CLOSE_DESCENDANTS','IDM_CLOSE_OTHERS','IDM_FIT_CONTENT','IDM_FOLDER_GROUPS'|ForEach-Object{Get-UiId $_})
    Assert-Ui ($ids-notcontains1125) 'A retired command remains in the real title menu.'
    foreach($command in $expected){Assert-Ui ($ids-contains$command) ('A saved action lost its command: '+$command)}
    Assert-Ui ($ids-notcontains(Get-UiId 'IDM_RENAME_PROXY')) 'Reading an existing composition unexpectedly restored the default menu.'
    [FileSpacerRegression.Native]::Keys($window,0x1b,$false,$false)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($popup.Value)} 'title popup closed'
    Write-Host 'PASS: retired command absent from the real menu and catalog, surviving saved action identifiers preserved, original preferences unchanged on Cancel.'
}catch{$failure=$_;throw}
finally{
    if($editor-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($editor)){[void][FileSpacerRegression.Native]::PostMessage($editor,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
    Stop-Ui -Failure $failure
}
