# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([Parameter(Mandatory=$true)][string]$ProjectRoot,[string[]]$Languages=@('en','fr','de','it','es','pt','el','pl','nl','uk'))
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$ProjectRoot=(Resolve-Path -LiteralPath $ProjectRoot).Path
foreach($name in @('FileSpacer.exe','FileSpacerFolder.dll')) {
    if(!(Test-Path -LiteralPath (Join-Path $ProjectRoot ('build\debug\'+$name)))){throw ('Build the validated Debug application first: missing '+$name)}
}
foreach($language in $Languages) {
    if($language-notmatch'^[a-z]{2}$'){throw 'Invalid capture language.'}
    $output=Join-Path $ProjectRoot ('build\documentation-capture\reusable-binaries\'+$language)
    [void][IO.Directory]::CreateDirectory($output)
    foreach($name in @('FileSpacer.exe','FileSpacerFolder.dll')) {
        $original=Join-Path $ProjectRoot ('build\debug\'+$name)
        $copy=Join-Path $output $name
        Copy-Item -LiteralPath $original -Destination $copy -Force
        if((Get-FileHash -LiteralPath $original).Hash-ne(Get-FileHash -LiteralPath $copy).Hash){throw ('Capture binary differs from the actual application: '+$name)}
    }
}
