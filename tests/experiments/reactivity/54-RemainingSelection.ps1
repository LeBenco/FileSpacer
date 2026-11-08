# Test-Category: UI
# Explicit host-capable experiment. Visible cases use the owned fixture caption.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path,
    [ValidateSet('fs','adopted','explorer','trace','visible','visible-adopted','columns','sort','events')][string]$Case='adopted',
    [string]$CacheDirectory='', [string]$SymbolCache='', [switch]$MissingRange)
$ErrorActionPreference='Stop'
if(Get-Process FileSpacer -ErrorAction SilentlyContinue){throw 'Close FileSpacer before building. No user process was stopped.'}
if($MissingRange-and!$CacheDirectory){throw '-MissingRange requires a previously qualified isolated cache.'}
if($Case-eq'columns'-and!$MissingRange){throw 'The columns experiment requires -MissingRange to isolate its gain.'}
. (Join-Path $ProjectRoot 'tests\regression\common\Native.ps1')
. (Join-Path $PSScriptRoot 'Fixture.ps1')
$exe=Build-NativeTest -Name RemainingSelectionTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','ShellViewIcons.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib','D3d11.lib','Dxgi.lib','Bcrypt.lib','Wtsapi32.lib','Dwmapi.lib','Dbghelp.lib','Psapi.lib')
$cache=Join-Path $RunDirectory 'cache';[void][IO.Directory]::CreateDirectory($cache)
if($CacheDirectory) {
    $base=@(Get-ChildItem -LiteralPath $CacheDirectory -Filter '*.bin' -File)
    if($base.Count-ne1){throw 'Use an isolated source cache containing one qualified Shell descriptor.'}
    Copy-Item -LiteralPath $base[0].FullName -Destination $cache
    if(!$MissingRange) {
        $stem=Join-Path $CacheDirectory ([IO.Path]::GetFileNameWithoutExtension($base[0].Name)+'-r1.range')
        foreach($path in @($stem,($stem+'.public'))){if(Test-Path -LiteralPath $path){Copy-Item -LiteralPath $path -Destination $cache}}
    }
}
if($SymbolCache){Copy-Item -LiteralPath $SymbolCache -Destination (Join-Path $cache 'symbols') -Recurse}
function Run-OwnedSelection([string]$Name,[string[]]$Arguments,[int]$TimeoutSeconds) {
    $job=Start-RecordedProcess -Executable $exe -Arguments (Join-ProcessArguments $Arguments) -OutputPath (Join-Path $RunDirectory ($Name+'.log')) -ErrorPath (Join-Path $RunDirectory ($Name+'.errors.log'))
    try {
        if(!$job.Process.WaitForExit($TimeoutSeconds*1000)) {
            $job.Process.Kill();$job.Process.WaitForExit();[void](Complete-RecordedProcess $job)
            throw ($Name+' timed out; only this owned helper was stopped; partial evidence retained.')
        }
        $code=Complete-RecordedProcess $job
        if($code-ne0){throw ($Name+' failed: '+$code+'; see '+$RunDirectory)}
    } finally {$job.Process.Dispose()}
}
if(!$MissingRange){Run-OwnedSelection 'qualification' @('--build-cache',$cache) 190}
$fixture=New-ReactivityFixture $ProjectRoot 5000
try {
    Run-OwnedSelection $Case @($RunDirectory,$fixture,$cache,$Case) 900
    if($Case-eq'sort') {
        if(@(Select-String -LiteralPath (Join-Path $RunDirectory 'sort.log') -Pattern '^SORT PASS ').Count-ne6){throw 'Incomplete sort comparison.'}
        Write-Host ('PASS six native/adopted sort checks; output='+$RunDirectory)
        return
    }
    $rows=@(Import-Csv -LiteralPath (Join-Path $RunDirectory 'remaining-selection.csv'))
    $filtered=!!$env:FS_REMAINING_MODE
    $expected=if($Case-eq'visible-adopted'){1}elseif($Case-eq'columns'){12}elseif($Case-eq'trace'){if($filtered){3}else{12}}elseif($Case-eq'explorer'){if($filtered){3}else{6}}elseif($Case-eq'visible'-or$Case-eq'events'){9}elseif($Case-eq'adopted'){if($filtered){3}else{12}}else{if($filtered){9}else{36}}
    if($rows.Count-ne$expected){throw ('Incomplete matrix: '+$rows.Count+' / '+$expected)}
    if(@($rows|Where-Object {([int]$_.arm-ne2-or$MissingRange)-and[int]$_.range_calls-ne0}).Count){throw 'Unexpected private range use in a fallback arm.'}
    if($Case-eq'events'-and@($rows|Where-Object {[int]$_.selection_events-lt1}).Count){throw 'Bulk selection notification missing.'}
    $qualified=@($rows|Where-Object arm -eq 2)
    if(!$MissingRange-and$qualified.Count-and@($qualified|Where-Object range_calls -ne '1').Count){throw 'The native range candidate was not exercised in every requested row.'}
    Write-Host ('PASS '+$rows.Count+' rows; '+$Case+'; missing_range='+$MissingRange+'; output='+$RunDirectory)
} finally {Remove-ReactivityFixture $ProjectRoot $fixture}
