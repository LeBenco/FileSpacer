# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'This experiment belongs to the dedicated VM.'}
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\ViewLayout.ps1')
function Get-ScrollTarget([IntPtr]$Window){
    $view=Use-UiView $Window {param($v) $v.Window}
    $ownerId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
    return @{Window=$view;Owner=[int]$ownerId}
}
function Read-Scroll($Target){return [FileSpacerRegression.ViewLayout]::VerticalScrollPercent($Target.Window,$Target.Owner)}
function Set-Scroll($Target,[double]$Percent){return [FileSpacerRegression.ViewLayout]::SetVerticalScrollPercent($Target.Window,$Target.Owner,$Percent)}
function Get-SelectionState([IntPtr]$Window){
    return @{Selected=((Use-UiView $Window {param($v) $v.Selected()})-join'|');
        Focused=(Use-UiView $Window {param($v) $v.Focused()});
        Keyboard=[FileSpacerRegression.Native]::FocusedWindowInOwningThread($Window);
        Foreground=[FileSpacerRegression.Native]::GetForegroundWindow()}
}
$testFailure=$null
$rows=New-Object Collections.Generic.List[object]
try{
    $parent=Join-Path $ProjectRoot 'build\regression\fixtures'
    [void][IO.Directory]::CreateDirectory($parent)
    Initialize-Ui 'scroll-percent' -FixtureParent $parent
    $other=Join-Path $script:UiRoot 'Other';[void][IO.Directory]::CreateDirectory($other)
    $otherWindow=Open-UiFolder $other 0
    foreach($count in @(200,5000)){
        $folder=Join-Path $script:UiRoot ('Items-'+$count);[void][IO.Directory]::CreateDirectory($folder)
        foreach($index in 0..($count-1)){[IO.File]::WriteAllText((Join-Path $folder ('item-'+$index.ToString('00000')+'.txt')),'fixture')}
        $window=Open-UiFolder $folder $count
        $work=[FileSpacerRegression.Native]::WorkArea($window)
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the fixture.'
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($otherWindow,[IntPtr]::Zero,($work.Right-310),($work.Bottom-210),300,200,0x14)) 'Cannot place the companion.'
        Activate-Ui $window
        Assert-Ui ((Use-UiView $window {param($v) $v.Mode})-eq4) 'Details mode required.'
        Use-UiView $window {param($v) $v.Select('item-00007.txt');$v.Focus()}
        $target=Get-ScrollTarget $window
        for($rep=0;$rep-lt3;++$rep){
            $watch=[Diagnostics.Stopwatch]::StartNew();$value=Read-Scroll $target;$watch.Stop()
            $rows.Add([pscustomobject]@{Items=$count;Operation='capture';Repetition=$rep;Ms=$watch.Elapsed.TotalMilliseconds;Percent=$value})
        }
        $middle='item-'+([int]($count/2)).ToString('00000')+'.txt'
        Use-UiView $window {param($v) $v.Reveal($middle)}
        $before=Read-Scroll $target
        Assert-Ui ($before-gt0) 'The fixture did not scroll.'
        Close-UiFolder $window
        Assert-Ui ([FileSpacerRegression.Native]::PostMessage($otherWindow,0x111,[IntPtr](Get-UiId 'IDM_REOPEN_WINDOW'),[IntPtr]::Zero)) 'Reopen failed.'
        $window=Track-UiFolder $folder
        Wait-Ui {(Use-UiView $window {param($v) $v.Count})-eq$count} 'reopened fixture enumeration' 0 10
        $target=Get-ScrollTarget $window
        $after=Read-Scroll $target
        Write-Host "OBSERVED: items=$count saved=$before reopened=$after"
        $state=Get-SelectionState $window
        Assert-Ui ($state.Selected-eq'item-00007.txt') 'Reopening did not retain the selected identity.'
        for($rep=0;$rep-lt3;++$rep){
            [void](Set-Scroll $target 0)
            $watch=[Diagnostics.Stopwatch]::StartNew();$value=Set-Scroll $target $before;$watch.Stop()
            $rows.Add([pscustomobject]@{Items=$count;Operation='restore';Repetition=$rep;Ms=$watch.Elapsed.TotalMilliseconds;Percent=$value})
            Assert-Ui ([Math]::Abs((Read-Scroll $target)-$before)-lt(100.0/$count)) 'ScrollPattern did not restore the saved position.'
            $actual=Get-SelectionState $window
            foreach($key in @('Selected','Focused','Keyboard','Foreground')){Assert-Ui ($actual[$key]-ceq$state[$key]) ('ScrollPattern changed '+$key)}
        }
        Close-UiFolder $window
        $watch=[Diagnostics.Stopwatch]::StartNew();$rejected=$false
        try{[void](Set-Scroll $target $before)}catch{$rejected=$true;Write-Host ('EXPECTED: closed view rejected: '+$_.Exception.Message)}
        $watch.Stop();Assert-Ui $rejected 'A closed view must not accept a scroll operation.'
        $rows.Add([pscustomobject]@{Items=$count;Operation='closed-view';Repetition=0;Ms=$watch.Elapsed.TotalMilliseconds;Percent=-1})
        $rows|Select-Object Items,Operation,Repetition,@{Name='Ms';Expression={([double]$_.Ms).ToString('R',[Globalization.CultureInfo]::InvariantCulture)}},@{Name='Percent';Expression={([double]$_.Percent).ToString('R',[Globalization.CultureInfo]::InvariantCulture)}}|Export-Csv -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'scroll-percent.csv') -NoTypeInformation -Encoding UTF8
        Write-Host "PASS: $count items, scroll restored; selection and both focus states retained."
    }
}catch{$testFailure=$_;throw}
finally{Stop-Ui -Failure $testFailure}
