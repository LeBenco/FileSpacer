# Test-Category: UI
param([string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\StorageSnapshot.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryUi.cs')
Add-Type -Path (Join-Path $PSScriptRoot 'StorageRecoveryInput.cs') -ReferencedAssemblies ([FileSpacerRegression.Native].Assembly.Location)
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName WindowsBase
Add-Type -AssemblyName System.Drawing
$titleReferences=@([System.Windows.Automation.AutomationElement].Assembly.Location,[System.Windows.Automation.ControlType].Assembly.Location,[System.Windows.Rect].Assembly.Location,[System.Drawing.Bitmap].Assembly.Location)
Add-Type -Path (Join-Path $PSScriptRoot '..\33-menu-titre\TitleMenuUi.cs') -ReferencedAssemblies $titleReferences
$helper=Build-NativeTest -Name StorageMutexRegression -CaseDirectory $PSScriptRoot -ProductionSources @('FolderStateStore.cpp') -Sqlite -Storage -Standard c++14
$probe=Build-NativeTest -Name FolderIdentityProbe -CaseDirectory $PSScriptRoot -ProductionSources @('FolderIdentity.cpp')
$failure=$null
try {
    Initialize-Ui -Name 'SQLiteRestoreFailure' -FixtureParent $RunDirectory
    $folder=Join-Path $script:UiRoot 'known-folder'
    [void][IO.Directory]::CreateDirectory($folder)
    foreach($name in @('alpha','beta')){[IO.File]::WriteAllText((Join-Path $folder $name),'fixture')}
    $identity=@(& $probe $folder)
    Assert-Ui ($LASTEXITCODE-eq0 -and $identity.Count-eq2) 'Fixture identity failed.'
    $key=$identity[0]
    $database=Join-Path $env:LOCALAPPDATA 'FileSpacer\folder-state-test.sqlite3'
    Invoke-TestProgram $helper @('--seed-product',$database,$key)
    $first=Open-UiFolder $folder 2
    Use-UiView $first {param($view) $view.SetViewModeAndIconSize(1,64)}
    Wait-UiViewMode $first 1 64
    Use-UiView $first {param($view) $view.AutoArrange=$false}
    Wait-Ui {(Use-UiView $first {param($view) $view.Mode})-eq1} 'reference icon mode' 300
    Close-UiFolder $first
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count-eq0} 'reference process exit'
    Invoke-TestProgram $helper @('--invalidate-product-icons',$database,$key)
    # This goes through the shipped restore path with real malformed data,
    # not a private barrier or an injected success/abort in the product.
    $application=Start-TestApplication -Executable $script:UiExecutable -Arguments ('/test "'+$folder+'"') -OutputDirectory $RunDirectory
    [void]$script:UiProcesses.Add($application)
    $script:restoreDialog=[IntPtr]::Zero
    Wait-Ui {
        $dialogs=@([MutexRecoveryNative]::Find($application.Id,'#32770'))
        if($dialogs.Count-eq1){$script:restoreDialog=$dialogs[0];return $true}
        return $false
    } 'restore error warning'
    $labels=@([MutexRecoveryNative]::Messages($script:UiExecutable,(Get-UiId 'IDS_ICON_POSITION_ERROR')))
    $texts=@([FileSpacerRegression.Native]::Children($script:restoreDialog,'Static')|ForEach-Object{[FileSpacerRegression.Native]::Text($_)})
    Assert-Ui (@($texts|Where-Object{$labels-contains$_}).Count-eq1) 'Restore warning does not explain preservation and possible unsaved layout.'
    [void][FileSpacerRegression.Native]::PostMessage($script:restoreDialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Wait-Ui {![FileSpacerRegression.Native]::IsWindow($script:restoreDialog)} 'warning dismissed'
    $frames=@([MutexRecoveryNative]::Find($application.Id,'FileSpacer Item Window'))
    Assert-Ui ($frames.Count-eq1) 'Failed restoration refused the file window.'
    $window=$frames[0];[void]$script:UiWindows.Add($window)
    Wait-Ui {
        $view=Get-UiView $window
        if($null-eq$view){return $false}
        try{return $view.Count-eq2}finally{$view.Dispose()}
    } 'usable failed-restoration view' 300
    $before=Use-UiView $window {param($view) $view.Positions()}
    Activate-Ui $window
    $point=[TitleMenuUi]::CatalogPoint($window,'alpha')
    $inputTarget=[StorageRecoveryInput]::PhysicalTarget($window,$point[0],$point[1])
    [TitleMenuUi]::Drag($inputTarget,$point[0],$point[1],($point[0]+100),($point[1]+70),$false)
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()})-ne$before} 'native drag remains available after failed restoration' 300
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($window)) 'Failed observation left the file window disabled.'
    Assert-Ui ((Use-UiView $window {param($view) [FileSpacerRegression.Native]::IsWindowEnabled($view.Window)})) 'Failed observation left the view disabled.'
    Close-UiFolder $window
    Wait-Ui {@($script:UiProcesses|Where-Object{!$_.HasExited}).Count-eq0} 'final process normal exit'
    foreach($finished in $script:UiProcesses){Assert-Ui ((Complete-RecordedProcess $finished.RegressionOutput)-eq0) 'Product exited abnormally.'}
    $snapshot=Join-Path $RunDirectory 'post-close-sql'
    Copy-TestDatabaseSnapshot $database $snapshot
    $analysis=Join-Path $RunDirectory 'post-close-sql-check'
    Copy-TestDatabaseSnapshot (Join-Path $snapshot 'folder-state-test.sqlite3') $analysis
    $proof=@(& $helper '--verify-invalid-product' (Join-Path $analysis 'folder-state-test.sqlite3') $key)
    Assert-Ui ($LASTEXITCODE-eq0) 'Closing after real restore/merge failure overwrote the reference or history.'
    $proof|Set-Content -LiteralPath (Join-Path $RunDirectory 'post-close-verification.json') -Encoding UTF8
    Write-Host 'PASS real malformed restore, explicit warning, usable native view, failed observation and post-close reference/history preservation.'
} catch {$failure=$_;throw}
finally {
    foreach($process in @($script:UiProcesses|Where-Object{!$_.HasExited})){
        foreach($dialog in @([MutexRecoveryNative]::Find($process.Id,'#32770'))){[void][MutexRecoveryNative]::PostMessage($dialog,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
    }
    Stop-Ui $failure
}
