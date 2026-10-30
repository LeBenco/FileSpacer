# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
if(!$env:FS_REGRESSION_PREPARED){throw 'Run this installer UI test with -VM AgentWin10; artifacts and helpers must be prepared on the host.'}
Add-Type -AssemblyName System.Drawing
Add-Type -Path (Join-Path $PSScriptRoot 'WizardArtworkTests.cs') -ReferencedAssemblies System.Drawing
$output=Join-Path $env:FS_REGRESSION_RUN 'wizard-artwork'
$referenceScript=Join-Path $PSScriptRoot 'BicubicReference.ps1'
[void][IO.Directory]::CreateDirectory($output)
foreach($language in @(1033,1036)) {
    $fixture=Join-Path $output ('profile-'+$language)
    [void][IO.Directory]::CreateDirectory($fixture)
    [IO.File]::WriteAllText((Join-Path $fixture 'install-info.ini'),"[Installation]`r`nMode=CurrentUser`r`nLanguage=$language`r`n")
    $bitmap=Join-Path $ProjectRoot ('build\installer-assets\wizard-'+$(if($language-eq1036){'fr'}else{'en'})+'.bmp')
    [FileSpacerInstallerTests.WizardArtwork]::Check(
        (Join-Path $ProjectRoot 'build\FileSpacer-install.exe'),
        ('/CurrentUser /D='+$fixture),$bitmap,$referenceScript,$output,('setup-'+$language),$language)
    [FileSpacerInstallerTests.WizardArtwork]::Check(
        (Join-Path $ProjectRoot 'build\installer-tests\artwork\ArtworkUninstall.exe'),
        ('_?='+$fixture),$bitmap,$referenceScript,$output,('uninstall-'+$language),$language)
}
Write-Host 'PASS: real setup and uninstaller UI, EN/FR, high-quality pixels, cancellation without installation.'
