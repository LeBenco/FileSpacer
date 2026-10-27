# Select in Run-Regression first. Preparation inspects scripts; it never runs a
# scenario (or UI setup) on the development host.
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop
function Invoke-ClientRegression {
    param([string]$VM,$Selected,[int]$TimeoutSeconds,[string]$RunDirectory)
    if ($VM -match '[\\/:*?"<>|]' -or $VM -in @('.','..')) { throw 'Invalid VM profile name.' }
    $configFile=Join-Path $env:ProgramData ('FileSpacerTests\'+$VM+'\runner.json')
    if (!(Test-Path -LiteralPath $configFile)) { throw ('VM is not configured for unattended tests: '+$VM) }
    $config=Get-Content -LiteralPath $configFile -Raw | ConvertFrom-Json
    if ($config.VMName -ne $VM) { throw 'Configured VM name differs from the requested VM.' }
    if (!$config.PSObject.Properties['ProtocolVersion'] -or $config.ProtocolVersion -ne 2) {
        throw 'The protected controller needs its one-time regression update; run Install-HyperVRunner.ps1 -UpdateController at an agreed time.'
    }
    foreach($name in @('Connect-HyperV.ps1','Run-HyperVSmoke.ps1','HyperVRunnerTask.ps1')) {
        $source=Join-Path $ProjectRoot ('tests\client\'+$name)
        $installed=Join-Path (Split-Path $configFile -Parent) $name
        # Git checkouts may differ only in CRLF/LF; that is not a controller update.
        if ([IO.File]::ReadAllText($source).Replace("`r`n","`n") -cne [IO.File]::ReadAllText($installed).Replace("`r`n","`n")) {
            throw ('Protected controller differs from this checkout: '+$name+'. Apply its explicit configuration update before running tests.')
        }
    }
    $runId=[guid]::NewGuid().ToString('N')
    $run=Join-Path $ProjectRoot ('build\regression\vm-'+$runId)
    $reportDestination=''
    if ($RunDirectory) {
        $reportDestination=[IO.Path]::GetFullPath($RunDirectory)
        if (Test-Path -LiteralPath $reportDestination) { throw 'The requested result directory already exists; previous results will not be overwritten.' }
    }
    [void][IO.Directory]::CreateDirectory($run)
    $package=Join-Path $run 'package'
    [void][IO.Directory]::CreateDirectory($package)
    [void](Start-Transcript -LiteralPath (Join-Path $run 'preparation.log'))
    try { New-ClientPackage -Selected $Selected -Destination $package -TimeoutSeconds $TimeoutSeconds }
    finally { [void](Stop-Transcript) }
    try {
        & (Join-Path $ProjectRoot 'tests\client\Invoke-HyperVRunner.ps1') -ProjectRoot $ProjectRoot -VMName $VM -RegressionRunId $runId
    } finally {
        if ($reportDestination) {
            [void][IO.Directory]::CreateDirectory($reportDestination)
            Get-ChildItem -LiteralPath $run | Copy-Item -Destination $reportDestination -Recurse
            Write-Host ('Reports copied to '+$reportDestination)
        }
    }
}

function New-ClientPackage {
    param($Selected,[string]$Destination,[int]$TimeoutSeconds)
    $regression=Join-Path $ProjectRoot 'tests\regression'
    $prepared=Join-Path $Destination 'prepared'
    [void][IO.Directory]::CreateDirectory($prepared)
    $managed=New-Object Collections.ArrayList; $native=New-Object Collections.ArrayList
    $copied=@{}; $built=@{}
    function Copy-PackageFile([string]$File) {
        $file=[IO.Path]::GetFullPath($File)
        if (!$file.StartsWith($ProjectRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw ('Dependency outside checkout: '+$file) }
        if ($copied.ContainsKey($file)) { return }
        $target=Join-Path $Destination $file.Substring($ProjectRoot.Length+1)
        [void][IO.Directory]::CreateDirectory((Split-Path $target -Parent))
        Copy-Item -LiteralPath $file -Destination $target
        $copied[$file]=$true
    }
    function Read-TestAst([string]$File) {
        $tokens=$null;$errors=$null
        $ast=[Management.Automation.Language.Parser]::ParseFile($File,[ref]$tokens,[ref]$errors)
        if ($errors.Count) { throw ('Cannot prepare invalid PowerShell: '+$File+': '+$errors[0].Message) }
        return $ast
    }
    function Get-Dependencies([string]$File,$Found) {
        $file=[IO.Path]::GetFullPath($File)
        if ($Found.ContainsKey($file)) { return }
        $Found[$file]=$true
        $ast=Read-TestAst $file
        foreach($literal in $ast.FindAll({param($a) $a -is [Management.Automation.Language.StringConstantExpressionAst]},$true)) {
            $value=$literal.Value
            if ($value -notmatch '\.(ps1|cs)$' -or $value -match '[*?\r\n]') { continue }
            # Most paths are relative to PSScriptRoot; a few helper aliases
            # name a unique CS file in another test group.
            $candidate=[IO.Path]::GetFullPath((Join-Path (Split-Path $file -Parent) $value))
            if (!(Test-Path -LiteralPath $candidate -PathType Leaf)) {
                $matches=@(Get-ChildItem -LiteralPath $regression -Recurse -File | Where-Object { $_.Name -eq [IO.Path]::GetFileName($value) })
                if ($matches.Count -ne 1) { throw ('Unresolved test dependency: '+$file+' -> '+$value) }
                $candidate=$matches[0].FullName
            }
            if ($candidate.EndsWith('.ps1',[StringComparison]::OrdinalIgnoreCase)) { Get-Dependencies $candidate $Found }
            else { $Found[$candidate]=$true }
        }
    }
    function Build-ManagedBundle([string]$Case,$Files) {
        $hashes=@();$sources=@()
        $bundle='managed-'+$managed.Count
        $intermediate=Join-Path (Split-Path $Destination -Parent) $bundle
        [void][IO.Directory]::CreateDirectory($intermediate)
        foreach($file in @($Files.Keys | Sort-Object)) {
            if ($file.EndsWith('.cs',[StringComparison]::OrdinalIgnoreCase)) { $sources+=$file }
            elseif ($file.EndsWith('.ps1',[StringComparison]::OrdinalIgnoreCase)) {
                $ast=Read-TestAst $file
                foreach($command in $ast.FindAll({param($a) $a -is [Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'Add-Type'},$true)) {
                    for($i=1;$i -lt $command.CommandElements.Count;$i++) {
                        if ($command.CommandElements[$i].Extent.Text -eq '-TypeDefinition') {
                            $definition=$command.CommandElements[$i+1].SafeGetValue()
                            $source=Join-Path $intermediate ('inline-'+$sources.Count+'.cs')
                            [IO.File]::WriteAllText($source,$definition,(New-Object Text.UTF8Encoding($false)))
                            $sources+=$source
                        }
                    }
                }
            }
        }
        if (!$sources.Count) { return }
        $sha=[Security.Cryptography.SHA256]::Create()
        try { foreach($source in $sources){$hashes+=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText($source))))).Replace('-','')} }
        finally { $sha.Dispose() }
        $assembly=Join-Path $prepared ($bundle+'.dll')
        $framework=Join-Path $env:SystemRoot 'Microsoft.NET\Framework64\v4.0.30319'
        $arguments=@('/nologo','/target:library','/platform:x64',('/out:'+$assembly),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll')
        foreach($name in @('UIAutomationClient','UIAutomationTypes','WindowsBase')) { $arguments+=('/reference:'+(Join-Path $framework ('WPF\'+$name+'.dll'))) }
        & (Join-Path $framework 'csc.exe') @arguments @sources | Out-Host
        if ($LASTEXITCODE -ne 0) { throw ('Host helper compilation failed: '+$Case) }
        [void]$managed.Add(@{Case=$Case;Assembly=($bundle+'.dll');Hashes=$hashes;Sha256=(Get-FileHash -LiteralPath $assembly -Algorithm SHA256).Hash})
    }
    # Only inspect selected scripts and their dependencies, plus runner helpers.
    foreach($relative in @('Run-Regression.ps1','common\Process.ps1','common\Results.ps1','common\Prepared.ps1','common\Prepared-Case.ps1')) { Copy-PackageFile (Join-Path $regression $relative) }
    Copy-PackageFile (Join-Path $ProjectRoot 'tests\client\HyperVSmoke.ps1')
    foreach($relative in @('src\resource.h','src\dialog.h','build\debug\FileSpacer.exe')) { Copy-PackageFile (Join-Path $ProjectRoot $relative) }
    $runnerFiles=@{(Join-Path $regression 'common\DesktopActivity.cs')=$true}
    Copy-PackageFile (Join-Path $regression 'common\DesktopActivity.cs')
    Build-ManagedBundle 'runner' $runnerFiles
    foreach($entry in $Selected) {
        Write-Host ('PREPARE '+$entry.Relative)
        $dependencies=@{}
        Get-Dependencies $entry.File.FullName $dependencies
        foreach($file in @($dependencies.Keys | Sort-Object)) {
            Copy-PackageFile $file
            if (!$file.EndsWith('.ps1',[StringComparison]::OrdinalIgnoreCase)) { continue }
            $ast=Read-TestAst $file
            foreach($command in $ast.FindAll({param($a) $a -is [Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'Build-NativeTest'},$true)) {
                $parameters=@{}
                for($i=1;$i -lt $command.CommandElements.Count;$i++) {
                    $parameter=$command.CommandElements[$i]
                    if ($parameter -isnot [Management.Automation.Language.CommandParameterAst]) { throw 'Native preparation requires named build arguments.' }
                    $name=$parameter.ParameterName
                    $value=$true
                    if ($i+1 -lt $command.CommandElements.Count -and $command.CommandElements[$i+1] -isnot [Management.Automation.Language.CommandParameterAst]) {
                        $i++;$argument=$command.CommandElements[$i]
                        if ($name -eq 'OutputDirectory') { continue }
                        if ($name -eq 'CaseDirectory') {
                            $directory=Split-Path $file -Parent
                            if ($argument.Extent.Text -eq '$PSScriptRoot') { $value=$directory }
                            elseif ($argument.Extent.Text -match '^\(Join-Path \$PSScriptRoot ''([^'']+)''\)$') { $value=[IO.Path]::GetFullPath((Join-Path $directory $Matches[1])) }
                            else { throw 'Unsupported native CaseDirectory expression.' }
                        } else { $value=$argument.SafeGetValue() }
                    }
                    $parameters[$name]=$value
                }
                $signature=$parameters | ConvertTo-Json -Compress
                $key=$parameters.CaseDirectory+'|'+$parameters.Name
                if ($built.ContainsKey($key)) { if($built[$key] -ne $signature){throw 'Conflicting native build arguments.'};continue }
                $built[$key]=$signature
                $nativeOutput=Join-Path $prepared ('native-'+$native.Count)
                # The existing compiler helper is used as-is on the host.
                $nativeParameters=$parameters.Clone();$nativeParameters.OutputDirectory=$nativeOutput
                $previousRun=$env:FS_REGRESSION_RUN
                try {
                    $env:FS_REGRESSION_RUN=Split-Path $Destination -Parent
                    $exe=& { . (Join-Path $regression 'common\Native.ps1'); Build-NativeTest @nativeParameters }
                } finally { $env:FS_REGRESSION_RUN=$previousRun }
                $description=@{}
                foreach($name in @('ProductionSources','Sqlite','Storage','Extract','Resources','CommonControls','Libraries','Standard','Defines')) { $description[$name]=if($parameters.ContainsKey($name)){@($parameters[$name]) -join '|'}else{''} }
                [void]$native.Add(@{Name=$parameters.Name;CaseDirectory=$parameters.CaseDirectory.Substring($ProjectRoot.Length+1);Parameters=$description;Executable=('native-'+$native.Count+'\'+[IO.Path]::GetFileName($exe));Sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash})
                # Only the executable is needed by the client; compiler output
                # remains with host preparation logs, outside the transferred package.
                $compilerOutput=Join-Path (Split-Path $Destination -Parent) ('native-build-'+($native.Count-1))
                [void][IO.Directory]::CreateDirectory($compilerOutput)
                Get-ChildItem -LiteralPath $nativeOutput -File | Where-Object { $_.Extension -ne '.exe' } | Move-Item -Destination $compilerOutput
            }
        }
        Build-ManagedBundle $entry.Relative $dependencies
    }
    @{Native=@($native);Managed=@($managed)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $prepared 'index.json') -Encoding UTF8
    @{Cases=@($Selected | ForEach-Object {$_.Relative});TimeoutSeconds=$TimeoutSeconds;BuildLocation='Host';HostMachine=$env:COMPUTERNAME;Configuration='Debug'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Destination 'selection.json') -Encoding UTF8
    $files=@(Get-ChildItem -LiteralPath $Destination -Recurse -File | ForEach-Object { @{Path=$_.FullName.Substring($Destination.Length+1);Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
    ConvertTo-Json -InputObject $files -Depth 3 | Set-Content -LiteralPath (Join-Path $Destination 'files.json') -Encoding UTF8
}
