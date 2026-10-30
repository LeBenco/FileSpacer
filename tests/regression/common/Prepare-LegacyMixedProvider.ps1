# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Host only: build the unmodified published provider with the normal x64 MSVC tools.
param([Parameter(Mandatory=$true)][string]$ProjectRoot)
$ErrorActionPreference='Stop'
if($env:FS_REGRESSION_PREPARED){throw 'The legacy provider must be prepared on the host.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)){throw 'Run tests.cmd or use an x64 MSVC developer prompt.'}
$output=Join-Path $ProjectRoot 'build\regression\legacy-provider-v1.2.2'
[void][IO.Directory]::CreateDirectory($output)
$archive=Join-Path $output 'source.zip'
$bash=Join-Path $env:ProgramFiles 'Git\bin\bash.exe'
$extract=Join-Path $output 'extract.sh'
[IO.File]::WriteAllText($extract,'git archive --format=zip --output="$1" v1.2.2 src/MixedFolderProvider.cpp src/MixedFolderProvider.def src/MixedFolder.h src/common.h'+"`n",[Text.Encoding]::ASCII)
Push-Location $ProjectRoot
try {
    & $bash --login $extract.Replace('\','/') $archive.Replace('\','/')
    if($LASTEXITCODE){throw 'Cannot extract the published v1.2.2 provider sources.'}
} finally {Pop-Location}
Expand-Archive -LiteralPath $archive -DestinationPath $output -Force
$source=Join-Path $output 'src'
$dll=Join-Path $output 'FileSpacerFolder.dll'
& cl.exe /nologo /std:c++14 /EHsc /W4 /MT /O2 /DSTRICT_TYPED_ITEMIDS ("/I$source") ("/Fo$output\MixedFolderProvider.obj") /LD ("/Fe$dll") (Join-Path $source 'MixedFolderProvider.cpp') /link /incremental:no ("/implib:$output\FileSpacerFolder.lib") ("/def:$source\MixedFolderProvider.def") User32.lib Shell32.lib Shlwapi.lib Ole32.lib OleAut32.lib Propsys.lib Uuid.lib | Out-Host
if($LASTEXITCODE){throw 'Published v1.2.2 provider compilation failed.'}
return $dll
