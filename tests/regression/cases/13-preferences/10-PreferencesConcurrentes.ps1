# Test-Category: UI
param([string]$ProjectRoot)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
function Sheets([uint32[]]$ProcessIds) {
    $sheets=@()
    foreach($processId in $ProcessIds) {
        $sheets+=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId)|Where-Object {
            @([FileSpacerRegression.Native]::Children($_,'SysTabControl32')).Count -eq 1
        })
    }
    return $sheets
}
function Request([IntPtr]$Window) {
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($Window,0x111,[IntPtr](Get-UiId 'IDM_SETTINGS'),[IntPtr]::Zero)) 'Settings post failed.'
}
function Cancel-Sheets([uint32[]]$ProcessIds) {
    foreach($sheet in @(Sheets $ProcessIds)) {
        [void][FileSpacerRegression.Native]::PostMessage($sheet,0x471,[IntPtr]5,[IntPtr]::Zero)
    }
    Wait-Ui { @(Sheets $ProcessIds).Count -eq 0 } 'all Preferences sheets cancelled'
}
$failure=$null;$gate=$null;$holder=$null;$observer=$null
try {
    Initialize-Ui 'settings-concurrent'
    $windows=@();$processIds=@()
    foreach($name in @('A','B','C')) {
        $folder=Join-Path $script:UiRoot $name
        [void][IO.Directory]::CreateDirectory($folder)
        [IO.File]::WriteAllText((Join-Path $folder 'item.txt'),'fixture')
        $window=Open-UiFolder $folder 1
        $windows+=@($window)
        $processId=[uint32]0
        [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$processId)
        $processIds+=@($processId)
    }
    Assert-Ui (@($processIds|Select-Object -Unique).Count -eq 3) 'Three separate processes required.'
    foreach($round in 1..10) {
        foreach($window in $windows[0..1]) {Request $window}
        Wait-Ui { @(Sheets $processIds).Count -gt 0 } 'concurrent Preferences request'
        Start-Sleep -Milliseconds 300
        $count=@(Sheets $processIds).Count
        Write-Host ('ROUND '+$round+': '+$count+' sheet(s)')
        Assert-Ui ($count -eq 1) 'Concurrent requests opened duplicate Preferences sheets.'
        Cancel-Sheets $processIds
    }
    Write-Host 'PASS: ten simultaneous request pairs produce one real property sheet.'

    $gate=New-Object Threading.Mutex($false,'Local\FileSpacer.Test.Preferences')
    Assert-Ui ($gate.WaitOne(5000)) 'Cannot reserve the initialization/closing gate.'
    Request $windows[0]
    Start-Sleep -Milliseconds 300
    Assert-Ui (@(Sheets $processIds).Count -eq 0) 'Preferences opened while initialization was reserved.'
    $gate.ReleaseMutex();$gate.Dispose();$gate=$null
    Wait-Ui { @(Sheets $processIds).Count -eq 1 } 'queued request after reservation release'
    Cancel-Sheets $processIds
    Write-Host 'PASS: a request queued before a dialog exists opens after the gate is released.'

    $gate=New-Object Threading.Mutex($false,'Local\FileSpacer.Test.Preferences')
    Assert-Ui ($gate.WaitOne(5000)) 'Cannot reserve the owner-close gate.'
    Request $windows[2]
    Start-Sleep -Milliseconds 300
    Close-UiFolder $windows[2]
    $gate.ReleaseMutex();$gate.Dispose();$gate=$null
    Request $windows[0]
    Wait-Ui { @(Sheets $processIds).Count -eq 1 } 'remaining owner can open Preferences'
    Assert-Ui (@(Sheets @($processIds[2])).Count -eq 0) 'Closed owner acquired a late dialog.'
    Cancel-Sheets $processIds
    Write-Host 'PASS: closing a waiting owner cancels the request without blocking its UI.'

    $ready=Join-Path $script:UiRoot 'mutex-ready';$finish=Join-Path $script:UiRoot 'mutex-finish'
    $helper=Join-Path $PSScriptRoot 'PreferencesMutexHolder.ps1'
    $arguments='-NoProfile -ExecutionPolicy Bypass -File "'+$helper+'" -Ready "'+$ready+'" -Finish "'+$finish+'"'
    $holder=Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
    Wait-Ui { Test-Path -LiteralPath $ready } 'external mutex owner ready'
    $observer=[Threading.Mutex]::OpenExisting('Local\FileSpacer.Test.Preferences')
    Request $windows[0]
    Start-Sleep -Milliseconds 300
    Assert-Ui (@(Sheets $processIds).Count -eq 0) 'Preferences bypassed a live mutex owner.'
    [IO.File]::WriteAllText($finish,'exit')
    Assert-Ui ($holder.WaitForExit(5000)) 'Mutex holder failed to exit.'
    Assert-Ui ($holder.ExitCode -eq 0) 'Mutex holder failed.'
    Wait-Ui { @(Sheets $processIds).Count -eq 1 } 'abandoned reservation recovered'
    Cancel-Sheets $processIds
    Write-Host 'PASS: abandoned mutex ownership is recovered by the queued request.'
} catch {$failure=$_;throw} finally {
    if($gate){try{$gate.ReleaseMutex()}catch{};$gate.Dispose()}
    if($observer){$observer.Dispose()}
    if($holder){if(!$holder.HasExited -and $finish){[IO.File]::WriteAllText($finish,'exit');[void]$holder.WaitForExit(5000)};$holder.Dispose()}
    Stop-Ui -Failure $failure
}
