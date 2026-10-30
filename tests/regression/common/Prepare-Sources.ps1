param(
    [Parameter(Mandatory=$true)][ValidateSet('Menu','Shell','Settings','View','Adapters','Geometry','LockBadge','Documentation','JumpLists')][string]$Mode,
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [Parameter(Mandatory=$true)][string]$CaseDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'

# Extract actual source blocks. Braces in comments and string/character literals
# do not delimit a function. No dated production copy or expected source hash.
function Get-SourceBlock([string]$Text, [string]$Signature) {
    $start=$Text.IndexOf($Signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Production signature missing: $Signature" }
    if ($Text.IndexOf($Signature,$start+1,[StringComparison]::Ordinal) -ge 0) {
        throw "Ambiguous production signature: $Signature"
    }
    $brace=$Text.IndexOf('{',$start)
    if ($brace -lt 0) { throw "Production body missing: $Signature" }
    $depth=0; $state='code'; $escape=$false
    for ($i=$brace; $i -lt $Text.Length; ++$i) {
        $c=$Text[$i]; $next=if ($i+1 -lt $Text.Length) { $Text[$i+1] } else { [char]0 }
        if ($state -eq 'line') { if ($c -eq "`n") { $state='code' }; continue }
        if ($state -eq 'comment') { if ($c -eq '*' -and $next -eq '/') { $state='code'; ++$i }; continue }
        if ($state -eq 'string' -or $state -eq 'char') {
            if ($escape) { $escape=$false; continue }
            if ($c -eq '\') { $escape=$true; continue }
            if (($state -eq 'string' -and $c -eq '"') -or ($state -eq 'char' -and $c -eq "'")) { $state='code' }
            continue
        }
        if ($c -eq '/' -and $next -eq '/') { $state='line'; ++$i; continue }
        if ($c -eq '/' -and $next -eq '*') { $state='comment'; ++$i; continue }
        if ($c -eq '"') { $state='string'; continue }
        if ($c -eq "'") { $state='char'; continue }
        if ($c -eq '{') { ++$depth }
        if ($c -eq '}') {
            --$depth
            if ($depth -eq 0) { return $Text.Substring($start,$i-$start+1) }
        }
    }
    throw "Unterminated production block: $Signature"
}
function Read-Source([string]$Name) {
    $path=Join-Path $ProjectRoot ('src\'+$Name)
    $script:hashes[$Name]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    return [IO.File]::ReadAllText($path).Replace("`r`n","`n")
}
$hashes=[ordered]@{}
# Extracted units exercise the functional code, with the optional investigation
# logger disabled exactly as in the production diagnostic-off branch.
$traceDefinition='#define TRACE_SELECTION(...) ((void)0)'
$utf8=New-Object Text.UTF8Encoding($false)
function Save-Source([string]$Name,[string]$Text) {
    [IO.File]::WriteAllText((Join-Path $OutputDirectory $Name),$Text+"`n",$utf8)
}
[void][IO.Directory]::CreateDirectory($OutputDirectory)
switch ($Mode) {
    'JumpLists' {
        $source=Read-Source 'ClosedWindows.cpp'
        Save-Source 'JumpListProduction.inc' (Get-SourceBlock $source 'void updateReopenJumpLists()')
    }
    'Documentation' {
        $source=Read-Source 'SettingsDialog.cpp'
        Save-Source 'DocumentationProduction.inc' (Get-SourceBlock $source 'void openDocumentation(HWND owner)')
    }
    'LockBadge' {
        $source=Read-Source 'ProxyIcon.cpp'
        $start=$source.IndexOf('// Native ICO loading',[StringComparison]::Ordinal)
        $end=$source.IndexOf('void ProxyIcon::init()',[StringComparison]::Ordinal)
        if ($start -lt 0 -or $end -le $start) { throw 'Production ICO badge helper not found.' }
        Save-Source 'LockBadge-under-test.inc' $source.Substring($start,$end-$start)
    }
    'Adapters' {
        $settings=Read-Source 'ShellViewSettings.cpp'
        $helpers=Get-SourceBlock $settings "namespace {`nFolderViewSettings::Key stateKey"
        foreach ($signature in @('bool sameView(', 'FolderViewSettings captureOptions(', 'HRESULT restoreOptions(')) {
            $helpers+="`n"+(Get-SourceBlock $settings $signature)
        }
        $prefix=[IO.File]::ReadAllText((Join-Path $CaseDirectory 'ViewAdapters-prefix.inc'))
        $suffix=[IO.File]::ReadAllText((Join-Path $CaseDirectory 'ViewAdapters-suffix.inc'))
        Save-Source 'ViewAdaptersTests.cpp' ($prefix+"`n"+$traceDefinition+"`n"+$helpers+"`n"+$suffix)
    }
    'Geometry' {
        $source=Read-Source 'ItemWindow.cpp'; $pieces=@()
        foreach ($signature in @('bool ItemWindow::normalWindowRect(', 'void ItemWindow::recordGeometry(', 'uint32_t ItemWindow::captureViewState(')) {
            $pieces+=Get-SourceBlock $source $signature
        }
        $prefix=[IO.File]::ReadAllText((Join-Path $CaseDirectory 'Geometry-prefix.inc'))
        $suffix=[IO.File]::ReadAllText((Join-Path $CaseDirectory 'Geometry-suffix.inc'))
        Save-Source 'GeometryTests.cpp' ($prefix+"`n"+($pieces -join "`n`n")+"`n"+$suffix)
    }
    'Menu' {
        $item=Read-Source 'ItemWindow.cpp'; $folder=Read-Source 'FolderWindow.cpp'
        $pieces=@()
        foreach ($signature in @('void ItemWindow::trackContextMenu(POINT pos)', 'int ItemWindow::trackContextMenu(POINT pos, HMENU menu, HWND menuOwner)')) {
            $pieces+=Get-SourceBlock $item $signature
        }
        foreach ($signature in @('CComPtr<IContextMenu> FolderWindow::queryBackgroundMenu(HMENU *popupMenu)', 'void FolderWindow::newItem(const char *verb)', 'void FolderWindow::trackContextMenu(POINT pos)', 'void FolderWindow::openViewMenu(POINT point)', 'void FolderWindow::openBackgroundSubMenu(', 'HRESULT FolderWindow::invokeViewCommand(', 'void FolderWindow::resumeDeferredClose()')) {
            $pieces+=Get-SourceBlock $folder $signature
        }
        Save-Source 'ProductionMethods.inc' ($pieces -join "`n`n")
    }
    'Shell' {
        $source=Read-Source 'main.cpp'
        $private=Get-SourceBlock $source "namespace {`n`nLONG processLockCount"
        $lock=Get-SourceBlock $source 'void lockProcess()'
        $unlock=Get-SourceBlock $source 'void unlockProcess()'
        $invoke=Get-SourceBlock $source 'HRESULT filespacer::invokeShellCommand('
        Save-Source 'ShellLifetimeProduction.inc' ($private+"`n"+$invoke+"`nnamespace filespacer {`n"+$lock+"`n"+$unlock+"`n}")
    }
    'Settings' {
        $source=Read-Source 'SettingsDialog.cpp'; $pieces=@()
        foreach ($signature in @('void enableToolbarOptions(HWND hwnd)', 'static SIZE minimumFolderWindowSize()', 'static bool readFolderWindowSize(HWND hwnd, SIZE &size)', 'INT_PTR CALLBACK displayProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)')) {
            $pieces+=Get-SourceBlock $source $signature
        }
        Save-Source 'SettingsDisplayProduction.inc' ($pieces -join "`n`n")
    }
    'View' {
        $header=Read-Source 'FolderWindow.h'
        $state=Get-SourceBlock $header '    struct ShellViewState'
        $settings=Read-Source 'ShellViewSettings.cpp'
        $helpers=Get-SourceBlock $settings "namespace {`nFolderViewSettings::Key stateKey"
        foreach ($signature in @('bool sameView(', 'FolderViewSettings captureOptions(', 'HRESULT restoreOptions(')) {
            $helpers+="`n"+(Get-SourceBlock $settings $signature)
        }
        $prefix=@'
#include "fake-shell.h"
class FolderWindow { public:
'@
        $cases=[IO.File]::ReadAllText((Join-Path $CaseDirectory 'ViewStateCases.inc'))
        Save-Source 'ViewStateTests.cpp' ($prefix+"`n"+$traceDefinition+"`n"+$state+";`n};`n"+$helpers+"`n"+$cases)
    }
}
Save-Source 'production-hashes.json' ($hashes | ConvertTo-Json)

