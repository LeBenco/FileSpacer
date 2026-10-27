param([switch]$Regression)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$inputRoot=if($Regression){[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))}else{$PSScriptRoot}
$outputRoot=Join-Path (Split-Path $inputRoot -Parent) 'output'
[void][IO.Directory]::CreateDirectory($outputRoot)
$report=[ordered]@{Status='BLOCKED';RunId=$null;GuestMachine=$env:COMPUTERNAME;SessionId=[Diagnostics.Process]::GetCurrentProcess().SessionId;IsAdministrator=$null;SessionQuery=$null;SessionQueryExitCode=$null;DesktopProcesses=@();BuildLocation='Host';Configuration='Debug';GuestCompilation=$false;ExecutableSha256=$null;Gesture=$null;StateRestored=$false;ProductionUnchanged=$false;Error=$null;CleanupErrors=@()}
$activity=$null;$statePrepared=$false;$initialFingerprint=$null
$transcriptStarted=$false
$testKey='Software\FileSpacer\Test'
$database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
$normalDirectory=Split-Path $database -Parent
$parentExisted=$true;$directoryExisted=$true
$testFingerprint=$null
function Get-TestFingerprint {
    function Read-Key([Microsoft.Win32.RegistryKey]$Key) {
        $tree=[ordered]@{Values=@();Children=@()}
        foreach($name in @($Key.GetValueNames() | Sort-Object)){$tree.Values+=@{Name=$name;Kind=$Key.GetValueKind($name).ToString();Value=$Key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}}
        foreach($name in @($Key.GetSubKeyNames() | Sort-Object)){$child=$Key.OpenSubKey($name);try{$tree.Children+=@{Name=$name;Tree=(Read-Key $child)}}finally{$child.Dispose()}}
        return $tree
    }
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($testKey)
    try{$tree=if($key){Read-Key $key}else{'absent'}}finally{if($key){$key.Dispose()}}
    $files=@();foreach($suffix in @('','-wal','-shm')){$file=$database+$suffix;$files+=if(Test-Path -LiteralPath $file){(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}else{'absent'}}
    return ([ordered]@{Registry=$tree;Files=$files} | ConvertTo-Json -Depth 30 -Compress)
}

function Get-ProductionFingerprint {
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer')
    $values=@()
    try {
        if($key){foreach($name in ($key.GetValueNames() | Sort-Object)){
            $values+=[ordered]@{Name=$name;Kind=$key.GetValueKind($name).ToString();Value=$key.GetValue($name,$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)}
        }}
        $description=[ordered]@{RegistryExists=($null-ne$key);Values=$values;Databases=[ordered]@{}}
    }finally{if($key){$key.Dispose()}}
    foreach($suffix in @('','-wal','-shm')){
        $file=Join-Path $normalDirectory ('folder-state.sqlite3'+$suffix)
        $description.Databases[$suffix]=if(Test-Path -LiteralPath $file){(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}else{'absent'}
    }
    return ($description | ConvertTo-Json -Depth 6 -Compress)
}
try {
    [void](Start-Transcript -LiteralPath (Join-Path $outputRoot 'worker.log'))
    $transcriptStarted=$true
    $manifest=Get-Content -LiteralPath (Join-Path $inputRoot 'manifest.json') -Raw | ConvertFrom-Json
    $report.RunId=$manifest.RunId;$report.ExecutableSha256=$manifest.ExecutableSha256
    if($env:COMPUTERNAME-eq$manifest.HostMachine -or $env:COMPUTERNAME-ne$manifest.GuestMachine){throw 'Expected the declared Hyper-V guest, never the host.'}
    if($manifest.Configuration-ne'Debug' -or ![Environment]::UserInteractive -or $report.SessionId-ne$manifest.GuestSessionId){throw 'The declared interactive Debug test session is unavailable.'}
    if([Security.Principal.WindowsIdentity]::GetCurrent().User.Value-ne$manifest.GuestAccountSid){throw 'Guest test account differs from the connected account.'}
    $testPrincipal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
    $report.IsAdministrator=$testPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if($report.IsAdministrator){throw 'The UI test must run with the declared Limited token.'}
    $report.SessionQuery=(& (Join-Path $env:SystemRoot 'System32\query.exe') session $report.SessionId 2>&1 | Out-String)
    $report.SessionQueryExitCode=$LASTEXITCODE
    $report.DesktopProcesses=@(Get-Process LogonUI,LockApp,explorer,rdpclip -ErrorAction SilentlyContinue | Select-Object ProcessName,Id,SessionId)
    if($report.SessionQuery-notmatch'(?m)^>\s*console\s'){throw 'The guest session is not attached to its console after preparation.'}
    if(@(Get-Process FileSpacer -ErrorAction SilentlyContinue).Count){throw 'Close existing guest FileSpacer processes before the smoke test.'}
    if(!$Regression){
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($testKey)
    if($key){$key.Dispose();throw 'The guest already has /test preferences; they will not be replaced.'}
    foreach($suffix in @('','-wal','-shm')){if(Test-Path -LiteralPath ($database+$suffix)){throw 'The guest already has a /test database; it will not be replaced.'}}
    }
    $parent=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer')
    $parentExisted=$null-ne$parent;if($parent){$parent.Dispose()}
    $directoryExisted=Test-Path -LiteralPath $normalDirectory
    $initialFingerprint=Get-ProductionFingerprint
    if($Regression){
        $testFingerprint=Get-TestFingerprint
        $packageFiles=Get-Content -LiteralPath (Join-Path $inputRoot 'files.json') -Raw | ConvertFrom-Json
        foreach($entry in $packageFiles){
            $file=[IO.Path]::GetFullPath((Join-Path $inputRoot $entry.Path))
            if(!$file.StartsWith($inputRoot+'\',[StringComparison]::OrdinalIgnoreCase) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash-ne$entry.Sha256){throw 'Prepared package hash or path mismatch.'}
        }
        $selection=Get-Content -LiteralPath (Join-Path $inputRoot 'selection.json') -Raw | ConvertFrom-Json
        if($selection.BuildLocation-ne'Host' -or $selection.HostMachine-ne$manifest.HostMachine){throw 'Unexpected package build identity.'}
        . (Join-Path $inputRoot 'tests\regression\common\Process.ps1')
        $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+(Join-Path $inputRoot 'tests\regression\Run-Regression.ps1')+'" -ProjectRoot "'+$inputRoot+'" -Prepared -RunDirectory "'+(Join-Path $outputRoot 'regression')+'"'
        $job=Start-RecordedProcess -Executable (Join-Path $PSHOME 'powershell.exe') -Arguments $arguments -OutputPath (Join-Path $outputRoot 'runner.stdout.log') -ErrorPath (Join-Path $outputRoot 'runner.stderr.log')
        try{
            $seconds=60+@($selection.Cases).Count*([int]$selection.TimeoutSeconds+15)
            if(!$job.Process.WaitForExit($seconds*1000)){throw 'Guest regression runner timed out; preserve its state for diagnosis.'}
            $report.RunnerExitCode=Complete-RecordedProcess $job
        }finally{$job.Process.Dispose()}
        $report.RegressionSummary=Get-Content -LiteralPath (Join-Path $outputRoot 'regression\summary.json') -Raw | ConvertFrom-Json
        $report.Status=$report.RegressionSummary.Status
        if($report.RunnerExitCode-ne0){
            if($report.Status-eq'PASS'){$report.Status='FAIL'}
            $report.Error='Regression runner returned a nonzero exit code.'
        }
    }else{
    $assembly=Join-Path $inputRoot 'FileSpacerRegression.dll'
    if((Get-FileHash -LiteralPath $assembly -Algorithm SHA256).Hash-ne$manifest.HelperSha256){throw 'Precompiled helper hash mismatch.'}
    if((Get-FileHash -LiteralPath (Join-Path $inputRoot 'FileSpacer.exe') -Algorithm SHA256).Hash-ne$manifest.ExecutableSha256){throw 'Host-built executable hash mismatch.'}
    [void][Reflection.Assembly]::LoadFrom($assembly)
    $activity=New-Object FileSpacerRegression.DesktopActivity
    $statePrepared=$true
    & (Join-Path $inputRoot 'SandboxGesture.ps1') -InputRoot $inputRoot -OutputRoot $outputRoot -TestProfile
    $report.Gesture=Get-Content -LiteralPath (Join-Path $outputRoot 'console-closed.json') -Raw | ConvertFrom-Json
    $report.Status=$report.Gesture.Status
    $report.Error=$report.Gesture.Error
    }
}catch{$report.Error=$_.Exception.Message;$report.ErrorLocation=$_.ScriptStackTrace}
finally {
    if($statePrepared -or $null-ne$testFingerprint){
        try {
            # The companion crash-recovery process may exit slightly later.
            $deadline=[DateTime]::UtcNow.AddSeconds(5)
            while(@(Get-Process FileSpacer -ErrorAction SilentlyContinue).Count -and [DateTime]::UtcNow-lt$deadline){Start-Sleep -Milliseconds 100}
            if(@(Get-Process FileSpacer -ErrorAction SilentlyContinue).Count){throw 'A guest FileSpacer process remains; preserve its /test state for diagnosis.'}
            if(!$Regression){
            [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($testKey,$false)
            foreach($suffix in @('','-wal','-shm')){if(Test-Path -LiteralPath ($database+$suffix)){Remove-Item -LiteralPath ($database+$suffix) -Force}}
            }elseif((Get-TestFingerprint)-ne$testFingerprint){throw 'The regression runner did not restore the preexisting /test registry/database.'}
            if(!$parentExisted){
                $parent=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer')
                try{$empty=$null-ne$parent -and $parent.ValueCount-eq0 -and $parent.SubKeyCount-eq0}finally{if($parent){$parent.Dispose()}}
                if($empty){[Microsoft.Win32.Registry]::CurrentUser.DeleteSubKey('Software\FileSpacer',$false)}
            }
            if(!$directoryExisted -and (Test-Path -LiteralPath $normalDirectory) -and !(Get-ChildItem -LiteralPath $normalDirectory -Force | Select-Object -First 1)){Remove-Item -LiteralPath $normalDirectory}
            $report.StateRestored=$true
        }catch{$report.CleanupErrors+=@($_.Exception.Message)}
    }
    if($activity){try{$activity.Dispose()}catch{$report.CleanupErrors+=@($_.Exception.Message)}}
    if($null-ne$initialFingerprint){
        try{
            $report.ProductionUnchanged=(Get-ProductionFingerprint)-eq$initialFingerprint
            if(!$report.ProductionUnchanged){throw 'Production preferences or database changed during the guest test.'}
        }catch{$report.CleanupErrors+=@($_.Exception.Message)}
    }
    if($transcriptStarted){try{[void](Stop-Transcript)}catch{$report.CleanupErrors+=@($_.Exception.Message)}}
    if($report.CleanupErrors.Count){$report.Status='FAIL'}
    $temporary=Join-Path $outputRoot 'result.tmp'
    $report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $temporary -Encoding UTF8
    Move-Item -LiteralPath $temporary -Destination (Join-Path $outputRoot 'result.json') -Force
}
if($report.Status-ne'PASS'){exit 1}
