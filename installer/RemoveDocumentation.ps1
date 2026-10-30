# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$HelpRoot,
    [string]$CurrentInventory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
$root=[IO.Path]::GetFullPath($HelpRoot).TrimEnd('\')
if([IO.Path]::GetFileName($root) -ine 'help'){throw 'Documentation cleanup requires a help directory.'}
$prefix=$root+'\'

function Owned-Path([string]$relative) {
    if([IO.Path]::IsPathRooted($relative)){throw "Rooted documentation path: $relative"}
    foreach($part in ($relative -split '[\\/]')) {
        if(!$part -or $part -in @('.','..') -or $part.TrimEnd([char[]]'. ') -cne $part -or
            $part.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0 -or
            $part -match '^(?i:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)'){
            throw "Invalid documentation path: $relative"
        }
    }
    $full=[IO.Path]::GetFullPath([IO.Path]::Combine($root,$relative))
    if(!$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "Documentation path escapes help: $relative"}
    for($entry=$full;;$entry=[IO.Path]::GetDirectoryName($entry)) {
        if(Test-Path -LiteralPath $entry) {
            if((Get-Item -LiteralPath $entry -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){
                throw "Documentation path crosses a reparse point: $entry"
            }
        }
        if($entry -ieq $root){break}
    }
    return $full
}

function Read-Inventory([string]$path) {
    $inventory=Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json
    if($inventory.files -isnot [PSCustomObject]){throw "Invalid documentation inventory: $path"}
    $owned=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($file in $inventory.files.PSObject.Properties) {
        if($file.Value -isnot [string] -or $file.Value -notmatch '^[0-9a-fA-F]{64}$'){
            throw "Invalid documentation digest: $($file.Name)"
        }
        $full=Owned-Path $file.Name
        $owned.Add($full,$file.Value)
    }
    return ,$owned
}

$installed=Owned-Path 'build-info.json'
# Older releases had no embedded help. An unowned directory is preserved.
if(!(Test-Path -LiteralPath $installed -PathType Leaf)){return}
$previous=Read-Inventory $installed
$current=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
if($CurrentInventory){$current=Read-Inventory $CurrentInventory}
# Validate both inventories before deleting anything. Retain obsolete files
# changed since installation, and all files absent from the old inventory.
$directories=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($file in $previous.GetEnumerator()) {
    if($current.ContainsKey($file.Key) -or !(Test-Path -LiteralPath $file.Key -PathType Leaf)){continue}
    if((Get-FileHash -LiteralPath $file.Key -Algorithm SHA256).Hash -ine $file.Value) {
        Write-Host "Preserving modified documentation: $($file.Key)"
        continue
    }
    Remove-Item -LiteralPath $file.Key -Force
    for($parent=[IO.Path]::GetDirectoryName($file.Key);$parent.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase);$parent=[IO.Path]::GetDirectoryName($parent)){
        [void]$directories.Add($parent)
    }
}
if(!$CurrentInventory){Remove-Item -LiteralPath $installed -Force}
foreach($directory in @($directories|Sort-Object Length -Descending)+@($root)) {
    if(Test-Path -LiteralPath $directory -PathType Container) {
        try {[IO.Directory]::Delete($directory,$false)}
        catch [IO.IOException] {
            if(![IO.Directory]::EnumerateFileSystemEntries($directory).GetEnumerator().MoveNext()){throw}
        }
    }
}
