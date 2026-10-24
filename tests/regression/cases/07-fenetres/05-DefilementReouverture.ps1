# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
try {
    Initialize-Ui 'reopen-scroll'
    $folder=Join-Path $script:UiRoot 'ScrollFixture'; [void](New-Item -ItemType Directory -Path $folder)
    foreach ($index in 1..200) { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$index.ToString('000')+'.txt')),'fixture') }
    $other=Join-Path $script:UiRoot 'Other'; [void](New-Item -ItemType Directory -Path $other)
    $window=Open-UiFolder $folder 200
    $otherWindow=Open-UiFolder $other 0
    $work=[FileSpacerRegression.Native]::WorkArea($window)
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the test window.'
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($otherWindow,[IntPtr]::Zero,($work.Right-310),($work.Bottom-210),300,200,0x14)) 'Cannot place the other window.'
    Activate-Ui $window
    # Scroll by revealing an item far down the list, without selecting or focusing it.
    Use-UiView $window {param($v) $v.Reveal('item-120.txt')}
    $seen=@{Before=''; After=''}
    Wait-Ui { $seen.Before=Use-UiView $window {param($v) $v.VisibleRange()}; $seen.Before -like '*;item-120.txt' } 'the revealed item is the last visible one' 300 5
    Write-Host "STEP: visible range before closing: $($seen.Before)"
    Assert-Ui (($seen.Before -split ';')[0] -ne 'item-001.txt') 'The view did not scroll.'
    Assert-UiSelected $window ''
    Close-UiFolder $window
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($otherWindow,0x111,[IntPtr](Get-UiId 'IDM_REOPEN_WINDOW'),[IntPtr]::Zero)) 'Reopen command failed.'
    $window=Track-UiFolder $folder
    Wait-Ui { (Use-UiView $window {param($v) $v.Count}) -eq 200 } 'reopened folder enumerated' 0 10
    # The reopened window keeps its saved size, so the anchored last item restores the whole page.
    Wait-Ui { $seen.After=Use-UiView $window {param($v) $v.VisibleRange()}; $seen.After -eq $seen.Before } ('reopened visible range '+$seen.Before+' (last observed: '+$seen.After+')') 500 10
    Assert-UiSelected $window ''
    Write-Host 'PASS: reopening a closed window restores the visible range, anchored on the last visible item, without selecting anything.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
