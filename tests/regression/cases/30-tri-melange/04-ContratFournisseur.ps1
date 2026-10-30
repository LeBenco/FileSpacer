# Test-Category: Internal
param(
    [string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path,
    [string]$ProviderOverride,
    [switch]$Legacy
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$test=Build-NativeTest -Name MixedProviderContractTests -CaseDirectory $PSScriptRoot -Standard c++14 -Libraries @('Ole32.lib','Shell32.lib','Shlwapi.lib','Uuid.lib')
$folder=Join-Path $RunDirectory 'provider-folder'
[void][IO.Directory]::CreateDirectory($folder)
[IO.File]::WriteAllText((Join-Path $folder 'native.txt'),'Native Shell identity.')
$class='HKCU\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$existed=Test-Path -LiteralPath $providerKey
$backup=Join-Path $RunDirectory 'provider-registration.reg'
if($existed){& reg.exe export $class $backup /y|Out-Host;if($LASTEXITCODE){throw 'Provider registry backup failed.'}}
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=[IO.Directory]::Exists($junction)
$failure=$null
try {
    $provider=if($ProviderOverride){$ProviderOverride}else{Join-Path $ProjectRoot 'build\debug\FileSpacerFolder.dll'}
    $mode=if($Legacy){'legacy'}else{'new'}
    Invoke-TestProgram $test @($provider,$folder,$mode)
} catch {$failure=$_;throw} finally {
    try {
        if(Test-Path -LiteralPath $providerKey){Remove-Item -LiteralPath $providerKey -Recurse -Force}
        if($existed){& reg.exe import $backup|Out-Host;if($LASTEXITCODE){throw 'Provider registry restore failed.'}}
        if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)}
    } catch {if($failure){Write-Error ('Cleanup failure: '+$_) -ErrorAction Continue}else{throw}}
}
