# Test-Category: UI
# Real hidden Shell qualification and independent optional-cache publication.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [string]$SymbolCache='', [switch]$RequireRange)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$exe=Build-NativeTest -Name SelectionCacheTests -CaseDirectory $PSScriptRoot -Standard c++14 -CommonControls -ControlFlowGuard -ProductionSources @('ShellSelectionDiscovery.cpp','ShellViewSelection.cpp','MixedFolder.cpp','ShellViewHost.cpp','DPI.cpp') -Libraries @('Ole32.lib','OleAut32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib','Comctl32.lib','Gdi32.lib','Advapi32.lib','Uiautomationcore.lib')
$cache=Join-Path $RunDirectory 'range-cache';[void][IO.Directory]::CreateDirectory($cache)
if($SymbolCache){Copy-Item -LiteralPath $SymbolCache -Destination (Join-Path $cache 'symbols') -Recurse}
function Check-Range([string]$Name,[string[]]$Arguments) {
    $code=Invoke-NativeProgram $exe $Arguments (Join-Path $RunDirectory ($Name+'.log')) (Join-Path $RunDirectory ($Name+'.errors.log'))
    if($code-ne0){throw ($Name+' failed: '+$code)}
}
$worker=Invoke-NativeProgram $exe @('--build-cache',$cache) (Join-Path $RunDirectory 'qualified-worker.log') (Join-Path $RunDirectory 'qualified-worker.errors.log')
if($worker-ne0-and$worker-ne50){throw ('Range worker failed: '+$worker)}
$base=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin' -File)
if(!$base.Count-and$worker-eq50){
    $negative=@(Get-ChildItem -LiteralPath $cache -Filter '*.bin.public' -File)
    if($negative.Count-ne1-or!(Get-Content $negative[0].FullName -Raw).StartsWith('FSPUBLIC01')){throw 'Private discovery failed without a saved structural fallback decision.'}
    Check-Range 'unsupported-shell-public-reuse' @('--public-persist',$cache)
    if(@(Get-ChildItem -LiteralPath $cache -Filter '*.attempt').Count){throw 'Saved public fallback restarted discovery.'}
    if($RequireRange){throw 'Private Shell path unavailable on the required host.'}
    Write-Host 'PASS: unsupported private Shell path reuses public clearing. Optional private range not exercised.'
    return
}
if($base.Count-ne1-or$base[0].Length-ne164){throw 'Shell selection descriptor missing.'}
$range=@(Get-ChildItem -LiteralPath $cache -Filter '*.range' -File)
function New-BaseCache([string]$Name) {
    $folder=Join-Path $RunDirectory $Name;[void][IO.Directory]::CreateDirectory($folder)
    Copy-Item -LiteralPath $base[0].FullName -Destination $folder
    return $folder
}
$missing=New-BaseCache 'missing-range'
Check-Range 'missing-range' @('--range-fallback',$missing)
if(!$range.Count){
    if($worker-ne0-and!@(Get-ChildItem -LiteralPath $cache -Filter '*.range.public' -File).Count){throw 'Optional qualification failed without a saved unsupported decision.'}
    if($RequireRange){throw 'Range candidate was not qualified on the required host.'}
    Write-Host 'Optional native range unavailable: existing private path and missing-range fallback passed; acceleration not exercised.'
    return
}
if($range.Count-ne1-or$range[0].Length-ne84){throw 'Unexpected range descriptor schema.'}
Check-Range 'range-supported' @('--range-supported',$cache)
$timestamps=@($base[0].LastWriteTimeUtc,$range[0].LastWriteTimeUtc)
Check-Range 'reuse-qualified-cache' @('--build-cache',$cache)
if((Get-Item -LiteralPath $base[0].FullName).LastWriteTimeUtc-ne$timestamps[0]-or(Get-Item -LiteralPath $range[0].FullName).LastWriteTimeUtc-ne$timestamps[1]){throw 'Unchanged modules recomputed discovery.'}
$bytes=[IO.File]::ReadAllBytes($range[0].FullName)
foreach($fault in @(@{Name='schema';Offset=0},@{Name='module';Offset=8},@{Name='size';Offset=28},
        @{Name='count-target';Offset=56},@{Name='clear-target';Offset=60},@{Name='manager';Offset=68},@{Name='slot';Offset=76},@{Name='vtable';Offset=80})) {
    $folder=New-BaseCache ('bad-range-'+$fault.Name)
    $changed=[byte[]]$bytes.Clone();$changed[$fault.Offset]=$changed[$fault.Offset]-bxor1
    [IO.File]::WriteAllBytes((Join-Path $folder $range[0].Name),$changed)
    Check-Range ('reject-'+$fault.Name) @('--range-fallback',$folder)
}
foreach($shape in @('truncated','trailing')) {
    $folder=New-BaseCache ('bad-range-'+$shape)
    $changed=if($shape-eq'truncated'){[byte[]]$bytes[0..($bytes.Length-2)]}else{[byte[]]($bytes+0)}
    [IO.File]::WriteAllBytes((Join-Path $folder $range[0].Name),$changed)
    Check-Range ('reject-'+$shape) @('--range-fallback',$folder)
}
$late=New-BaseCache 'late-range'
Check-Range 'late-publication' @('--range-late',$late,$range[0].FullName)
$negative=New-BaseCache 'range-public'
$decision=Join-Path $negative ($range[0].Name+'.public')
[byte[]]$negativeBytes=[Text.Encoding]::ASCII.GetBytes('FSPUBLIC01')+$bytes[8..31]+[Text.Encoding]::ASCII.GetBytes('optional native function unavailable')
[IO.File]::WriteAllBytes($decision,$negativeBytes)
Check-Range 'saved-optional-fallback' @('--live',$negative)
Check-Range 'reuse-optional-fallback' @('--build-cache',$negative)
if(@(Get-ChildItem -LiteralPath $negative -Filter '*.attempt').Count-or(Test-Path -LiteralPath (Join-Path $negative 'symbols'))){throw 'Saved optional fallback started discovery.'}
$stale=New-BaseCache 'range-stale-public'
$negativeBytes[10]=$negativeBytes[10]-bxor1
[IO.File]::WriteAllBytes((Join-Path $stale ($range[0].Name+'.public')),$negativeBytes)
Check-Range 'obsolete-optional-fallback' @('--range-late',$stale,$range[0].FullName)
Write-Host 'PASS: real Shell range qualification; malformed/missing cache fallback; unchanged-cache reuse; independent late publication; saved optional fallback.'
