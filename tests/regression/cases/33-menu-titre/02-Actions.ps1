# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$references=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot 'TitleMenuUi.cs') -ReferencedAssemblies $references
$failure=$null;$dialog=[IntPtr]::Zero;$group=[IntPtr]::Zero
$env:FILESPACER_TEST_NILESOFT='0'
function Command([IntPtr]$Window,[string]$Name){Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr](Get-UiId $Name),[IntPtr]::Zero)) "Post command $Name"}
function Size([IntPtr]$Window){$r=[FileSpacerRegression.Native]::Bounds($Window);return "$($r.Right-$r.Left),$($r.Bottom-$r.Top)"}
function Geometry([IntPtr]$Window){$r=[FileSpacerRegression.Native]::Bounds($Window);return "$($r.Left),$($r.Top),$($r.Right),$($r.Bottom)"}
function Place([IntPtr]$Window,[int]$X,[int]$Y,[int]$Width,[int]$Height){
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$X,$Y,$Width,$Height,0x14)) 'Cannot position fixture window.'
    [void][FileSpacerRegression.Native]::Message($Window,0x232,0,0)
}
function Locked([IntPtr]$Window){return (@([FileSpacerRegression.Native]::Children($Window,'Static')|ForEach-Object{[FileSpacerRegression.Native]::Text($_)})-join'|')-match'Window state locked|État de la fenêtre verrouillé'}
function Confirm {
    $found=@{Value=[IntPtr]::Zero}
    Wait-Ui {$d=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object{[FileSpacerRegression.Native]::GetDlgItem($_,6)-ne[IntPtr]::Zero});if($d.Count-eq1){$found.Value=$d[0];return $true};return $false} 'action confirmation'
    $script:dialog=$found.Value
    [TitleMenuUi]::Click([FileSpacerRegression.Native]::GetDlgItem($script:dialog,6))
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:dialog)} 'confirmation accepted'
    $script:dialog=[IntPtr]::Zero
}
function Groups([IntPtr]$Window){
    Activate-Ui $Window
    Command $Window 'IDM_FOLDER_GROUPS';$found=@{Value=[IntPtr]::Zero}
    Wait-Ui {$d=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,0)|Where-Object{[FileSpacerRegression.Native]::GetDlgItem($_,(Get-UiId 'IDC_GROUPS_NAME'))-ne[IntPtr]::Zero});if($d.Count-eq1){$found.Value=$d[0];return $true};return $false} 'folder groups dialog'
    $script:dialog=$found.Value
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowVisible($script:dialog)-and[FileSpacerRegression.Native]::GetForegroundWindow()-eq$script:dialog} 'visible foreground groups dialog'
    return $script:dialog
}
function Button([IntPtr]$Window,[string]$Id){[TitleMenuUi]::Click([FileSpacerRegression.Native]::GetDlgItem($Window,(Get-UiId $Id)))}
function GroupPaths {
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test\FolderGroups')
    if(!$key){return @()};try{return @($key.GetValue('Regression group',@()))}finally{$key.Dispose()}
}
function SelectGroup([IntPtr]$Window){
    $id=Get-UiId 'IDC_GROUPS_LIST';$list=[FileSpacerRegression.Native]::GetDlgItem($Window,$id)
    [void][FileSpacerRegression.Native]::Message($list,0x186,0,0)
    [void][FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr]($id-bor(1-shl16)),[IntPtr]$list)
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowEnabled([FileSpacerRegression.Native]::GetDlgItem($Window,(Get-UiId 'IDC_GROUPS_RECALL')))} 'group selection'
}
try {
    Initialize-Ui 'title-menu-actions'
    Set-UiPreference 'WindowStateLockEnabled' 1
    $a=Join-Path $script:UiRoot 'Layout';$parent=Join-Path $script:UiRoot 'Parent';$child=Join-Path $parent 'Child';$grand=Join-Path $child 'Grandchild';$other=Join-Path $script:UiRoot 'Other'
    foreach($path in @($a,$grand,$other)){[void][IO.Directory]::CreateDirectory($path)}
    $window=Open-UiFolder $a 0
    Place $window 80 70 660 460;$reference=Geometry $window
    Command $window 'IDM_TOGGLE_STATE_LOCK'
    [TitleMenuUi]::Capture($window,(Join-Path $env:FS_REGRESSION_RUN 'lock-request.png'))
    Wait-Ui {Locked $window} 'reference locked'
    Place $window 140 130 760 520
    Command $window 'IDM_RESTORE_LAYOUT'
    Wait-Ui {(Geometry $window)-eq$reference} 'restore locked geometry'
    Wait-Ui {
        try {$view=Get-UiView $window} catch [InvalidCastException] {return $false}
        if(!$view){return $false};try{return $view.ContentVisible -and $view.Count-eq0}finally{$view.Dispose()}
    } 'native view restored'
    Place $window 120 110 700 480;$replacement=Geometry $window
    Command $window 'IDM_REPLACE_LAYOUT';Confirm
    Place $window 170 150 780 540
    Command $window 'IDM_RESTORE_LAYOUT';Wait-Ui {(Geometry $window)-eq$replacement} 'replacement reference stored'
    Place $window 150 140 620 440;Close-UiFolder $window
    $window=Open-UiFolder $a 0
    Assert-Ui ((Geometry $window)-eq$replacement) 'Closing and reopening lost the locked replacement geometry.'
    Assert-Ui (Locked $window) 'Closing and reopening lost the state lock.'
    Command $window 'IDM_TOGGLE_STATE_LOCK';Wait-Ui {!(Locked $window)} 'reference unlocked'
    Place $window 150 140 620 440;$unlocked=Geometry $window;Close-UiFolder $window
    $window=Open-UiFolder $a 0
    Assert-Ui ((Geometry $window)-eq$unlocked) 'Closing and reopening lost the unlocked geometry.'
    # NS window.command is forwarded by this public menu-owner control. Exercise
    # the same route with NS excluded by /test, without loading an extension.
    $caption=@([FileSpacerRegression.Native]::Children($window,'Static')|Where-Object{([FileSpacerRegression.Native]::Style($_,-16)-band31)-eq13})[0]
    $before=Size $window;Command $caption 'IDM_FIT_CONTENT'
    Wait-Ui {(Size $window)-ne$before} 'native fit-content command through menu owner'
    $r=[FileSpacerRegression.Native]::Bounds($window);$work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ($r.Right-le$work.Right -and $r.Bottom-le$work.Bottom) 'Fit-content exceeds the work area.'
    [TitleMenuUi]::Capture($window,(Join-Path $env:FS_REGRESSION_RUN 'fit-content.png'))
    $p=Open-UiFolder $parent 1;$c=Open-UiFolder $child 1;$g=Open-UiFolder $grand 0;$o=Open-UiFolder $other 0
    Command $p 'IDM_CLOSE_DESCENDANTS'
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($c) -and ![FileSpacerRegression.Native]::IsWindow($g)} 'descendant windows close'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($p) -and [FileSpacerRegression.Native]::IsWindow($o) -and [FileSpacerRegression.Native]::IsWindow($window)) 'Descendant action closed unrelated windows.'
    $group=Groups $p
    [FileSpacerRegression.Native]::SetText([FileSpacerRegression.Native]::GetDlgItem($group,(Get-UiId 'IDC_GROUPS_NAME')),'Regression group')
    Button $group 'IDC_GROUPS_SAVE'
    Wait-Ui { @(GroupPaths).Count-eq3 } 'named group saved'
    Assert-Ui (((GroupPaths|Sort-Object)-join'|')-eq((@($a,$parent,$other)|Sort-Object)-join'|')) 'Group contains incorrect folder identities.'
    [TitleMenuUi]::Capture($group,(Join-Path $env:FS_REGRESSION_RUN 'folder-groups.png'))
    [void][FileSpacerRegression.Native]::PostMessage($group,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($group)} 'close group dialog'
    $script:dialog=[IntPtr]::Zero
    Command $p 'IDM_CLOSE_OTHERS';Wait-Ui {![FileSpacerRegression.Native]::IsWindow($window) -and ![FileSpacerRegression.Native]::IsWindow($o)} 'close other windows'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($p)) 'Close-others closed its source.'
    $group=Groups $p;SelectGroup $group;Button $group 'IDC_GROUPS_RECALL'
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($group)} 'recall closes group dialog'
    $script:dialog=[IntPtr]::Zero
    $window=Track-UiFolder $a;$o=Track-UiFolder $other
    Assert-Ui (@(Find-UiFolder $parent).Count-eq1) 'Recall duplicated an already-open folder.'
    $group=Groups $p;SelectGroup $group;Button $group 'IDC_GROUPS_DELETE';Confirm
    Wait-Ui {@(GroupPaths).Count-eq0} 'named group removed'
    Assert-Ui ([IO.Directory]::Exists($a) -and [FileSpacerRegression.Native]::IsWindow($o)) 'Deleting a group deleted data or closed a folder.'
    [void][FileSpacerRegression.Native]::PostMessage($group,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($group)} 'close groups'
    $script:dialog=[IntPtr]::Zero
    Write-Host 'PASS: restore/replace layouts, locked/unlocked persistence, native command forwarding, fit content, descendant/other windows, named group save/recall/delete and real folder identities.'
}catch{$failure=$_;throw}
finally{
    foreach($candidate in @($group,$dialog)){
        if($candidate-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($candidate)){
            [void][FileSpacerRegression.Native]::PostMessage($candidate,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
        }
    }
    Stop-Ui -Failure $failure
}
