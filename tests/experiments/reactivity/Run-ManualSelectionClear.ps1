param([switch]$Elevated)
$ErrorActionPreference='Stop'
if($env:COMPUTERNAME-ne'AGENTWIN10'){throw 'Ce paquet doit etre lance dans AgentWin10.'}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    if($Elevated){throw 'Le jeton administrateur requis pour ETW est absent.'}
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Elevated'
    $child=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -Verb RunAs -WindowStyle Hidden -PassThru
    try{$null=$child.Handle;$child.WaitForExit();if($null-eq$child.ExitCode){throw 'Code de sortie du processus eleve indisponible.'};exit $child.ExitCode}finally{$child.Dispose()}
}
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'package.json') -Raw|ConvertFrom-Json
$binary=Join-Path $PSScriptRoot 'SelectionClearTests.exe'
if((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash-ne$manifest.ExecutableSha256){throw 'Le binaire differe du paquet prepare.'}
$mutex=New-Object Threading.Mutex($false,'Local\FileSpacer-AgentWin10-Smoke')
$locked=$false;$started=$false;$fixture=$null;$failure=$null;$code=1
$run=Join-Path $PSScriptRoot ('results-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8))
[void][IO.Directory]::CreateDirectory($run)
$instance='FileSpacerManualSelection'+[guid]::NewGuid().ToString('N')
$report=[ordered]@{Status='FAIL';Prototype='Classic Shell clear plus aggregate CDefView notification';CodeCommit=$manifest.CodeCommit;ExecutableSha256=$manifest.ExecutableSha256;NativeExitCode=$null;Trace='NOT_STARTED';Error=$null;CleanupErrors=@();Results=$run}
Start-Transcript -LiteralPath (Join-Path $run 'runner.log')
try {
    try{$locked=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
    if(!$locked){throw 'Un autre test utilise AgentWin10. Relance ce script apres sa fin.'}
    if(@(Get-ScheduledTask -TaskName 'FileSpacer-Smoke-*' -ErrorAction SilentlyContinue|Where-Object State -eq 'Running').Count){throw 'Un passage automatise utilise la VM. Attends sa fin avant de lancer ce test manuel.'}
    if(@(Get-Process FileSpacer -ErrorAction SilentlyContinue).Count){throw 'Ferme FileSpacer dans la VM avant ce test.'}
    Write-Host 'Six vues, 5000 fichiers de test, trois variantes. Ne manipule pas la VM pendant le test.'
    Write-Host 'Ce prototype utilise des interfaces internes limitees a une version exacte de Windows.'
    $profile=Join-Path $run 'SelectionClearTrace.wprp'
    @'
<?xml version="1.0" encoding="utf-8"?>
<WindowsPerformanceRecorder Version="1.0" Author="FileSpacer investigation">
 <Profiles>
  <EventCollector Id="SelectionCollector" Name="FileSpacer manual selection"><BufferSize Value="1024"/><Buffers Value="64"/></EventCollector>
  <EventProvider Id="Shell" Name="Microsoft-Windows-Shell-Core" Level="5" Stack="true" Strict="true" ProcessExeFilter="SelectionClearTests.exe"><Keywords><Keyword Value="0x10000"/></Keywords></EventProvider>
  <Profile Id="ExplorerSelection.Verbose.Memory" Name="ExplorerSelection" Description="Selection clear prototype" LoggingMode="Memory" DetailLevel="Verbose"><Collectors><EventCollectorId Value="SelectionCollector"><EventProviders><EventProviderId Value="Shell"/></EventProviders></EventCollectorId></Collectors></Profile>
 </Profiles>
</WindowsPerformanceRecorder>
'@ | Set-Content -LiteralPath $profile -Encoding UTF8
    & wpr.exe -start ($profile+'!ExplorerSelection') -instancename $instance
    if($LASTEXITCODE-eq0){$started=$true;$report.Trace='RECORDING'}else{$report.Trace='UNAVAILABLE';Write-Host 'ETW indisponible : les tests fonctionnels et mesures API continuent.'}
    . (Join-Path $PSScriptRoot 'Fixture.ps1')
    $fixture=New-ReactivityFixture $PSScriptRoot
    $job=Start-Process -FilePath $binary -ArgumentList ('"'+$run+'" "'+$fixture+'"') -WorkingDirectory $PSScriptRoot -PassThru -WindowStyle Hidden -RedirectStandardOutput (Join-Path $run 'native.stdout.log') -RedirectStandardError (Join-Path $run 'native.stderr.log')
    try {
        $null=$job.Handle
        if(!$job.WaitForExit(600000)){$job.Kill();$job.WaitForExit();throw 'Le prototype a depasse dix minutes ; seul le processus de test a ete termine.'}
        $job.WaitForExit();$report.NativeExitCode=$job.ExitCode
        if($job.ExitCode-ne0){throw ('Echec du prototype, code '+$job.ExitCode+'. Consulter native.stderr.log.')}
    } finally {$job.Dispose()}
    $rows=@(Import-Csv -LiteralPath (Join-Path $run 'clear-results.csv'))
    if($rows.Count-ne54-or@($rows|Where-Object {$_.arm-eq'candidate'-and$_.private_applied-ne'1'}).Count){throw 'La couverture du prototype est incomplete.'}
    $report.Status=if($report.Trace-eq'UNAVAILABLE'){'REVIEW'}else{'PASS'}
    $code=0
} catch {$failure=$_.Exception.Message;$report.Error=$failure;Write-Host $failure -ForegroundColor Red}
finally {
    if($started){
        & wpr.exe -status collectors -instancename $instance
        & wpr.exe -stop (Join-Path $run 'selection.etl') -instancename $instance
        if($LASTEXITCODE-ne0){$report.CleanupErrors+=('Echec sauvegarde ETW : '+$LASTEXITCODE);& wpr.exe -cancel -instancename $instance;$report.Trace='SAVE_FAILED'}else{$report.Trace='SAVED'}
    }
    if($fixture){try{Remove-ReactivityFixture $PSScriptRoot $fixture}catch{$report.CleanupErrors+=$_.Exception.Message}}
    if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()
    if($report.CleanupErrors.Count){$report.Status='FAIL';$code=1}
    $report|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $run 'result.json') -Encoding UTF8
    Stop-Transcript
}
try{Compress-Archive -LiteralPath $run -DestinationPath ($run+'.zip') -CompressionLevel Optimal}catch{Write-Host ('Archive non creee : '+$_.Exception.Message)}
Write-Host ('Resultat : '+$report.Status+' ; dossier : '+$run)
exit $code
