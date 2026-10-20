# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$testFailure=$null
function Inside($Inner,$Outer,[string]$Name) {
    Assert-Ui ($Inner.Width -gt 0 -and $Inner.Height -gt 0) "$Name has no usable area."
    Assert-Ui ($Inner.Left -ge $Outer.Left-1 -and $Inner.Top -ge $Outer.Top-1 -and
        $Inner.Right -le $Outer.Right+1 -and $Inner.Bottom -le $Outer.Bottom+1) "$Name extends beyond its parent."
}
function Separate($First,$Second,[string]$Name) {
    $overlap=$First.Left -lt $Second.Right -and $Second.Left -lt $First.Right -and
        $First.Top -lt $Second.Bottom -and $Second.Top -lt $First.Bottom
    Assert-Ui (!$overlap) "$Name overlaps."
}
function Configure-Layout([bool]$Path,[bool]$Navigation,[bool]$Commands,[bool]$Toolbar=$true) {
    [void](Show-UiSettings $window); $page=Set-UiPage 1
    # Enable the master first, so its dependent checkboxes accept actual clicks.
    Set-UiCheck $page 'IDC_TOOLBAR_ENABLED' $true
    foreach ($name in @('IDC_QUICK_ACCESS_ENABLED','IDC_UP_BUTTON_ENABLED')) { Set-UiCheck $page $name $Navigation }
    foreach ($name in @('IDC_REFRESH_BUTTON_ENABLED','IDC_VIEW_BUTTON_ENABLED')) { Set-UiCheck $page $name $Commands }
    Set-UiCheck $page 'IDC_PATH_BAR_ENABLED' $Path
    Set-UiCheck $page 'IDC_TOOLBAR_ENABLED' $Toolbar
    Press-UiSettings 3
    Assert-UiVisible $window 'ReBarWindow32' $Toolbar
    Assert-UiVisible $window 'FileSpacer Path Bar' ($Toolbar -and $Path)
}
function Snapshot-Layout([bool]$Path,[bool]$Commands) {
    $toolbar=Get-UiCommandToolbar $window
    $search=Get-UiControl $window 'FileSpacer Search Box'
    $pathBar=Get-UiControl $window 'FileSpacer Path Bar'
    $toolbarHost=[FileSpacerRegression.Native]::GetParent($toolbar)
    $grips=@([FileSpacerRegression.Native]::Children($window,'Static') | Where-Object {
        [FileSpacerRegression.Native]::GetParent($_) -eq $toolbarHost -and
        ([FileSpacerRegression.Native]::Style($_,-16) -band 0x100) -ne 0 # SS_NOTIFY: resize grip, not the hidden menu owner.
    })
    Assert-Ui ($grips.Count -eq 1) 'Expected one search resize grip in the application toolbar host.'
    Wait-Ui { [FileSpacerRegression.Native]::IsWindowVisible($grips[0]) -eq $Path } 'search grip follows path visibility' 200
    $searchBounds=[FileSpacerRegression.Native]::Bounds($search)
    Inside $searchBounds ([FileSpacerRegression.Native]::ClientBounds($toolbar)) 'Search box'
    Inside ([FileSpacerRegression.Native]::Bounds((Get-UiSearchEdit $window))) ([FileSpacerRegression.Native]::ClientBounds($search)) 'Search edit'
    $submit=Get-UiControl $search 'Button'
    Inside ([FileSpacerRegression.Native]::Bounds($submit)) ([FileSpacerRegression.Native]::ClientBounds($search)) 'Search submit button'
    Separate ([FileSpacerRegression.Native]::Bounds((Get-UiSearchEdit $window))) ([FileSpacerRegression.Native]::Bounds($submit)) 'Search edit / submit button'
    if ($Path) {
        Inside ([FileSpacerRegression.Native]::Bounds($pathBar)) ([FileSpacerRegression.Native]::ClientBounds($toolbarHost)) 'Path bar'
        Separate ([FileSpacerRegression.Native]::Bounds($pathBar)) $searchBounds 'Path bar / search'
    }
    foreach ($name in @('IDM_REFRESH','IDM_VIEW_MENU')) {
        $id=Get-UiId $name
        Assert-Ui (([FileSpacerRegression.Native]::Message($toolbar,0x40c,$id,0) -ne 0) -eq (!$Commands)) "$name hidden state differs from the preference."
        if ($Commands) { Separate ([FileSpacerRegression.Native]::ToolbarButton($toolbar,$id)) $searchBounds "$name / search" }
    }
    $content=Use-UiView $window {param($view) $view.Bounds}
    $row=[FileSpacerRegression.Native]::Bounds((Get-UiControl $window 'ReBarWindow32'))
    $status=[FileSpacerRegression.Native]::Bounds((Get-UiControl $window 'msctls_statusbar32'))
    Assert-Ui ($content.Top -ge $row.Bottom-1 -and $content.Bottom -le $status.Top+1) 'Folder view overlaps the toolbar or status bar.'
    return @{Search=$searchBounds; Path=[FileSpacerRegression.Native]::Bounds($pathBar); Content=$content}
}
try {
    Initialize-Ui 'layout'
    $folder=Join-Path $script:UiRoot 'Folder'
    [void](New-Item -ItemType Directory -Path $folder)
    [IO.File]::WriteAllText((Join-Path $folder 'keep.txt'),'fixture')
    $window=Open-UiFolder $folder 1
    $dpi=[FileSpacerRegression.Native]::GetDpiForWindow($window)
    Assert-Ui ($dpi -gt 0) 'Cannot obtain the actual window DPI.'
    $standard=@{}
    foreach ($configuration in @('standard','path-hidden','controls-hidden')) {
        $path=$configuration -ne 'path-hidden'
        $controls=$configuration -ne 'controls-hidden'
        Configure-Layout $path $controls $controls
        foreach ($widthDip in @(550,900)) {
            $width=[int][Math]::Round($widthDip*$dpi/96.0)
            $height=[int][Math]::Round(500*$dpi/96.0)
            Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,0,0,$width,$height,0x16)) 'Native resize failed.'
            # Wait for measured positions to settle, rather than waiting an arbitrary delay.
            $script:layoutLast=''; $script:layoutSince=[DateTime]::UtcNow
            Wait-Ui {
                $r=[FileSpacerRegression.Native]::Bounds((Get-UiControl $window 'FileSpacer Search Box'))
                $signature="$($r.Left),$($r.Top),$($r.Right),$($r.Bottom)"
                if ($signature -ne $script:layoutLast) { $script:layoutLast=$signature; $script:layoutSince=[DateTime]::UtcNow; return $false }
                return ([DateTime]::UtcNow-$script:layoutSince).TotalMilliseconds -ge 250
            } 'settled search geometry' 250
            $snapshot=Snapshot-Layout $path $controls
            if ($configuration -eq 'standard') { $standard[$widthDip]=$snapshot }
            elseif ($configuration -eq 'path-hidden') {
                Assert-Ui ($snapshot.Search.Width -gt $standard[$widthDip].Search.Width) 'Hiding the path did not give its space to search.'
            } else {
                Assert-Ui ($snapshot.Path.Left -lt $standard[$widthDip].Path.Left -and
                    $snapshot.Path.Width -gt $standard[$widthDip].Path.Width) 'Hidden navigation/command controls left their old spaces reserved.'
            }
            Write-Host "PASS: $configuration, width=$widthDip DIP, actual DPI=$dpi; usable controls and no overlap."
        }
    }
    Configure-Layout $true $true $true
    $before=Snapshot-Layout $true $true
    Configure-Layout $true $true $true $false
    Assert-UiVisible $window 'FileSpacer Search Box' $false
    $content=Use-UiView $window {param($view) $view.Bounds}
    Assert-Ui ($content.Top -lt $before.Content.Top) 'Hiding the toolbar did not release its vertical space to the folder view.'
    Configure-Layout $true $true $true
    [void](Snapshot-Layout $true $true)
    Write-Host 'PASS: toolbar hide/show releases and restores its space without unusable controls.'
} catch { $testFailure=$_; throw }
finally { Stop-Ui -Failure $testFailure }
