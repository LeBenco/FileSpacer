# Host controller contract; no VM or remoting connection is opened.
param()
$ErrorActionPreference='Stop'
# Exercise the actual selection and guard without opening a remoting session.
$path=Join-Path $PSScriptRoot 'Run-HyperVSmoke.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Desktop controller source has parse errors.'}
$selection=@($ast.FindAll({param($node)
    $node -is [Management.Automation.Language.AssignmentStatementAst] -and
    $node.Left -is [Management.Automation.Language.VariableExpressionAst] -and
    $node.Left.VariablePath.UserPath-eq'desktops'
},$true))
if($selection.Count-ne1){throw 'Expected one desktop selection in the controller.'}
$statements=$selection[0].Parent.Statements
$index=[Array]::IndexOf($statements,$selection[0])
$guard=$statements[$index+1]
if($guard-isnot[Management.Automation.Language.IfStatementAst]){throw 'Desktop selection is no longer followed by its guard.'}
$probe=[scriptblock]::Create('param($Guest)'+"`n"+$selection[0].Extent.Text+"`n"+$guard.Extent.Text+"`n"+'$desktops')
function Check-Desktop($Rows,[bool]$Accept) {
    $failure=$null;$result=@()
    try{$result=@(& $probe ([pscustomobject]@{ExplorerSessions=@($Rows)}))}catch{$failure=$_.Exception.Message}
    if($Accept){
        if($failure-or$result.Count-ne1-or$result[0].SessionId-ne2){throw ('Valid single desktop rejected: '+$failure)}
    }elseif($failure-cne'Expected exactly one guest desktop for the connected account.'){
        throw ('Missing or ambiguous desktop was not rejected: '+$failure)
    }
}
$owned=[pscustomobject]@{SessionId=2;BelongsToConnectedAccount=$true}
$same=[pscustomobject]@{SessionId=2;BelongsToConnectedAccount=$true}
$other=[pscustomobject]@{SessionId=3;BelongsToConnectedAccount=$true}
$foreign=[pscustomobject]@{SessionId=5;BelongsToConnectedAccount=$false}
Check-Desktop @($owned) $true
Check-Desktop @($owned,$same) $true
Check-Desktop @($owned,$same,$foreign) $true
Check-Desktop @() $false
Check-Desktop @($foreign) $false
Check-Desktop @($owned,$other) $false
Write-Host 'PASS: Explorer processes share a desktop by session; missing and multiple owned sessions remain blocked.'

$function=$ast.Find({param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Test-ConflictingVMConnect'
},$true)
if (!$function) { throw 'Missing VMConnect target check.' }
. ([scriptblock]::Create($function.Extent.Text))
$target=[guid]'00112233-4455-6677-8899-aabbccddeeff'
$otherId=[guid]'ffeeddcc-bbaa-9988-7766-554433221100'
$exe='"C:\Windows\System32\vmconnect.exe"'
foreach ($test in @(
    @{Arguments=($exe+' "'+$env:COMPUTERNAME+'" "AgentWin10" -G "'+$otherId+'" -C "0"'); Conflict=$false},
    @{Arguments="vmconnect . -G {$otherId}"; Conflict=$false},
    @{Arguments='vmconnect localhost AgentWin11'; Conflict=$false},
    @{Arguments='vmconnect localhost "AgentWin11Pro"'; Conflict=$true},
    @{Arguments="vmconnect . Renamed -G $target"; Conflict=$true},
    @{Arguments='vmconnect remote AgentWin11'; Conflict=$true},
    @{Arguments='vmconnect .'; Conflict=$true},
    @{Arguments='vmconnect . AgentWin10 -unknown'; Conflict=$true},
    @{Arguments=''; Conflict=$true}
)) {
    if ((Test-ConflictingVMConnect $test.Arguments 'AgentWin11Pro' $target) -ne $test.Conflict) {
        throw ('Unexpected VMConnect classification: '+$test.Arguments)
    }
}
Write-Host 'PASS: VMConnect target selection by guest name or ID, including the Hyper-V Manager command line.'
