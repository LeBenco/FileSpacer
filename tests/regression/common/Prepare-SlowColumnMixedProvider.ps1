# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
# Host only: build probe variants of the current mixed provider from a modified
# COPY of its sources (the checkout's src/ is never touched). Two variants:
#   slow : keep SHCOLSTATE_PREFER_VARCMP only on SHCOLSTATE_SLOW columns (M-17: now the shipped flags);
#   all  : keep SHCOLSTATE_PREFER_VARCMP on every column (control: does DefView honour
#          NOSORTBYFOLDERNESS when it compares values itself on PIDL columns?).
#   counted : flags unchanged (same comparator as the shipped DLL), instrumented only.
# Returns the DLL paths (counted, slow, all) under build\regression\slow-column-provider.
param([Parameter(Mandatory=$true)][string]$ProjectRoot)
$ErrorActionPreference='Stop'
if($env:FS_REGRESSION_PREPARED){throw 'The probe provider variants must be prepared on the host.'}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)){throw 'Run tests.cmd or use an x64 MSVC developer prompt.'}
# The shipped provider (M-17) keeps PREFER_VARCMP on SHCOLSTATE_SLOW columns:
# these three lines form the block the variants replace.
$original='            *out &= ~static_cast<SHCOLSTATEF>(SHCOLSTATE_PREFER_FMTCMP);'
$originalBlock=@($original,
    '            if (!(*out & static_cast<SHCOLSTATEF>(SHCOLSTATE_SLOW)))',
    '                *out &= ~static_cast<SHCOLSTATEF>(SHCOLSTATE_PREFER_VARCMP);')
$variants=[ordered]@{
    counted=$originalBlock
    slow=$originalBlock
    all=@('            *out &= ~static_cast<SHCOLSTATEF>(SHCOLSTATE_PREFER_FMTCMP);')
}
# Every variant also counts its CompareIDs calls in the isolated /test key, so the
# case can tell whether DefView compared values itself or still called the folder.
$compareSignature='    STDMETHODIMP CompareIDs(LPARAM flags, PCUIDLIST_RELATIVE left, PCUIDLIST_RELATIVE right) override {'
$lastInclude='#include <limits>'
$counter=@(
    '// Probe instrumentation (test copy only): count CompareIDs calls in the /test preferences.',
    'static void probeCountCompare() {',
    '    static LONG count = 0;',
    '    const DWORD value = static_cast<DWORD>(InterlockedIncrement(&count));',
    '    HKEY key;',
    '    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\FileSpacer\\Test", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {',
    '        RegSetValueExW(key, L"ProbeCompareIds", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&value), sizeof(value));',
    '        RegCloseKey(key);',
    '    }',
    '}')
$root=Join-Path $ProjectRoot 'build\regression\slow-column-provider'
# Link with the libraries of the Makefile's own provider rule, so the copy
# follows the checkout's source (a new import there must not break the probe).
$makefile=[IO.File]::ReadAllLines((Join-Path $ProjectRoot 'Makefile'))
$ruleAt=@(0..($makefile.Count-1) | Where-Object { $makefile[$_] -match '^\$\(OUTDIR\)\\FileSpacerFolder\.dll:' })
if($ruleAt.Count -ne 1 -or $ruleAt[0]+1 -ge $makefile.Count){throw 'The Makefile rule for FileSpacerFolder.dll was not found.'}
$libraries=@([regex]::Matches($makefile[$ruleAt[0]+1],'\b[A-Za-z0-9_]+\.lib\b') | ForEach-Object { $_.Value })
if(!$libraries.Count){throw 'The Makefile rule for FileSpacerFolder.dll names no import library.'}
$results=@()
foreach($name in $variants.Keys) {
    $output=Join-Path $root $name
    $source=Join-Path $output 'src'
    [void][IO.Directory]::CreateDirectory($source)
    foreach($file in @('MixedFolderProvider.cpp','MixedFolderProvider.def','MixedFolder.h','common.h')) {
        Copy-Item -LiteralPath (Join-Path $ProjectRoot ('src\'+$file)) -Destination (Join-Path $source $file) -Force
    }
    $cpp=Join-Path $source 'MixedFolderProvider.cpp'
    $lines=[IO.File]::ReadAllLines($cpp)
    $hits=@(0..($lines.Count-1) | Where-Object { $lines[$_] -ceq $original })
    if($hits.Count -ne 1){throw ('Expected exactly one flag-removal line in MixedFolderProvider.cpp, found '+$hits.Count+'; the probe no longer matches the source.')}
    for($k=0;$k -lt $originalBlock.Count;$k++){ if($lines[$hits[0]+$k] -cne $originalBlock[$k]){throw ('The flag block of MixedFolderProvider.cpp no longer matches the probe at line '+($hits[0]+$k+1)+'.')} }
    $patched=@($lines[0..($hits[0]-1)])+@($variants[$name])+@($lines[($hits[0]+$originalBlock.Count)..($lines.Count-1)])
    $compareAt=@(0..($patched.Count-1) | Where-Object { $patched[$_] -ceq $compareSignature })
    $includeAt=@(0..($patched.Count-1) | Where-Object { $patched[$_] -ceq $lastInclude })
    if($compareAt.Count -ne 1 -or $includeAt.Count -ne 1){throw 'The CompareIDs signature or the last include of MixedFolderProvider.cpp no longer matches the probe.'}
    $patched=@($patched[0..$compareAt[0]])+@('        probeCountCompare();')+@($patched[($compareAt[0]+1)..($patched.Count-1)])
    $patched=@($patched[0..$includeAt[0]])+$counter+@($patched[($includeAt[0]+1)..($patched.Count-1)])
    [IO.File]::WriteAllLines($cpp,[string[]]$patched,(New-Object Text.ASCIIEncoding))
    $note=@(('Replaced lines '+($hits[0]+1)+'-'+($hits[0]+$originalBlock.Count)+':'))+$originalBlock+@('with:')+@($variants[$name])+@('and counted CompareIDs calls (ProbeCompareIds).')
    [IO.File]::WriteAllLines((Join-Path $output 'variant.txt'),[string[]]$note)
    $dll=Join-Path $output 'FileSpacerFolder.dll'
    # Same flags as the Makefile's Debug provider (minus /MP and the PDB), so the
    # variant behaves like build\debug\FileSpacerFolder.dll apart from the patch.
    & cl.exe /nologo /EHsc /W4 /MTd /D FILESPACER_DEBUG /DSTRICT_TYPED_ITEMIDS ("/I$source") ("/Fo$output\MixedFolderProvider.obj") /LD ("/Fe$dll") $cpp /link /incremental:no ("/implib:$output\FileSpacerFolder.lib") ("/def:$source\MixedFolderProvider.def") @libraries | Out-Host
    if($LASTEXITCODE){throw ('Probe provider variant compilation failed: '+$name)}
    $results+=$dll
}
return $results
