# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$legacy=Join-Path $ProjectRoot 'build\regression\legacy-provider-v1.2.2\FileSpacerFolder.dll'
if(!$env:FS_REGRESSION_PREPARED){$legacy=& (Join-Path $PSScriptRoot '..\..\common\Prepare-LegacyMixedProvider.ps1') -ProjectRoot $ProjectRoot}
Assert-Ui (Test-Path -LiteralPath $legacy -PathType Leaf) 'The host-prepared v1.2.2 provider is missing.'
$class='HKCU\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$backup=Join-Path $RunDirectory 'fallback-provider-registration.reg'
if($registrationExisted){& reg.exe export $class $backup /y|Out-Host;if($LASTEXITCODE){throw 'Provider registry backup failed.'}}
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=[IO.Directory]::Exists($junction)
$failure=$null
$cleanupErrors=New-Object Collections.Generic.List[string]
try {
    # Prove that the DLL really implements the old IID/vtable and rejects the
    # current one; an absent DLL or a failed factory is not this regression.
    & (Join-Path $PSScriptRoot '04-ContratFournisseur.ps1') -ProjectRoot $ProjectRoot -ProviderOverride $legacy -Legacy
    # Keep executable + fixture paths short enough for Shell relaunch properties.
    $application=Join-Path $ProjectRoot ('build\ip-'+[guid]::NewGuid().ToString('N').Substring(0,8))
    [void][IO.Directory]::CreateDirectory($application)
    Copy-Item -LiteralPath $script:UiExecutable -Destination (Join-Path $application 'FileSpacer.exe')
    $script:UiExecutable=Join-Path $application 'FileSpacer.exe'
    $provider=Join-Path $application 'FileSpacerFolder.dll'
    Copy-Item -LiteralPath $legacy -Destination $provider
    [void](New-Item -Path ($providerKey+'\InprocServer32') -Force)
    Set-Item -LiteralPath ($providerKey+'\InprocServer32') -Value $provider
    New-ItemProperty -LiteralPath ($providerKey+'\InprocServer32') -Name ThreadingModel -Value Apartment -PropertyType String -Force|Out-Null
    Initialize-Ui 'fallback' -FixtureParent $application
    Set-UiPreference 'MixedFolderSorting' 1
    Set-UiPreference 'KeepSourceWindowOpen' 1
    $outer=Join-Path $script:UiRoot 'level-3'
    $middle=Join-Path $outer 'b-level-2'
    $inner=Join-Path $middle 'b-level-1'
    [void][IO.Directory]::CreateDirectory((Join-Path $inner 'b-folder'))
    $folders=@($inner,$middle,$outer)
    $children=@('b-folder','b-level-1','b-level-2')
    foreach($folder in $folders) {
        foreach($name in @('a-file.txt','c-file.txt')){[IO.File]::WriteAllText((Join-Path $folder $name),'Native fallback fixture.')}
    }
    $windows=@();$applicationId=[uint32]0
    foreach($index in 1..3) {
        $folder=$folders[$index-1]
        if($index -eq 1){$window=Open-UiFolder $folder 3}
        else {
            # Dispatch FileSpacer's own Up command: a fresh executable launch
            # creates another process and would not test the process-wide latch.
            Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_PREV_WINDOW'),[IntPtr]::Zero)) 'Cannot dispatch parent navigation.'
            $window=Track-UiFolder $folder
        }
        $windows+=@($window)
        $windowProcessId=[uint32]0
        [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($window,[ref]$windowProcessId)
        if($index -eq 1){$applicationId=$windowProcessId}
        Assert-Ui ($windowProcessId -eq $applicationId) 'A subsequent navigation used another application process.'
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true)}
        Wait-Ui {
            Use-UiView $window {param($view)
                $view.ContentVisible -and $view.FolderClassId -ne [guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604' -and
                    (($view.OrderedNames() -join '|') -ceq ($children[$index-1]+'|a-file.txt|c-file.txt'))
            }
        } 'visible native folders-first view after incompatible-provider navigation' 300
        Assert-Ui ((Get-UiPreference 'MixedFolderSorting') -eq 1) 'Compatibility fallback changed the mixed-sort preference.'
        Assert-Ui (@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$applicationId)).Count -eq 0) 'Compatibility fallback displayed an error dialog.'
        if($index -eq 1) {
            $registered=(Get-Item -LiteralPath ($providerKey+'\InprocServer32')).GetValue('')
            Assert-Ui ($registered -ceq $provider) 'The first navigation did not register the legacy DLL.'
            Assert-Ui ([IO.Directory]::Exists($junction)) 'The legacy provider factory did not create its test junction.'
            # The real legacy factory writes these values at every call. Poison
            # both: any later attempt changes them even if COM caches the DLL.
            Set-Item -LiteralPath ($providerKey+'\InprocServer32') -Value 'fallback-must-not-retry.dll'
            Set-ItemProperty -LiteralPath ($providerKey+'\InprocServer32') -Name ThreadingModel -Value 'NoRetry'
        } else {
            $key=Get-Item -LiteralPath ($providerKey+'\InprocServer32')
            Assert-Ui ($key.GetValue('') -ceq 'fallback-must-not-retry.dll' -and $key.GetValue('ThreadingModel') -ceq 'NoRetry') 'The application retried the provider after incompatibility.'
        }
    }
    foreach($window in $windows){Close-UiFolder $window}
} catch {$failure=$_;Write-Host $_.Exception.ToString()} finally {
    try {Stop-Ui -Failure $failure}catch{$cleanupErrors.Add($_.Exception.Message)}
    try {
        if(Test-Path -LiteralPath $providerKey){Remove-Item -LiteralPath $providerKey -Recurse -Force}
        if($registrationExisted){& reg.exe import $backup|Out-Host;if($LASTEXITCODE){throw 'Provider registry restore failed.'}}
        if($registrationExisted) {
            $restored=Join-Path $RunDirectory 'fallback-provider-restored.reg'
            & reg.exe export $class $restored /y|Out-Host
            if($LASTEXITCODE -or [IO.File]::ReadAllText($restored) -cne [IO.File]::ReadAllText($backup)){throw 'The original provider registration was not restored exactly.'}
        }
        if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)}
        Assert-Ui ((Test-Path -LiteralPath $providerKey) -eq $registrationExisted) 'Provider registration was not restored.'
        Assert-Ui ([IO.Directory]::Exists($junction) -eq $junctionExisted) 'The test junction was not cleaned up.'
    } catch {$cleanupErrors.Add($_.Exception.Message)}
}
if($failure){foreach($errorMessage in $cleanupErrors){Write-Host ('Cleanup failure: '+$errorMessage)};throw $failure}
if($cleanupErrors.Count){throw ($cleanupErrors -join '; ')}
# Stop-Ui collected stdout only after the actual application exited normally.
$output=Get-Content -LiteralPath (Join-Path $RunDirectory ('application-'+$applicationId+'.stdout.log')) -Raw
Assert-Ui ([regex]::Matches($output,'Mixed provider unavailable or incompatible: HRESULT=0x80004002; using native view').Count -eq 1) 'Expected exactly one E_NOINTERFACE fallback from the real application.'
Assert-Ui ([regex]::Matches($output,'Mixed provider unavailable or incompatible:').Count -eq 1) 'A later navigation attempted the provider again.'
Assert-Ui ($output -notmatch '(?m)^Error (?:0x|\d)') 'The application logged an unexpected error during native fallback.'
Write-Host 'PASS: the real v1.2.2 DLL is rejected once; three same-process navigations show the native view without error, retain the preference, never retry the factory, and restore registration/junction state.'
