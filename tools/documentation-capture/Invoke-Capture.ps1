# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([Parameter(Mandatory=$true)][string]$ProjectRoot,
      [string[]]$Languages=@(),
      [ValidateSet('customize-title-menu','move-window','resize-window','locked-title')][string[]]$Scenes=@('move-window','resize-window','customize-title-menu','locked-title'),
      [string]$VMName='AgentWin10',[int]$TimeoutSeconds=2400)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
if($Languages.Count-ne1){throw 'Pass exactly one -Languages value matching the signed-in Windows display language.'}
$assets=Join-Path $ProjectRoot 'build\documentation-capture'
$source=Join-Path $assets 'reusable-source'
[void][IO.Directory]::CreateDirectory($source)
$supported=(Get-Content -LiteralPath (Join-Path $PSScriptRoot 'scenes.json') -Raw -Encoding UTF8 | ConvertFrom-Json).PSObject.Properties.Name
foreach($language in $Languages){if($language-notin$supported){throw ('Unknown language: '+$language)}}
if(@($Languages|Select-Object -Unique).Count-ne$Languages.Count){throw 'Duplicate capture language.'}
foreach($name in @('Capture.ps1','Capture.Core.ps1','SceneActions.ps1','CaptureDesktop.cs','scenes.json','wallpaper.jpg')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $source $name) -Force
}
$wallpaperHash='8950e941ecb7805266b09cd472325fd5d849820ae34bec76f2161bfe5f76a714'
if((Get-FileHash -LiteralPath (Join-Path $source 'wallpaper.jpg') -Algorithm SHA256).Hash-ne$wallpaperHash){throw 'Unexpected documentation wallpaper.'}
@{Languages=$Languages;Scenes=$Scenes;WallpaperSha256=$wallpaperHash;SourceApplication=(Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe') -Algorithm SHA256).Hash} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $assets 'reusable-plan.json') -Encoding UTF8
& (Join-Path $PSScriptRoot 'Prepare-Binaries.ps1') -ProjectRoot $ProjectRoot -Languages $Languages
. (Join-Path $ProjectRoot 'tests\regression\common\Prepare-Client.ps1')
$runId=[guid]::NewGuid().ToString('N')
$run=Join-Path $ProjectRoot ('build\regression\vm-'+$runId)
$package=Join-Path $run 'package'
[void][IO.Directory]::CreateDirectory($package)
$relative='99-documentation\01-Capture.ps1'
$selection=@([pscustomobject]@{Relative=$relative;Category='UI';File=(Get-Item -LiteralPath (Join-Path $source 'Capture.ps1'))})
New-ClientPackage -Selected $selection -Destination $package -TimeoutSeconds $TimeoutSeconds
$case=Join-Path $package ('tests\regression\cases\'+$relative)
[void][IO.Directory]::CreateDirectory((Split-Path $case -Parent))
Copy-Item -LiteralPath $selection[0].File.FullName -Destination $case
$destination=Join-Path $package 'build\documentation-capture'
[void][IO.Directory]::CreateDirectory($destination)
foreach($name in @('reusable-source','reusable-plan.json')) {
    $item=Get-Item -LiteralPath (Join-Path $assets $name)
    if($item.PSIsContainer) {
        $folder=Join-Path $destination $name
        [void][IO.Directory]::CreateDirectory($folder)
        Get-ChildItem -LiteralPath $item.FullName | Copy-Item -Destination $folder -Recurse -Force
    } else {Copy-Item -LiteralPath $item.FullName -Destination $destination -Force}
}
$binaries=Join-Path $destination ('reusable-binaries\'+$Languages[0])
[void][IO.Directory]::CreateDirectory($binaries)
foreach($name in @('FileSpacer.exe','FileSpacerFolder.dll')) {
    Copy-Item -LiteralPath (Join-Path $assets ('reusable-binaries\'+$Languages[0]+'\'+$name)) -Destination $binaries
}
if(@($Scenes|Where-Object {$_-ne'locked-title'}).Count){
    $archive=Join-Path $ProjectRoot 'build\tools\ScreenToGif.zip'
    if((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash-ne'42A0B53D67ECAE22698B4881FFB2C9C7A9049663F5FBDD2EDB779E1971000664'){throw 'Use the validated ScreenToGif 2.43.2 portable archive.'}
    Copy-Item -LiteralPath $archive -Destination $destination
}
$files=@(Get-ChildItem -LiteralPath $package -Recurse -File | Where-Object {$_.Name-ne'files.json'} | ForEach-Object {
    @{Path=$_.FullName.Substring($package.Length+1);Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
ConvertTo-Json -InputObject $files -Depth 3 | Set-Content -LiteralPath (Join-Path $package 'files.json') -Encoding UTF8
Write-Host ('Documentation capture package: '+$run)
& (Join-Path $ProjectRoot 'tests\client\Invoke-HyperVRunner.ps1') -ProjectRoot $ProjectRoot -VMName $VMName -RegressionRunId $runId
exit $LASTEXITCODE
