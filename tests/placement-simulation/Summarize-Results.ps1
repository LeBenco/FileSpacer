param([Parameter(Mandatory=$true)][string]$RunDirectory)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$rows = @(Import-Csv (Join-Path $RunDirectory 'measurements.csv'))
function Get-Percentile($Values, [double]$Fraction) {
    $sorted = @($Values | Sort-Object)
    return $sorted[[Math]::Max(0, [Math]::Ceiling($Fraction * $sorted.Count) - 1)]
}
$summary = foreach ($group in ($rows | Group-Object width,height,pattern,n,method)) {
    $first = $group.Group[0]
    $cold = @($group.Group | ForEach-Object { [double]::Parse($_.cold_us,$culture) })
    $after = @($group.Group | ForEach-Object { [double]::Parse($_.after_update_us,$culture) })
    $warm = @($group.Group | ForEach-Object { [double]::Parse($_.warm_us,$culture) })
    $setup = @($group.Group | ForEach-Object { [double]::Parse($_.setup_us,$culture) })
    $update = @($group.Group | ForEach-Object { [double]::Parse($_.update_us,$culture) })
    [pscustomobject][ordered]@{
        width=[int]$first.width; height=[int]$first.height; pattern=$first.pattern
        n=[int]$first.n; method=$first.method; scenes=$group.Count
        setup_p50_us=(Get-Percentile $setup 0.5)
        cold_p50_us=(Get-Percentile $cold 0.5)
        cold_p90_us=(Get-Percentile $cold 0.9)
        warm_p50_us=(Get-Percentile $warm 0.5)
        update_p50_us=(Get-Percentile $update 0.5)
        after_update_p50_us=(Get-Percentile $after 0.5)
        after_update_p90_us=(Get-Percentile $after 0.9)
        max_estimated_peak_bytes=($group.Group | ForEach-Object { [long]$_.estimated_peak_bytes } | Measure-Object -Maximum).Maximum
        suboptimal=(@($group.Group | Where-Object { [long]$_.extra_area -gt 0 })).Count
        free_missed=(@($group.Group | Where-Object { $_.missed_free -eq '1' })).Count
        free_exists=(@($group.Group | Where-Object { $_.optimal_area -eq '0' })).Count
    }
}
$summary | Sort-Object width,height,pattern,n,method | Export-Csv (Join-Path $RunDirectory 'summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Where-Object { $_.n -in @(10,60,200) -and $_.method -in @('grid10','grid1','exact','exact-free') } |
    Sort-Object width,height,pattern,n,method |
    Format-Table width,height,pattern,n,method,cold_p50_us,after_update_p50_us,free_missed -AutoSize
Write-Host "Summary: $(Join-Path $RunDirectory 'summary.csv')"
