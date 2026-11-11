Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop

function Write-PrivateDiagnostics([string]$ProjectRoot,[string]$RunDirectory) {
    $records=@();$decisions=@();$errors=@();$observed=@()
    foreach($file in @(Get-ChildItem -LiteralPath $RunDirectory -Recurse -File -Filter 'private-capabilities.tsv')) {
        foreach($entry in @(Import-Csv -LiteralPath $file.FullName -Delimiter "`t")) {
            $records+=[pscustomobject]@{Source=$file.FullName.Substring($RunDirectory.Length+1);Kind=$entry.Kind;Name=$entry.Name;Value=$entry.Value;Detail=$entry.Detail}
        }
    }
    foreach($file in @(Get-ChildItem -LiteralPath $RunDirectory -Recurse -File | Where-Object {$_.Name -match '\.(public|attempt)$'})) {
        $bytes=[IO.File]::ReadAllBytes($file.FullName)
        $prefix=[Text.Encoding]::ASCII.GetBytes('FSPUBLIC01');$offset=0
        $kind='TransientDiscoveryFailure'
        if($bytes.Length -ge $prefix.Length -and [Text.Encoding]::ASCII.GetString($bytes,0,$prefix.Length) -ceq 'FSPUBLIC01') {
            $kind='StructuralIncompatibility';$offset=$prefix.Length
            if($file.Name -match '\.range\.(public|attempt)$'){$offset+=24} # PDB GUID, age and image size precede the reason.
        }
        $reason=if($offset -le $bytes.Length){[Text.Encoding]::UTF8.GetString($bytes,$offset,$bytes.Length-$offset).Trim()}else{'Truncated decision record'}
        $decisions+=[pscustomobject]@{File=$file.FullName.Substring($RunDirectory.Length+1);Kind=$kind;Reason=$reason;Sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
    }
    foreach($file in @(Get-ChildItem -LiteralPath $RunDirectory -Recurse -File -Filter '*.log')) {
        foreach($line in @(Select-String -LiteralPath $file.FullName -Pattern '^(PRODUCT |RESOLVED |FALLBACK:|Optional native range|PASS: unsupported private|COVERAGE |VIEW |PARTIAL_ALL |SELECT_ALL )')) {
            $observed+=[pscustomobject]@{File=$file.FullName.Substring($RunDirectory.Length+1);Line=$line.LineNumber;Text=$line.Line}
        }
        if($file.Name -match 'error' -and $file.Length){$errors+=[pscustomobject]@{File=$file.FullName.Substring($RunDirectory.Length+1);Text=[IO.File]::ReadAllText($file.FullName)}}
    }
    $modules=@(foreach($name in @('shell32.dll','comctl32.dll')) {
        $path=Join-Path ([Environment]::SystemDirectory) $name
        if(Test-Path -LiteralPath $path){$file=Get-Item -LiteralPath $path;[pscustomobject]@{Path=$path;FileVersion=$file.VersionInfo.FileVersion;Sha256=(Get-FileHash -LiteralPath $path).Hash;Scope='System copy; loaded control identity is recorded separately by the native probe'}}
    })
    $capabilities=@($records|Where-Object Kind -eq 'capability')
    $unavailable=@($capabilities|Where-Object Value -ne '1')
    $status=if(!$capabilities.Count){'NOT_OBSERVED'}elseif($unavailable.Count){'PARTIAL'}else{'AVAILABLE'}
    [ordered]@{Status=$status;Windows=[Environment]::OSVersion.VersionString;Architecture=$env:PROCESSOR_ARCHITECTURE;
        ExecutableSha256=(Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe')).Hash;
        SystemModules=$modules;ProbeRecords=$records;DiscoveryDecisions=$decisions;ObservedDispatch=$observed;Errors=$errors;
        Interpretation='Availability is separate from functional PASS. Observations include deliberately degraded test arms identified by Source. An unresolved descriptor does not prove a function is absent from the DLL. Saved incompatibilities and transient discovery failures retain their original reason.'} |
        ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $RunDirectory 'private-diagnostics.json') -Encoding UTF8
    Write-Host ('PRIVATE DIAGNOSTICS: '+$status+'; unavailable observations='+$unavailable.Count+'; decisions='+$decisions.Count)
}
