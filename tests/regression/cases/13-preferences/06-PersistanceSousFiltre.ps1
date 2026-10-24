# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\SearchUi.ps1')
$testFailure=$null
function Assert-SearchLayout([IntPtr]$Window,[string]$Expected,[string]$Label) {
    $observed=@{Positions=''; Mode=0; IconSize=0; AutoArrange=$true}
    try {
        Wait-Ui {
            Use-UiView $Window {
                param($view)
                $observed.Positions=$view.Positions()
                $observed.Mode=$view.Mode; $observed.IconSize=$view.IconSize
                $observed.AutoArrange=$view.AutoArrange
            }
            return $observed.Positions -ceq $Expected -and $observed.Mode -eq 1 -and
                $observed.IconSize -eq 48 -and !$observed.AutoArrange
        } ('exact manual icon layout: '+$Label) 400
    } finally {
        @{Expected=$Expected; Observed=$observed} | ConvertTo-Json -Depth 4 |
            Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN ('layout-'+$Label+'.json')) -Encoding UTF8
    }
}
function Close-SearchApplication([IntPtr]$Window,[string]$Label) {
    Close-UiFolder $Window
    Wait-Ui { @($script:UiProcesses | Where-Object { !$_.HasExited }).Count -eq 0 } 'application exit before reading persisted layout'
    foreach ($applicationProcess in $script:UiProcesses) {
        Assert-Ui ($applicationProcess.ExitCode -eq 0) ('Application exited with '+$applicationProcess.ExitCode)
    }
    Write-Host ('STEP: application exited before reopening: '+$Label)
}
try {
    Initialize-Ui 'filter-persistence'
    Set-UiPreference 'LiveNameSearch' 1
    $folder=Join-Path $script:UiRoot 'Placement'
    [void][IO.Directory]::CreateDirectory($folder)
    $allNames=@('keep.txt','notes.md','other.txt')
    foreach ($name in $allNames) { [IO.File]::WriteAllText((Join-Path $folder $name),'fixture') }
    $window=Open-UiFolder $folder 3
    Activate-Ui $window
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48)}
    Wait-Ui { (Use-UiView $window {param($view) $view.Mode -eq 1 -and $view.IconSize -eq 48 -and $view.Count -eq 3}) } '48-pixel native icon fixture' 400
    Use-UiView $window {param($view) $view.AutoArrange=$false}
    Wait-Ui { !(Use-UiView $window {param($view) $view.AutoArrange}) } 'manual icon placement' 400
    Use-UiView $window {
        param($view)
        $view.Position('keep.txt',37,53)
        $view.Position('other.txt',219,69)
        $view.Position('notes.md',401,211)
    }
    $baseline='keep.txt=37,53;notes.md=401,211;other.txt=219,69'
    Assert-SearchLayout $window $baseline 'prepared'
    Close-SearchApplication $window 'unfiltered-baseline'
    $window=Open-UiFolder $folder 3
    Assert-SearchNames $window $allNames
    Assert-SearchLayout $window $baseline 'unfiltered-reopen'

    $edit=Get-UiSearchEdit $window
    [FileSpacerRegression.Native]::SetText($edit,'keep')
    Assert-SearchNames $window @('keep.txt')
    # Deliberately change the subset; it must not replace the complete saved layout.
    Use-UiView $window {param($view) $view.Position('keep.txt',315,157); $view.Select('keep.txt')}
    Wait-Ui { (Use-UiView $window {param($view) $view.Positions()}) -ceq 'keep.txt=315,157' } 'visible subset has a different position' 400
    [IO.File]::WriteAllText((Join-Path $folder 'keep-new.txt'),'fixture')
    Assert-SearchNames $window @('keep-new.txt','keep.txt')
    Move-Item -LiteralPath (Join-Path $folder 'keep-new.txt') -Destination (Join-Path $folder 'other-new.txt')
    Assert-SearchNames $window @('keep.txt')
    Move-Item -LiteralPath (Join-Path $folder 'other-new.txt') -Destination (Join-Path $folder 'keep-returned.txt')
    Assert-SearchNames $window @('keep-returned.txt','keep.txt')
    Remove-Item -LiteralPath (Join-Path $folder 'keep-returned.txt')
    Assert-SearchNames $window @('keep.txt')
    Close-SearchApplication $window 'nonempty-filter'

    $window=Open-UiFolder $folder 3
    Assert-SearchNames $window $allNames
    Assert-SearchLayout $window $baseline 'after-filtered-close'
    $edit=Get-UiSearchEdit $window
    [FileSpacerRegression.Native]::SetText($edit,'no-match-unique')
    Assert-SearchNames $window @()
    Close-SearchApplication $window 'empty-filter'
    $window=Open-UiFolder $folder 3
    Assert-SearchNames $window $allNames
    Assert-SearchLayout $window $baseline 'after-empty-filtered-close'
    Write-Host 'PASS: native notifications under a filter and complete icon persistence through nonempty/empty filtered closure and process restart.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
