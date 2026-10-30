# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$cleanup=Join-Path $PSScriptRoot '..\..\installer\RemoveDocumentation.ps1'
function Fixture([string]$name) {
    $help=Join-Path $RunDirectory ($name+'\help')
    [void][IO.Directory]::CreateDirectory($help)
    foreach($file in @('index.html','obsolete.css','edited.css','notes.txt')){
        [IO.File]::WriteAllText((Join-Path $help $file),$file)
    }
    $files=[ordered]@{}
    foreach($file in @('index.html','obsolete.css','edited.css')){
        $files[$file]=(Get-FileHash -LiteralPath (Join-Path $help $file) -Algorithm SHA256).Hash
    }
    @{files=$files}|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $help 'build-info.json') -Encoding UTF8
    return $help
}
function Reject([string]$name,[string]$relative) {
    $help=Fixture $name
    $manifest=Join-Path $help 'build-info.json'
    $data=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
    $data.files|Add-Member -NotePropertyName $relative -NotePropertyValue ('0'*64)
    $data|ConvertTo-Json|Set-Content -LiteralPath $manifest -Encoding UTF8
    $rejected=$false
    try{& $cleanup -HelpRoot $help}catch{$rejected=$true}
    if(!$rejected){throw "Unsafe inventory accepted: $relative"}
    foreach($file in @('index.html','obsolete.css','edited.css','notes.txt','build-info.json')){
        if(!(Test-Path -LiteralPath (Join-Path $help $file))){throw "Invalid inventory caused partial cleanup: $relative"}
    }
}
$help=Fixture 'upgrade'
[IO.File]::WriteAllText((Join-Path $help 'edited.css'),'User changes.')
$next=Join-Path (Split-Path -Parent $help) 'next.json'
@{files=@{'index.html'=('0'*64)}}|ConvertTo-Json|Set-Content -LiteralPath $next -Encoding UTF8
& $cleanup -HelpRoot $help -CurrentInventory $next
if(Test-Path -LiteralPath (Join-Path $help 'obsolete.css')){throw 'Obsolete owned asset survived.'}
foreach($file in @('index.html','edited.css','notes.txt','build-info.json')){
    if(!(Test-Path -LiteralPath (Join-Path $help $file))){throw "Upgrade removed preserved file: $file"}
}
$help=Fixture 'uninstall'
& $cleanup -HelpRoot $help
foreach($file in @('index.html','obsolete.css','edited.css','build-info.json')){
    if(Test-Path -LiteralPath (Join-Path $help $file)){throw "Owned asset survived uninstall: $file"}
}
if(!(Test-Path -LiteralPath (Join-Path $help 'notes.txt'))){throw 'Unowned note was removed.'}
Reject 'traversal' '..\outside.txt'
Reject 'rooted' 'C:\outside.txt'
Reject 'wildcard' '*.txt'
Reject 'stream' 'notes.txt:payload'
Reject 'alias' 'index.html.'
Reject 'device' 'NUL.txt'
$help=Fixture 'junction'
$outside=Join-Path $RunDirectory 'outside'
[void][IO.Directory]::CreateDirectory($outside)
[IO.File]::WriteAllText((Join-Path $outside 'keep.txt'),'Outside help.')
[void](New-Item -ItemType Junction -Path (Join-Path $help 'linked') -Target $outside)
$manifest=Join-Path $help 'build-info.json'
$data=Get-Content -LiteralPath $manifest -Raw|ConvertFrom-Json
$data.files|Add-Member -NotePropertyName 'linked/keep.txt' -NotePropertyValue (Get-FileHash -LiteralPath (Join-Path $outside 'keep.txt')).Hash
$data|ConvertTo-Json|Set-Content -LiteralPath $manifest -Encoding UTF8
$rejected=$false
try{& $cleanup -HelpRoot $help}catch{$rejected=$true}
if(!$rejected -or [IO.File]::ReadAllText((Join-Path $outside 'keep.txt')) -cne 'Outside help.'){throw 'Reparse-point isolation failed.'}
if(!(Test-Path -LiteralPath (Join-Path $help 'index.html'))){throw 'Reparse rejection caused partial cleanup.'}
$help=Fixture 'missing'
Remove-Item -LiteralPath (Join-Path $help 'build-info.json')
& $cleanup -HelpRoot $help
if(!(Test-Path -LiteralPath (Join-Path $help 'obsolete.css'))){throw 'Missing inventory removed unknown content.'}
Write-Host 'PASS: upgrade, uninstall, modified/unowned files, missing inventory, path escapes, aliases, devices and junctions.'
