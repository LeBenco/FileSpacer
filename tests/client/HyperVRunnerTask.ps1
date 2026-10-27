$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$config=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runner.json') -Raw | ConvertFrom-Json
$record=[ordered]@{RunId=$null;State='Complete';Status='BLOCKED';ControllerProcessId=$PID;RunDirectory=$null;Error=$null;StartedUtc=[DateTime]::UtcNow.ToString('o')}
$credential=$null
try {
    $request=Get-Content -LiteralPath $config.RequestFile -Raw | ConvertFrom-Json
    if($request.RunId -notmatch '^[0-9a-f]{32}$'){throw 'Invalid test run identifier.'}
    $record.RunId=$request.RunId
    $project=[IO.Path]::GetFullPath($request.ProjectRoot).TrimEnd('\')
    $workspace=[IO.Path]::GetFullPath($config.WorkspaceRoot).TrimEnd('\')
    if($project-ne$workspace -and !$project.StartsWith($workspace+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'The requested checkout is outside the configured workspace.'}
    if($request.PSObject.Properties['Mode'] -and $request.Mode -notin @('Smoke','Regression')){throw 'Unknown test request mode.'}
    $regression=$request.PSObject.Properties['Mode'] -and $request.Mode -eq 'Regression'
    $relative=if($regression){'build\regression\vm-'}else{'build\client-smoke\hyperv-smoke-'}
    $run=Join-Path $project ($relative+$request.RunId)
    if(!(Test-Path -LiteralPath $run -PathType Container)){throw 'The caller must prepare the test output directory.'}
    $record.RunDirectory=$run;$record.State='Running'
    $record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'last-run.json') -Encoding UTF8
    $credential=Import-Clixml -LiteralPath (Join-Path $PSScriptRoot 'guest-credential.xml')
    if($credential -isnot [System.Management.Automation.PSCredential]){throw 'Stored guest credential has an unexpected type.'}
    # Only protected host controller scripts run elevated. Workspace binaries
    # and guest scripts are transferred to the VM; none runs on the host.
    & (Join-Path $PSScriptRoot 'Connect-HyperV.ps1') -VMName $config.VMName -Elevated -RunSmoke:(!$regression) -RunRegression:$regression -GuestCredential $credential -ProjectRoot $project -ReportDirectory $run
    $result=Get-Content -LiteralPath (Join-Path $run 'connection.json') -Raw | ConvertFrom-Json
    $record.Status=$result.Status;$record.Error=$result.Error
}catch{$record.Error=$_.Exception.Message}
finally {
    if($credential){$credential.Password.Dispose()}
    $record.State='Complete'
    $temporary=Join-Path $PSScriptRoot 'last-run.tmp'
    $record | ConvertTo-Json | Set-Content -LiteralPath $temporary -Encoding UTF8
    Move-Item -LiteralPath $temporary -Destination (Join-Path $PSScriptRoot 'last-run.json') -Force
}
if($record.Status-ne'PASS'){exit 1}
