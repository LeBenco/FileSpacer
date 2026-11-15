# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
$failure=$null
$script:ColumnsDiagnostic=''
try {
    Initialize-Ui 'column-proportions' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    Set-UiPreference 'DetailsColumnsEnabled' 1
    Set-UiPreference 'DetailsColumnsDeferred' 1
    Set-UiPreference 'WindowStateLockEnabled' 1
    $folder=Join-Path $script:UiRoot 'Proportions'
    [void][IO.Directory]::CreateDirectory($folder)
    1..5 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns') }
    $window=Open-UiFolder $folder 5
    Use-UiView $window {param($view) $view.Mode=4}
    Wait-UiViewMode $window 4
    $wide=Wait-Columns $window
    $bounds=[FileSpacerRegression.Native]::Bounds($window)
    Write-Host ('WIDE '+($wide.Widths -join ',')+' available='+$wide.Available+' window='+$bounds.Width)
    # Narrow the window until a column stops at its minimum width: from there
    # the widths no longer show the proportions the user had.
    $narrow=$null
    for($width=$bounds.Width-120;$width -ge 240 -and $null -eq $narrow;$width-=60) {
        Resize-ColumnsWindow $window $width
        $candidate=Wait-Columns $window
        for($i=0;$i -lt $candidate.Widths.Count;++$i) {
            if([Math]::Abs($candidate.Widths[$i]-$wide.Widths[$i]*$candidate.Available/$wide.Available) -gt 8) { $narrow=$candidate }
        }
    }
    Assert-Ui ($null -ne $narrow) 'Precondition: no column reached its minimum width while the window was narrowed.'
    Write-Host ('NARROW '+($narrow.Widths -join ',')+' available='+$narrow.Available)
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5
    $reopened=Wait-Columns $window
    Write-Host ('REOPENED '+($reopened.Widths -join ',')+' available='+$reopened.Available)
    for($i=0;$i -lt $narrow.Widths.Count;++$i) {
        Assert-Ui ([Math]::Abs($reopened.Widths[$i]-$narrow.Widths[$i]) -le 1) ('Reopening the narrow window changed column '+$i+'.')
    }
    Resize-ColumnsWindow $window $bounds.Width
    $restored=Wait-Columns $window
    Write-Host ('RESTORED '+($restored.Widths -join ',')+' available='+$restored.Available)
    Assert-Ui ($restored.Available -eq $wide.Available) 'Precondition: the window did not return to its first width.'
    for($i=0;$i -lt $wide.Widths.Count;++$i) {
        Assert-Ui ([Math]::Abs($restored.Widths[$i]-$wide.Widths[$i]) -le 1) ('Column '+$i+' lost its proportion across the session: '+$restored.Widths[$i]+' instead of '+$wide.Widths[$i]+'.')
    }
    # Proportions are retained metadata in Icons, not a Shell getter that can
    # make the strict lock capture fail. Exercise both transition directions.
    $toggle=Get-UiId 'IDM_TOGGLE_STATE_LOCK'
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48)}
    Wait-UiViewMode $window 1 48
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr]$toggle,[IntPtr]::Zero)) 'Cannot request locking.'
    Wait-Ui { (@([FileSpacerRegression.Native]::Children($window,'Static') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)}) -join '|') -match 'Window state locked|tat de la fen.tre verrouill' } 'lock in Icons with retained proportions'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5
    Wait-UiViewMode $window 1 48
    Wait-Ui { (@([FileSpacerRegression.Native]::Children($window,'Static') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)}) -join '|') -match 'Window state locked|tat de la fen.tre verrouill' } 'lock persisted after reopening in Icons'
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr]$toggle,[IntPtr]::Zero)) 'Cannot request unlocking.'
    Wait-Ui { (@([FileSpacerRegression.Native]::Children($window,'Static') | ForEach-Object {[FileSpacerRegression.Native]::Text($_)}) -join '|') -notmatch 'Window state locked|tat de la fen.tre verrouill' } 'unlock in Icons with retained proportions'
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5
    Wait-UiViewMode $window 1 48
    Use-UiView $window {param($view) $view.Mode=4}
    Wait-UiViewMode $window 4
    $afterLock=Wait-Columns $window
    for($i=0;$i -lt $wide.Widths.Count;++$i) {
        Assert-Ui ([Math]::Abs($afterLock.Widths[$i]-$wide.Widths[$i]) -le 1) ('Locking in Icons lost column proportion '+$i+'.')
    }
    Close-UiFolder $window
    $window=Open-UiFolder $folder 5
    Wait-UiViewMode $window 4
    Write-Host 'PASS: column proportions survive narrowing, reopening and locking/unlocking in Icons.'
} catch {$failure=$_;throw} finally {Stop-Ui -Failure $failure}
