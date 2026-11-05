# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
$ErrorActionPreference = 'Stop'
$keys = @(Get-UninstallRegistryKey -SoftwareName 'FileSpacer' | Where-Object {
    $_.PSPath -match 'HKEY_LOCAL_MACHINE' -and $_.PSChildName -eq 'FileSpacer'
})
if ($keys.Count -eq 0) {
    Write-Warning 'No machine-wide FileSpacer installation remains.'
    return
}
if ($keys.Count -ne 1 -or !$keys[0].InstallLocation) {
    throw 'Cannot identify a unique machine-wide FileSpacer installation.'
}
$uninstaller = Join-Path $keys[0].InstallLocation 'uninstall.exe'
if (!(Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
    throw "FileSpacer uninstaller is missing: $uninstaller"
}
Uninstall-ChocolateyPackage -PackageName $env:ChocolateyPackageName -FileType 'exe' `
    -File $uninstaller -SilentArgs '/S /AllUsers' -ValidExitCodes @(0)
