# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Generate the production uninstaller without running any installation section.
param([string]$MakeNSIS="${env:ProgramFiles(x86)}\NSIS\makensis.exe")
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$output=Join-Path $root 'build\installer-tests\artwork'
[void][IO.Directory]::CreateDirectory($output)
$source=[IO.File]::ReadAllText((Join-Path $root 'installer\install.nsi'))
$builder=Join-Path $output 'WriteArtworkUninstaller.exe'
$uninstaller=Join-Path $output 'ArtworkUninstall.exe'
$source=$source.Replace('OutFile "..\build\FileSpacer-install.exe"','OutFile "'+$builder+'"')
$source=$source.Replace('SetCompressor /SOLID lzma','SetCompress off')
$initialization=[regex]::new('Function \.onInit\r?\n.*?FunctionEnd',[Text.RegularExpressions.RegexOptions]::Singleline)
if($initialization.Matches($source).Count-ne1){throw 'Expected exactly one production installer initialization.'}
$replacement="Function .onInit`r`n    WriteUninstaller `"$uninstaller`"`r`n    SetErrorLevel 0`r`n    Quit`r`nFunctionEnd"
$source=$initialization.Replace($source,[Text.RegularExpressions.MatchEvaluator]{param($match) $replacement})
$source='!cd "'+(Join-Path $root 'installer')+'"'+"`r`n"+$source
$fixture=Join-Path $output 'WriteArtworkUninstaller.nsi'
[IO.File]::WriteAllText($fixture,$source,[Text.UTF8Encoding]::new($true))
& $MakeNSIS /NOCONFIG /WX /V2 $fixture
if($LASTEXITCODE-ne0){throw 'Artwork uninstaller fixture compilation failed.'}
$process=Start-Process -FilePath $builder -WindowStyle Hidden -PassThru
try {
    if(!$process.WaitForExit(30000)){throw 'Artwork uninstaller generation timed out.'}
    if($process.ExitCode-ne0 -or !(Test-Path -LiteralPath $uninstaller)){throw 'Artwork uninstaller was not generated.'}
} finally { $process.Dispose() }
Write-Host ('Prepared production uninstaller UI without installation: '+$uninstaller)
