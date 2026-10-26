param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference = 'Stop'
$invariant = [Globalization.CultureInfo]::InvariantCulture
$rows = @(Import-Csv (Join-Path $RunDirectory 'worst-measurements.csv'))
function Get-Percentile($Values, [double]$Fraction) {
    $sorted = @($Values | Sort-Object)
    return $sorted[[Math]::Max(0,[Math]::Ceiling($Fraction*$sorted.Count)-1)]
}
$scenes = @(foreach ($group in ($rows | Group-Object scene)) {
    $first = $group.Group[0]
    $hybrid = @($group.Group | ForEach-Object { [double]::Parse($_.hybrid_us,$invariant) })
    $exact = @($group.Group | ForEach-Object { [double]::Parse($_.exact_us,$invariant) })
    [pscustomobject][ordered]@{
        scene=[int]$first.scene; width=[int]$first.width; height=[int]$first.height
        n=[int]$first.n; target_width=[int]$first.target_width; target_height=[int]$first.target_height
        pattern=$first.pattern; seed=[uint32]$first.seed; path=$first.path
        candidates=([int]$first.candidates_x*[int]$first.candidates_y)
        compressed_cells=[int]$first.compressed_cells
        hybrid_median_us=(Get-Percentile $hybrid 0.5)
        hybrid_max_us=($hybrid | Measure-Object -Maximum).Maximum
        exact_median_us=(Get-Percentile $exact 0.5)
    }
})
$scenes | Export-Csv (Join-Path $RunDirectory 'worst-scenes-summary.csv') -NoTypeInformation -Encoding UTF8
$summary = foreach ($group in ($rows | Group-Object width,height,n,path)) {
    $first=$group.Group[0]
    $hybrid = @($group.Group | ForEach-Object { [double]::Parse($_.hybrid_us,$invariant) })
    $caseScenes = @($scenes | Where-Object { $_.width -eq [int]$first.width -and $_.height -eq [int]$first.height -and $_.n -eq [int]$first.n -and $_.path -eq $first.path })
    $worst = $caseScenes | Sort-Object hybrid_median_us -Descending | Select-Object -First 1
    [pscustomobject][ordered]@{
        width=[int]$first.width; height=[int]$first.height; n=[int]$first.n; path=$first.path
        scenes=$caseScenes.Count; samples=$group.Count
        median_us=(Get-Percentile $hybrid 0.5); p99_us=(Get-Percentile $hybrid 0.99)
        max_us=($hybrid | Measure-Object -Maximum).Maximum
        worst_scene_median_us=$worst.hybrid_median_us; worst_scene=$worst.scene
        worst_scene_pattern=$worst.pattern; worst_target_width=$worst.target_width; worst_target_height=$worst.target_height
        max_candidates=($caseScenes.candidates | Measure-Object -Maximum).Maximum
        max_compressed_cells=($caseScenes.compressed_cells | Measure-Object -Maximum).Maximum
    }
}
$summary | Sort-Object width,height,n,path | Export-Csv (Join-Path $RunDirectory 'worst-summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Where-Object path -eq 'positive-exact' | Sort-Object width,height,n |
    Format-Table width,height,n,scenes,samples,median_us,worst_scene_median_us,p99_us,max_us -AutoSize
Write-Host "Results: $RunDirectory"
