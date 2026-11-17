# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot 'GridDrag.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'ExternalDragSource.cs') -ReferencedAssemblies @('System.Windows.Forms.dll','System.Drawing.dll')
$failure=$null;$source=$null;$sourceWindow=[IntPtr]::Zero
$outcomes=New-Object Collections.Generic.List[object]
function Read-SourceFeedback([string]$Path) {
    $stream=$null;$reader=$null
    try {
        $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
        $reader=New-Object IO.StreamReader($stream);return $reader.ReadToEnd()
    } catch [IO.IOException] {return ''} finally {if($reader){$reader.Dispose()}elseif($stream){$stream.Dispose()}}
}
function New-DragPoint([int]$X,[int]$Y) {
    $p=New-Object FileSpacerRegression.MousePoint;$p.X=$X;$p.Y=$Y;return $p
}
try {
    Initialize-Ui 'external-after-snap' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'MixedFolderSorting' 1
    $folder=Join-Path $script:UiRoot 'Target';$external=Join-Path $script:UiRoot 'External'
    [void][IO.Directory]::CreateDirectory((Join-Path $folder 'Destination'))
    [void][IO.Directory]::CreateDirectory($external)
    [IO.File]::WriteAllText((Join-Path $folder 'S-seed.txt'),'stationary payload')
    foreach($name in @('folder-drop.txt','background-drop.txt')){[IO.File]::WriteAllText((Join-Path $external $name),('external payload '+$name))}
    $source=[FileSpacerViewMenuTests.ExternalDragSource]::Start($external)
    Wait-Ui {$source.Refresh();$script:sourceWindow=$source.MainWindowHandle;$sourceWindow-ne[IntPtr]::Zero} 'external OLE source window'
    $window=Open-UiFolder $folder 2
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,30,40,760,600,0x14)) 'Cannot size target fixture.'
    Set-GridMode;Choose-Placement 'IDM_FREE_PLACEMENT_ICONS'
    Wait-Ui {Use-UiView $window {param($view) $view.FolderClassId-eq[guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604'}} 'mixed-provider target is active' 150
    $body=Use-UiView $window {param($view) $view.Window}
    $lists=@([FileSpacerRegression.Native]::Children($body,'SysListView32')|Where-Object {[FileSpacerRegression.Native]::IsWindowEnabled($_)})
    Assert-Ui ($lists.Count-eq1) 'Expected one native target list.';$list=$lists[0]
    $targetProcess=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$targetProcess)
    Assert-Ui ($targetProcess-ne$source.Id) 'The OLE source must be a different process.'
    Use-UiView $window {param($view) $view.Position('S-seed.txt',350,140);$view.Select('S-seed.txt')}
    Activate-Ui $window
    $bounds=[FileSpacerRegression.Native]::ClientBounds($list)
    $seedIndex=Use-UiView $window {param($view) $view.IndexOf('S-seed.txt')}
    $seedRect=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$seedIndex)
    $start=New-DragPoint ($bounds.Left+[int](($seedRect[0]+$seedRect[2])/2)) ($bounds.Top+[int](($seedRect[1]+$seedRect[3])/2))
    $drop=New-DragPoint ($bounds.Left+420) ($bounds.Top+320)
    $beforeSnap=Use-UiView $window {param($view) $view.Positions()}
    [FileSpacerRegression.AltDragInput]::Begin($window,$start,$false,$false)
    Wait-Ui {![FileSpacerRegression.AltDragInput]::Idle($window)} 'native seed press captured'
    for($step=1;$step-le6;++$step){[FileSpacerRegression.AltDragInput]::Move((New-DragPoint ($start.X+[int](($drop.X-$start.X)*$step/6)) ($start.Y+[int](($drop.Y-$start.Y)*$step/6))))}
    [FileSpacerViewMenuTests.GridKey]::Hold($true)
    Wait-Ui {[FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'S frame before releasing only the mouse' 0 5
    [FileSpacerRegression.AltDragInput]::ReleasePrimary()
    Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($window)-and![FileSpacerViewMenuTests.GridKey]::Marker($body).Visible} 'Snap placement completed with S still held' 150
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()})-cne$beforeSnap} 'selected icon placement applied before deactivation' 150
    Assert-Ui ([FileSpacerViewMenuTests.ExternalDragSource]::SHeld()) 'S was released before external activation.'
    Activate-Ui $sourceWindow
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    Wait-Ui {![FileSpacerViewMenuTests.ExternalDragSource]::SHeld()} 'S release delivered outside FileSpacer' 0 5
    # Read-only geometry must not click the target or start another FS gesture.
    $index=Use-UiView $window {param($view) $view.IndexOf('Destination')}
    $icon=[FileSpacerViewMenuTests.GridKey]::IconBounds($list,$index)
    $folderPoint=New-DragPoint ($bounds.Left+[int](($icon[0]+$icon[2])/2)) ($bounds.Top+[int](($icon[1]+$icon[3])/2))
    $backgroundPoint=New-DragPoint ($bounds.Left+220) ($bounds.Top+250)
    foreach($route in @('folder','background')) {
        $name=if($route-eq'folder'){'folder-drop.txt'}else{'background-drop.txt'}
        $wanted=if($route-eq'folder'){Join-Path $folder ('Destination/'+$name)}else{Join-Path $folder $name}
        $point=if($route-eq'folder'){$folderPoint}else{$backgroundPoint}
        $coordinates=[FileSpacerViewMenuTests.ExternalDragSource]::ButtonPoint($sourceWindow,$name)
        Activate-Ui $sourceWindow
        $hoverPath=Join-Path $external ($name+'.hover')
        [FileSpacerRegression.AltDragInput]::Begin($sourceWindow,(New-DragPoint $coordinates[0] $coordinates[1]),$false,$false)
        Wait-Ui {Test-Path -LiteralPath (Join-Path $external ($name+'.started'))} 'external source received its mouse press before motion' 0 5
        for($step=1;$step-le6;++$step){[FileSpacerRegression.AltDragInput]::Move((New-DragPoint ($coordinates[0]+[int](($point.X-$coordinates[0])*$step/6)) ($coordinates[1]+[int](($point.Y-$coordinates[1])*$step/6))))}
        $hover=@{Value=$null}
        Wait-Ui {
            try {$hover.Value=Read-SourceFeedback $hoverPath;return $hover.Value.StartsWith(($point.X.ToString()+','+$point.Y.ToString()+','))}
            catch [IO.IOException] {return $false}
        } 'native feedback reached the exact target before release' 0 5
        [FileSpacerRegression.AltDragInput]::ReleasePrimary()
        Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($sourceWindow)} 'external native drag completed' 150
        $errorText=$null
        try {
            Wait-Ui {(Test-Path -LiteralPath $wanted)-and!(Test-Path -LiteralPath (Join-Path $external $name))} ('external '+$route+' drop moved the exact file') 0 8
            Assert-Ui ([IO.File]::ReadAllText($wanted)-ceq('external payload '+$name)) 'External drop changed payload.'
        } catch {$errorText=$_.Exception.Message}
        $effectPath=Join-Path $external ($name+'.effect')
        $outcomes.Add(@{Route=$route;Error=$errorText;DestinationExists=(Test-Path -LiteralPath $wanted);SourceExists=(Test-Path -LiteralPath (Join-Path $external $name));Hover=$hover.Value;Effect=$(if(Test-Path -LiteralPath $effectPath){[IO.File]::ReadAllText($effectPath)}else{'missing'});SourcePid=$source.Id;TargetPid=$targetProcess})
        ConvertTo-Json -InputObject $outcomes.ToArray() -Depth 4|Set-Content (Join-Path $env:FS_REGRESSION_RUN 'external-drop-outcomes.json') -Encoding UTF8
    }
    Assert-Ui (@($outcomes|Where-Object {$_.Error}).Count-eq0) (($outcomes|Where-Object {$_.Error}|ForEach-Object {$_.Route+': '+$_.Error})-join'; ')
    Assert-Ui ([IO.File]::ReadAllText((Join-Path $folder 'S-seed.txt'))-ceq'stationary payload') 'Seed icon operation changed the source file.'
    Activate-Ui $window
    Use-UiView $window {param($view) $view.Select('background-drop.txt')}
    [FileSpacerViewMenuTests.GridKey]::Hold($true)
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    Wait-Ui {(Read-PreviewState $window).Selected-ceq'S-seed.txt'} 'type-to-select is restored after the external S release' 0 5
    Write-Host 'PASS: S released in an external process leaves native folder and background file drops available.'
} catch {$failure=$_;throw} finally {
    [FileSpacerViewMenuTests.GridKey]::Hold($false)
    [FileSpacerRegression.AltDragInput]::End()
    if($sourceWindow-ne[IntPtr]::Zero-and[FileSpacerRegression.Native]::IsWindow($sourceWindow)) {
        [void][FileSpacerRegression.Native]::PostMessage($sourceWindow,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    }
    if($source){
        try {
            if(!$source.WaitForExit(5000)){
                Stop-TestCaseProcess $source
                if(!$failure){$failure=New-Object InvalidOperationException 'The external source did not close normally.'}
            }
        } catch {if(!$failure){$failure=$_}} finally {$source.Dispose()}
    }
    Stop-Ui -Failure $failure
    if($failure){throw $failure}
}
