# Test-Category: UI
# Mixed sorting on the real DefView: logical number order and case folding on the
# Name column (oracle: StrCmpLogicalW, the documented Explorer name comparison),
# and the Size column with ties and folders (no size value): folders grouped at
# one end, files by value, equal sizes contiguous. Nothing is hard-coded about
# how the provider compares: the oracle and the real view order are compared.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class LogicalNameOrder {
    [DllImport("shlwapi.dll", CharSet=CharSet.Unicode, ExactSpelling=true)]
    static extern int StrCmpLogicalW(string a, string b);
    public static string[] Sort(string[] names, bool ascending) {
        var list=new List<string>(names);
        list.Sort((a,b)=>ascending?StrCmpLogicalW(a,b):StrCmpLogicalW(b,a));
        return list.ToArray();
    }
}
'@
$failure=$null
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=Test-Path -LiteralPath $junction
$sizeKey=[guid]'B725F130-47EF-101A-A5F1-02608C9EEBAC'
function Wait-Provider([IntPtr]$Window) {
    Wait-Ui {
        try { return Use-UiView $Window {param($view) $view.ContentVisible -and ($view.FolderClassId -eq [guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604')} }
        catch [InvalidCastException] { return $false }
        catch [Runtime.InteropServices.COMException] { return $false }
    } 'mixed provider active' 300
}
# Observes the real view order until it is complete and stable; returns the names.
function Get-StableOrder([IntPtr]$Window,[int]$Count) {
    # A hashtable: assignments inside the Wait-Ui script block would otherwise create locals.
    $state=@{Last=$null;Since=$null;Names=@()}
    Wait-Ui {
        try { $names=@(Use-UiView $Window {param($view) $view.OrderedNames()}) }
        catch [Runtime.InteropServices.COMException] { if($_.Exception.HResult -notin @(-2147023728,-2147483638)){throw};return $false }
        $joined=$names -join '|'
        if($names.Count -ne $Count -or $joined -cne $state.Last){$state.Names=$names;$state.Last=$joined;$state.Since=Get-Date;return $false}
        return ((Get-Date)-$state.Since).TotalMilliseconds -ge 1500
    } 'complete and stable view order' 300
    return $state.Names
}
function Assert-Order([string[]]$Actual,[string[]]$Expected,[string]$What) {
    Write-Host ('DIAG: '+$What+' expected=['+($Expected -join '|')+'] actual=['+($Actual -join '|')+']')
    Assert-Ui (($Actual -join '|') -ceq ($Expected -join '|')) ($What+' differs from the StrCmpLogicalW oracle.')
}
try {
    Initialize-Ui 'mixed-order'
    Set-UiPreference 'MixedFolderSorting' 1
    $folder=Join-Path $script:UiRoot 'items';[void][IO.Directory]::CreateDirectory($folder)
    # Name column: digits compared numerically, case folded, folders and files interleaved.
    $files=@('b-2.txt','B-10.txt','a-20.txt','item-9.txt','ITEM-100.txt')
    $folders=@('b-7','b-1','A-3','item-10','Item-1')
    foreach($name in $files){[IO.File]::WriteAllText((Join-Path $folder $name),'fixture')}
    foreach($name in $folders){[void][IO.Directory]::CreateDirectory((Join-Path $folder $name))}
    # Size column: two equal sizes, one smaller file, folders without a size value.
    $sizes=@{'s-a.txt'=10;'s-b.txt'=10;'s-c.txt'=5}
    foreach($entry in $sizes.GetEnumerator()){[IO.File]::WriteAllText((Join-Path $folder $entry.Key),('x'*$entry.Value))}
    $sizeFolders=@('s-d','s-e')
    foreach($name in $sizeFolders){[void][IO.Directory]::CreateDirectory((Join-Path $folder $name))}
    $all=@($files+$folders+@($sizes.Keys)+$sizeFolders)
    $window=Open-UiFolder $folder $all.Count
    Wait-Provider $window
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48)}
    foreach($ascending in @($true,$false)) {
        Use-UiView $window {param($view) $view.SortBy($false,$ascending)}
        $actual=Get-StableOrder $window $all.Count
        Assert-Order $actual ([LogicalNameOrder]::Sort($all,$ascending)) ('Name '+$(if($ascending){'ascending'}else{'descending'}))
    }
    Write-Host 'PASS: the mixed Name order equals StrCmpLogicalW (numbers, case) with folders interleaved.'
    foreach($ascending in @($true,$false)) {
        [void](Use-UiView $window {param($view) $view.SortByColumn($sizeKey,12,$ascending)})
        $actual=Get-StableOrder $window $all.Count
        Write-Host ('DIAG: Size '+$(if($ascending){'ascending'}else{'descending'})+' actual=['+($actual -join '|')+']')
        $folderNames=@($folders+$sizeFolders)
        $folderPositions=@(0..($actual.Count-1)|Where-Object{$folderNames -ccontains $actual[$_]})
        $filePositions=@(0..($actual.Count-1)|Where-Object{$folderNames -cnotcontains $actual[$_]})
        # Folders carry no size: grouped at one end, files by value on the other side.
        if($ascending){Assert-Ui ((($folderPositions|Measure-Object -Maximum).Maximum) -lt (($filePositions|Measure-Object -Minimum).Minimum)) 'Ascending Size: folders are not grouped before the files.'}
        else{Assert-Ui ((($folderPositions|Measure-Object -Minimum).Minimum) -gt (($filePositions|Measure-Object -Maximum).Maximum)) 'Descending Size: folders are not grouped after the files.'}
        $fileOrder=@($actual|Where-Object{$folderNames -cnotcontains $_})
        $c=[array]::IndexOf($fileOrder,'s-c.txt');$a=[array]::IndexOf($fileOrder,'s-a.txt');$b=[array]::IndexOf($fileOrder,'s-b.txt')
        $smallFirst=if($ascending){$c -lt $a -and $c -lt $b}else{$c -gt $a -and $c -gt $b}
        Assert-Ui $smallFirst 'Size: the smaller file is not ordered by value against the equal pair.'
        Assert-Ui ([math]::Abs($a-$b) -eq 1) 'Size: the two equal sizes are not contiguous.'
    }
    Write-Host 'PASS: the mixed Size order keeps folders grouped at one end, files by value and ties contiguous.'
    Close-UiFolder $window
}catch{$failure=$_;Write-Host $_.Exception.ToString();throw}finally{
    try { Stop-Ui -Failure $failure } finally {
    if(!$registrationExisted -and (Test-Path -LiteralPath $providerKey)){Remove-Item -LiteralPath $providerKey -Recurse -Force}
    if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)} }
}
