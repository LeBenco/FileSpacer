# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\ViewLayout.ps1')
function Get-ScrollPercent([IntPtr]$Window) {
    $view=Use-UiView $Window {param($v) $v.Window}
    $processId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$processId)
    $percent=[FileSpacerRegression.ViewLayout]::VerticalScrollPercent($view,[int]$processId)
    Write-Host "MEASURE: HWND=$Window vertical_scroll_percent=$percent"
    return $percent
}
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
    Assert-Ui ((Use-UiView $window {param($v) $v.Mode}) -eq 4) 'The fixture must open in Details view.'
    $initial=Get-ScrollPercent $window
    # Scroll by revealing an item far down the list, without selecting or focusing it.
    Use-UiView $window {param($v) $v.Reveal('item-120.txt')}
    $seen=@{Before=-1; After=-1}
    Wait-Ui { $seen.Before=Get-ScrollPercent $window; $seen.Before -gt $initial } 'the list scrolled after revealing a far item' 300 5
    Write-Host "STEP: vertical scroll percent before closing: $($seen.Before)"
    Assert-UiSelected $window ''
    Close-UiFolder $window
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($otherWindow,0x111,[IntPtr](Get-UiId 'IDM_REOPEN_WINDOW'),[IntPtr]::Zero)) 'Reopen command failed.'
    $window=Track-UiFolder $folder
    Wait-Ui { (Use-UiView $window {param($v) $v.Count}) -eq 200 } 'reopened folder enumerated' 0 10
    # With 200 rows, 0.5 percent of the scroll range is less than one row.
    # Preserve the visible rows without requiring an exact sub-row pixel offset.
    Wait-Ui { $seen.After=Get-ScrollPercent $window; [Math]::Abs($seen.After-$seen.Before) -lt 0.5 } ('reopened scroll percent '+$seen.Before+' (last observed: '+$seen.After+')') 500 10
    Assert-UiSelected $window ''
    Write-Host 'PASS: reopening a closed Details window restores its scroll position without selecting anything.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
