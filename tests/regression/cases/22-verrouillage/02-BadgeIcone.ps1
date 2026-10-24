# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$source=[IO.File]::ReadAllText((Join-Path $ProjectRoot 'src/ProxyIcon.cpp'))
$start=$source.IndexOf('// Native ICO loading',[StringComparison]::Ordinal)
$end=$source.IndexOf('void ProxyIcon::init()',[StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Production ICO badge helper not found.' }
$generated=Join-Path $RunDirectory 'NativeLockBadgeTests'
[void][IO.Directory]::CreateDirectory($generated)
[IO.File]::WriteAllText((Join-Path $generated 'LockBadge-under-test.inc'),$source.Substring($start,$end-$start))
$test=Build-NativeTest -Name NativeLockBadgeTests -CaseDirectory $PSScriptRoot -OutputDirectory $generated -Resources -Libraries @('Gdi32.lib')
Invoke-TestProgram $test
