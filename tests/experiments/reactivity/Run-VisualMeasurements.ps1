param([ValidateSet('SelfTest','Details','List','Placement')][string]$Scenario='SelfTest')
$ErrorActionPreference='Stop'
$project=$PSScriptRoot
if(!(Test-Path -LiteralPath (Join-Path $project 'selection.json'))){throw 'Run this launcher from the prepared visual-measurement package.'}
if($env:FILESPACER_VISUAL_SCENARIO){throw 'A visual scenario is already selected in this process.'}
Write-Host 'Leave this workstation free; the test opens and controls isolated FileSpacer and Explorer windows.'
Write-Host 'Results contain CSV timings and before/final BMP images of the measured view.'
$cases=if($Scenario-eq'SelfTest'){@('SelfTest')}else{@('SelfTest',$Scenario)}
$result=1
try{
    foreach($case in $cases){
        $run=Join-Path $project ('results\visual-'+$case.ToLowerInvariant()+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
        Write-Host ('Scenario: '+$case+'; results: '+$run)
        $env:FILESPACER_VISUAL_SCENARIO=$case
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $project 'tests\regression\Run-Regression.ps1') -ProjectRoot $project -Prepared -Pattern '36-reactivity-prototype\09-*' -RunDirectory $run -TimeoutSeconds 900
        $result=$LASTEXITCODE
        $rows=@($(if(Test-Path -LiteralPath $run){Get-ChildItem -LiteralPath $run -Recurse -File -Filter '*-presentation.csv'}) | ForEach-Object{
            $source=$_.FullName.Substring($run.Length+1)
            $name=$_.Name
            foreach($row in @(Import-Csv -LiteralPath $_.FullName)){
                $row|Add-Member -NotePropertyName Source -NotePropertyValue $source
                if($name-match'^(?<app>.+)-(?<operation>(?:select|clear)_\d+|position_16)-(?<repetition>\d+)-presentation.csv$'){
                    $row|Add-Member -NotePropertyName App -NotePropertyValue $Matches.app
                    $row|Add-Member -NotePropertyName Operation -NotePropertyValue $Matches.operation
                    $row|Add-Member -NotePropertyName Repetition -NotePropertyValue $Matches.repetition
                }
                $row
            }
        })
        if($rows.Count){$rows|Export-Csv -LiteralPath (Join-Path $run 'presentation-summary.csv') -NoTypeInformation -Encoding UTF8}
        if($result-ne0){break}
    }
}finally{Remove-Item Env:\FILESPACER_VISUAL_SCENARIO}
if($result-ne0){Write-Host ('Measurement did not pass; retain the complete results directory: '+$run)}
exit $result
