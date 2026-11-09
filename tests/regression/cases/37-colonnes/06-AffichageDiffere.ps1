# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot 'Columns.ps1')
function Set-DeferredColumns([IntPtr]$Window,[bool]$Value) {
    [void](Show-UiSettings $Window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_DEFERRED' $Value
    Press-UiSettings 3
    Wait-Ui { (Get-UiPreference 'DetailsColumnsDeferred') -eq [int]$Value } 'deferred view preference stored'
}
$failure=$null
try {
    Initialize-Ui 'column-deferred' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $folder=Join-Path $script:UiRoot 'Details'
    [void][IO.Directory]::CreateDirectory($folder)
    1..100 | ForEach-Object { [IO.File]::WriteAllText((Join-Path $folder ('item-'+$_+'.txt')),'columns') }
    $window=Open-UiFolder $folder 100
    Use-UiView $window {param($view) $view.Mode=4};Wait-UiViewMode $window 4
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    $deferred=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_DETAILS_COLUMNS_DEFERRED'))
    Assert-Ui ($deferred -ne [IntPtr]::Zero -and ![FileSpacerRegression.Native]::IsWindowEnabled($deferred) -and [FileSpacerRegression.Native]::Message($deferred,0xf0,0,0) -eq 0) 'The explicit off fixture must disable the dependent preference.'
    $checkBounds=[FileSpacerRegression.Native]::Bounds($deferred)
    $editBounds=[FileSpacerRegression.Native]::Bounds([FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_FOLDER_WINDOW_WIDTH')))
    Assert-Ui ($checkBounds.Bottom -lt $editBounds.Top) 'Deferred checkbox overlaps the dimension controls.'
    Save-ColumnWindow $script:UiSettings 'settings-layout'
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $true
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($deferred)) 'Enabling columns did not enable the deferred preference.'
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_DEFERRED' $true
    Press-UiSettings 5
    Assert-Ui ((Get-UiPreference 'DetailsColumnsDeferred') -ne 1) 'Cancel saved the deferred preference.'
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $true
    Press-UiSettings 3
    [void](Wait-Columns $window)
    foreach($enabled in @($false,$true)) {
        Set-DeferredColumns $window $enabled
        $before=Wait-Columns $window
        $bounds=[FileSpacerRegression.Native]::Bounds($window)
        $body=Get-ColumnsViewWindow $window
        $initialWidth=[FileSpacerRegression.Native]::ClientBounds($body).Width
        Activate-Ui $window
        $start=New-Object FileSpacerRegression.MousePoint
        $corner=$false
        foreach($offset in @(3,4,5,6,7)) {
            $start.X=$bounds.Right-$offset;$start.Y=$bounds.Bottom-$offset
            $packed=($start.X -band 65535) -bor (($start.Y -band 65535) -shl 16)
            if([FileSpacerRegression.Native]::Message($window,0x84,0,$packed) -eq 17) { $corner=$true;break }
        }
        Assert-Ui $corner 'No native bottom-right sizing border was found.'
        Save-ColumnWindow $window ('before-'+$enabled)
        [FileSpacerRegression.AltDragInput]::BeginConfigured($window,$start,0,0)
        [FileSpacerRegression.AltDragInput]::VerifyHeld($false)
        $point=New-Object FileSpacerRegression.MousePoint
        foreach($delta in @(32,64,96)) {
            $point.X=$start.X+$delta;$point.Y=$start.Y+$delta/2
            [FileSpacerRegression.AltDragInput]::Move($point)
            Wait-Ui { $now=[FileSpacerRegression.Native]::Bounds($window);$now.Width -eq $bounds.Width+$delta -and $now.Height -eq $bounds.Height+$delta/2 } 'real native border resize reaches the requested geometry' 0 15
            if(!$enabled) { Wait-Ui { [FileSpacerRegression.Native]::ClientBounds($body).Width -eq $initialWidth+$delta } 'live viewport follows the native border' 0 15 }
            $width=[FileSpacerRegression.Native]::ClientBounds($body).Width
            Assert-Ui ($width -eq $initialWidth+$(if($enabled){0}else{$delta})) 'The live/deferred viewport did not follow its setting.'
            $during=Read-Columns $window
            if($enabled) { Assert-Ui (($during.Widths -join ',') -eq ($before.Widths -join ',')) 'Deferred columns were recalculated while the button was held.' }
            else { [void](Wait-Columns $window 0 15) }
        }
        [FileSpacerColumnTests.ColumnsUi]::FlushPaint($window)
        Save-ColumnWindow $window ('during-'+$enabled)
        [FileSpacerRegression.AltDragInput]::End()
        Wait-Ui { [FileSpacerRegression.AltDragInput]::Idle($window) } 'native border sizing completed'
        $after=Wait-Columns $window
        Assert-Ui ([FileSpacerRegression.Native]::ClientBounds($body).Width -eq $initialWidth+96) 'The final viewport was not applied after releasing the mouse.'
        for($i=0;$i -lt $after.Widths.Count;++$i) {
            Assert-Ui ([Math]::Abs($after.Widths[$i]-$before.Widths[$i]*$after.Available/$before.Available) -le 2) 'The final columns lost their original proportions.'
        }
        [FileSpacerColumnTests.ColumnsUi]::FlushPaint($window)
        Save-ColumnWindow $window ('after-'+$enabled)
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,$bounds.Left,$bounds.Top,$bounds.Width,$bounds.Height,0x14)) 'Cannot restore the test geometry.'
        [void](Wait-Columns $window)
    }
    Close-UiFolder $window
    $window=Open-UiFolder $folder 100
    [void](Wait-Columns $window)
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Save-ColumnWindow $script:UiSettings 'settings-enabled'
    $deferred=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_DETAILS_COLUMNS_DEFERRED'))
    Assert-Ui ([FileSpacerRegression.Native]::Message($deferred,0xf0,0,0) -eq 1) 'Deferred preference did not survive reopen.'
    Set-UiCheck $page 'IDC_DETAILS_COLUMNS_ENABLED' $false
    Assert-Ui (![FileSpacerRegression.Native]::IsWindowEnabled($deferred)) 'Deferred preference remained editable with the global option off.'
    Press-UiSettings 3
    Assert-Ui ((Get-UiPreference 'DetailsColumnsDeferred') -eq 1) 'Disabling the global feature erased the deferred preference.'
    Wait-ModernColumns $window
    Write-Host 'PASS: default-off dependent preference, Cancel/OK, no overlap, real native mouse sizing in live and deferred modes, unchanged columns during the gesture, final proportional layout, persisted preference and preservation on global disable.'
} catch { $failure=$_;throw } finally {
    try { [FileSpacerRegression.AltDragInput]::End() } finally { Stop-Ui -Failure $failure }
}
