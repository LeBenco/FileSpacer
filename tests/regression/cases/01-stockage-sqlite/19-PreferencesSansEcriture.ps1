# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$helper=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$failure=$null;$holder=$null;$ready=$null;$release=$null
try {
    Initialize-Ui 'preferences-without-storage' -FixtureParent $RunDirectory
    $folder=Join-Path $script:UiRoot 'folder'
    [void][IO.Directory]::CreateDirectory($folder)
    $window=Open-UiFolder $folder 0
    $database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    $sheet=Show-UiSettings $window
    $page=Set-UiPage 0
    $expectedLimit=[int][FileSpacerRegression.Native]::Text([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_SEARCH_HISTORY_LIMIT')))
    $prefix='Local\FileSpacer.NoHistoryChange.'+[Guid]::NewGuid().ToString('N')
    $ready=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.ready'))
    $release=New-Object Threading.EventWaitHandle($false,[Threading.EventResetMode]::ManualReset,($prefix+'.release'))
    $holder=Start-RecordedProcess -Executable $helper -Arguments ('--child "'+$database+'" "'+$prefix+'" hold') -OutputPath (Join-Path $RunDirectory 'holder.out') -ErrorPath (Join-Path $RunDirectory 'holder.err')
    Assert-Ui ($ready.WaitOne(15000)) 'Controlled database mutex holder did not start.'
    Set-UiCheck $page 'IDC_KEEP_SOURCE_WINDOW_OPEN' $false
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'KeepSourceWindowOpen') -eq 0 } 'preferences applied despite unavailable storage and unchanged history limit' 0 8
    Assert-Ui ((Get-UiPreference 'SearchHistoryLimit') -eq $expectedLimit) 'Apply did not persist the explicit history limit.'
    $processId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$processId)
    Assert-Ui (@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$processId) | Where-Object {$_ -ne $sheet}).Count -eq 0) 'Unchanged history caused a storage error during Apply.'
    [void]$release.Set()
    Assert-Ui ($holder.Process.WaitForExit(15000)) 'Controlled holder did not exit.'
    Assert-Ui ((Complete-RecordedProcess $holder) -eq 0) 'Controlled holder failed.'
    Press-UiSettings 5
    Write-Host 'PASS: real PropSheet Apply succeeds without writing an unchanged search history, despite an unavailable storage mutex.'
} catch {$failure=$_;throw}
finally {
    $cleanupFailure=$null
    try {
        if($release){[void]$release.Set()}
        if($holder){
            if(!$holder.Process.WaitForExit(20000)){throw 'Holder cleanup failed.'}
            if((Complete-RecordedProcess $holder) -ne 0){throw 'Holder exited with an error.'}
        }
    } catch {$cleanupFailure=$_}
    finally {
        if($holder){$holder.Process.Dispose()}
        if($ready){$ready.Dispose()};if($release){$release.Dispose()}
        Stop-Ui $(if($failure){$failure}else{$cleanupFailure})
    }
    if($cleanupFailure){
        if($failure){throw ('Initial failure: '+$failure.Exception.Message+'; cleanup failure: '+$cleanupFailure.Exception.Message)}
        throw $cleanupFailure
    }
}
