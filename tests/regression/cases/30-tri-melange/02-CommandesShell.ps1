# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
$parentRun=$env:FS_REGRESSION_RUN
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$junctionExisted=[IO.Directory]::Exists($junction)
$scenarios=@(
    '..\19-commandes-shell\01-CreationRenommageSuppression.ps1',
    '..\19-commandes-shell\02-CopierCouperColler.ps1',
    '..\19-commandes-shell\03-GlisserDeposer.ps1',
    '..\19-commandes-shell\04-MenusContextuels.ps1',
    '..\19-commandes-shell\05-RenommageDossierOuvert.ps1',
    '..\19-commandes-shell\06-UnicodeEtAnnulation.ps1'
)
try {
    $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('Software\FileSpacer\Test')
    try {$key.SetValue('MixedFolderSorting',1,[Microsoft.Win32.RegistryValueKind]::DWord)}finally{$key.Dispose()}
    foreach($scenario in $scenarios) {
        $env:FS_REGRESSION_RUN=Join-Path $parentRun ([IO.Path]::GetFileNameWithoutExtension($scenario))
        [void][IO.Directory]::CreateDirectory($env:FS_REGRESSION_RUN)
        Write-Host ('MIXED PROVIDER: '+$scenario)
        & (Join-Path $PSScriptRoot $scenario) -ProjectRoot $ProjectRoot
    }
    Write-Host 'PASS: the existing native Shell command scenarios also pass with mixed sorting enabled.'
}finally {
    $env:FS_REGRESSION_RUN=$parentRun
    if(!$registrationExisted -and (Test-Path -LiteralPath $providerKey)){Remove-Item -LiteralPath $providerKey -Recurse -Force}
    if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)}
}
