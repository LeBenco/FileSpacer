# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\Native.ps1')
$wanted=@('plain','with spaces','','quote"inside','C:\folder\','backslash\"quote','éλληνικά')
$arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Echo-Arguments.ps1'))+$wanted
$output=Join-Path $RunDirectory 'arguments.stdout.log';$errors=Join-Path $RunDirectory 'arguments.stderr.log'
$code=Invoke-NativeProgram (Join-Path $PSHOME 'powershell.exe') $arguments $output $errors
if($code-ne17){throw ('Native helper exit code differs: '+$code)}
$actual=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String(([IO.File]::ReadAllText($output)).Trim()))|ConvertFrom-Json
if($actual.Count-ne$wanted.Count){throw 'Native helper argument count differs.'}
for($i=0;$i-lt$wanted.Count;$i++){if($actual[$i]-cne$wanted[$i]){throw ('Native helper argument differs at '+$i)}}
if(([IO.File]::ReadAllText($errors)).Trim()-cne'helper stderr'){throw 'Native helper stderr differs.'}
Write-Host 'PASS: spaces, empty/quoted/Unicode arguments, trailing backslashes, stderr and nonzero exit preserved.'
