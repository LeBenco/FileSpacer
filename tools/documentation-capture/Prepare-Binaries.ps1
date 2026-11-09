# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([Parameter(Mandatory=$true)][string]$ProjectRoot,[string[]]$Languages=@('en','fr','de','it','es','pt','el','pl','nl','uk'))
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
$makefile=Get-Content -LiteralPath (Join-Path $ProjectRoot 'Makefile') -Raw
$list=[regex]::Match($makefile,'(?ms)^OBJECTS = (.*?)(?=\r?\n\r?\n)').Groups[1].Value
$objects=@([regex]::Matches($list,'\$\(OUTDIR\)\\([^\s]+\.obj)') | ForEach-Object {Join-Path $ProjectRoot ('build\debug\'+$_.Groups[1].Value)})
$objects+=Join-Path $ProjectRoot 'build\debug\sqlite3.obj'
if($objects.Count-lt30){throw 'Could not read the current Makefile object list.'}
foreach($file in $objects){if(!(Test-Path -LiteralPath $file)){throw ('Build the validated Debug application first: missing '+$file)}}
$resource=Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\resource.rc') -Raw
foreach($language in $Languages) {
    if($language-notmatch'^[a-z]{2}$'){throw 'Invalid capture language.'}
    $output=Join-Path $ProjectRoot ('build\documentation-capture\reusable-binaries\'+$language)
    [void][IO.Directory]::CreateDirectory($output)
    $single=[regex]::Replace($resource,'(?m)^#include "(?:resource|dialog)\.([a-z]{2})\.rc"\r?\n',{
        param($match)
        if($match.Groups[1].Value-eq$language){return $match.Value};return ''
    })
    [IO.File]::WriteAllText((Join-Path $output 'resource.rc'),$single,(New-Object Text.UTF8Encoding($false)))
    & rc.exe /nologo /D _UNICODE /D UNICODE /n /I (Join-Path $ProjectRoot 'src') /fo (Join-Path $output 'resource.res') (Join-Path $output 'resource.rc')
    if($LASTEXITCODE-ne0){throw ('Resource compilation failed: '+$language)}
    & link.exe /nologo /incremental:no /manifest:no /subsystem:windows ('/out:'+(Join-Path $output 'FileSpacer.exe')) @objects (Join-Path $output 'resource.res') Uuid.lib
    if($LASTEXITCODE-ne0){throw ('Documentation link failed: '+$language)}
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacerFolder.dll') -Destination $output
}
