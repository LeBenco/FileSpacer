# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot '..\28-apercu-affichage\ViewMenuUi.cs')
$failure=$null
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=Test-Path -LiteralPath $junction
function Wait-Provider([IntPtr]$Window,[bool]$Mixed) {
    $observed=@{}
    Write-Host ('EXPECT PROVIDER mixed='+$Mixed)
    try { Wait-Ui {
        try { return Use-UiView $Window {param($view)
            $observed.Id=$view.FolderClassId; $observed.Mode=$view.Mode; $observed.AutoArrange=$view.AutoArrange; $observed.Visible=$view.ContentVisible
            $observed.Visible -and (($observed.Id -eq [guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604') -eq $Mixed)
        } }
        catch [InvalidCastException] { return $false }
        catch [Runtime.InteropServices.COMException] { return $false }
    } 'provider matching the committed layout' 300 } catch {Write-Host ($observed|ConvertTo-Json -Compress);throw}
}
# The provider roots its namespace in a junction under the application data:
# the junction carries the hidden and system attributes like other Shell data.
function Assert-TestJunctionHidden {
    Wait-Ui { (Test-Path -LiteralPath $junction) -and (((Get-Item -LiteralPath $junction -Force).Attributes -band 6) -eq 6) } 'test junction with hidden and system attributes'
}
function Wait-Order([IntPtr]$Window,[string[]]$Names) {
    $expected=$Names -join '|'
    $observation=@{Error=$null;Names=@()}
    Write-Host ('EXPECT '+$expected)
    Wait-Ui {
        try { $actual=Use-UiView $Window {param($view) $view.OrderedNames()}; $observation.Names=$actual; return (($actual -join '|') -ceq $expected) }
        catch [InvalidCastException] { $observation.Error=$_.Exception; Write-Host ('OBSERVATION '+$_.Exception.ToString()); return $false }
        catch [Runtime.InteropServices.COMException] {
            if($_.Exception.HResult -notin @(-2147023728,-2147483638)){throw};return $false
        }
    } ('exact view order: '+$expected) 100
}
function Expected-Order([bool]$Date,[bool]$Ascending,[bool]$Separated) {
    $ordered=if($Date){@($script:items|Sort-Object { [int]$_.Rank })}else{@($script:items|Sort-Object { [string]$_.Name })}
    if($Separated){$ordered=@($ordered|Where-Object Folder)+@($ordered|Where-Object {!$_.Folder})}
    $names=@($ordered|ForEach-Object Name)
    if(!$Ascending){[array]::Reverse($names)}
    return $names
}
function Toggle-AutoArrange([IntPtr]$Window) {
    Activate-Ui $Window
    [void][FileSpacerViewMenuTests.MenuUi]::Open($Window,(Get-UiCommandToolbar $Window),(Get-UiId 'IDM_VIEW_MENU'))
    $popup=[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)
    $index=-1
    for($i=0;$i-lt$popup.Entries.Count;$i++) {
        if(($popup.Entries[$i].Label -replace '&','') -match '(?i)auto arrange|automatique'){$index=$i;break}
    }
    Assert-Ui ($index-ge0) ('Native auto-arrange command absent: '+(($popup.Entries|ForEach-Object Label)-join '|'))
    [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$index,$Window)
    Wait-Ui {$null-eq[FileSpacerViewMenuTests.MenuUi]::Active($Window)} 'native auto-arrange menu closed'
}
try {
    Initialize-Ui 'mixed-sort'
    $folder=Join-Path $script:UiRoot 'items';[void][IO.Directory]::CreateDirectory($folder)
    $selectedName='item-04-'+[char]0xe9+[char]0x03bb+'.txt'
    $script:items=@()
    for($index=0;$index-lt12;$index++) {
        $name=('item-{0:d2}'-f$index)+$(if($index%2-eq0){'.txt'}else{''})
        if($index-eq4){$name=$selectedName}
        $path=Join-Path $folder $name
        if($index%2){[void][IO.Directory]::CreateDirectory($path)}else{[IO.File]::WriteAllText($path,'fixture')}
        $rank=($index*7)%12
        if($index%2){[IO.Directory]::SetLastWriteTimeUtc($path,([datetime]'2020-01-01Z').AddDays($rank))}else{[IO.File]::SetLastWriteTimeUtc($path,([datetime]'2020-01-01Z').AddDays($rank))}
        $script:items+=[pscustomobject]@{Name=$name;Rank=$rank;Folder=($index%2-eq1)}
    }
    $window=Open-UiFolder $folder 12
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true);$view.Select($selectedName)}
    Wait-Order $window (Expected-Order $false $true $true)
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    $control=[FileSpacerRegression.Native]::GetDlgItem($page,(Get-UiId 'IDC_MIXED_FOLDER_SORTING'))
    Assert-Ui ([FileSpacerRegression.Native]::IsWindowEnabled($control)) 'Mixed sorting preference unavailable.'
    Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $true
    Press-UiSettings 4
    Wait-Order $window (Expected-Order $false $true $false)
    Assert-Ui ((Use-UiView $window {param($view) $view.Selected()}) -eq $selectedName) 'Enabling mixed sorting lost the real selected item.'
    Assert-Ui ((Use-UiView $window {param($view) $view.Focused()}) -ceq $selectedName) 'Enabling mixed sorting lost the focused native identity.'

    Press-UiSettings 3
    foreach($mode in @(4,3,1)) {
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize($mode,48);$view.AutoArrange=$true}
        foreach($date in @($false,$true)) {foreach($ascending in @($false,$true)) {
            Use-UiView $window {param($view) $view.SortBy($date,$ascending)}
            Wait-Order $window (Expected-Order $date $ascending $false)
        }}
    }
    Write-Host 'PASS: real FileSpacer mixes folders and files by name/date in Details/List/Icons with Windows rendering.'
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true)}
    $edit=Get-UiSearchEdit $window;$submit=Get-UiSearchSubmit $window
    [FileSpacerRegression.Native]::SetText($edit,$selectedName)
    [void][FileSpacerRegression.Native]::Message($submit,0xf5,0,0)
    Wait-Order $window @($selectedName)
    [FileSpacerRegression.Native]::SetText($edit,'')
    [void][FileSpacerRegression.Native]::Message($submit,0xf5,0,0)
    Wait-Order $window (Expected-Order $false $true $false)
    Write-Host 'PASS: the existing name filter resolves a Unicode item through the mixed provider.'
    [IO.File]::WriteAllText((Join-Path $folder 'added.txt'),'notification')
    $names=@($script:items|ForEach-Object Name)+@('added.txt')
    Wait-Order $window @($names|Sort-Object)
    [IO.File]::Move((Join-Path $folder 'added.txt'),(Join-Path $folder 'renamed.txt'))
    $names=@($script:items|ForEach-Object Name)+@('renamed.txt')
    Wait-Order $window @($names|Sort-Object)
    [IO.File]::Delete((Join-Path $folder 'renamed.txt'))
    Wait-Order $window (Expected-Order $false $true $false)
    Write-Host 'PASS: actual filesystem creation, rename and deletion update the mixed view without refresh commands.'
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$false}
    Wait-Provider $window $false
    Use-UiView $window {param($view) $view.Select($selectedName);$view.Select('item-03',$true);$view.Position($selectedName,310,170);$view.Position('item-03',100,270)}
    $positions=Use-UiView $window {param($view) $view.Positions()}
    Close-UiFolder $window
    $window=Open-UiFolder $folder 12
    Wait-Provider $window $false
    Assert-Ui ((Use-UiView $window {param($view) $view.Positions()}) -ceq $positions) 'Native free placement did not survive reopening.'
    Use-UiView $window {param($view) $view.Select($selectedName);$view.Select('item-03',$true)}
    $selection=(Use-UiView $window {param($view) $view.Selected()}) -join '|'
    $focus=Use-UiView $window {param($view) $view.Focused()}
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'free placement before switching' 500
    foreach($mode in @(4,3,1)) {
        if($mode-eq1){Toggle-AutoArrange $window}
        else {Use-UiView $window {param($view) $view.SetViewModeAndIconSize($mode,48);$view.SortBy($false,$true)}}
        Wait-Provider $window $true
        Assert-TestJunctionHidden
        Wait-Order $window (Expected-Order $false $true $false)
        Assert-Ui (((Use-UiView $window {param($view) $view.Selected()}) -join '|') -ceq $selection) 'Switching to a sorted provider lost the multiselection.'
        Assert-Ui ((Use-UiView $window {param($view) $view.Focused()}) -ceq $focus) 'Switching to a sorted provider lost focus.'
        if($mode-eq1){Toggle-AutoArrange $window}
        else {Use-UiView $window {param($view) $view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$false}}
        Wait-Provider $window $false
        $actualPositions=Use-UiView $window {param($view) $view.Positions()}
        Assert-Ui ($actualPositions -ceq $positions) ('Returning to the native free layout lost icon positions. Expected: '+$positions+' Actual: '+$actualPositions)
        Assert-Ui (((Use-UiView $window {param($view) $view.Selected()}) -join '|') -ceq $selection) 'Returning to the native free layout lost the multiselection.'
        Assert-Ui ((Use-UiView $window {param($view) $view.Focused()}) -ceq $focus) 'Returning to the native free layout lost focus.'
    }
    Write-Host 'PASS: free layout uses the native provider; Details/List/arranged Icons use mixed sorting; transitions retain all positions, multiselection and focus.'
    Use-UiView $window {param($view) $view.Select($selectedName)}
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $false;Press-UiSettings 4
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'native icon positions retained while disabling mixed sorting' 150
    Assert-Ui ((Use-UiView $window {param($view) $view.Selected()}) -eq $selectedName) 'Disabling mixed sorting lost selection.'
    Assert-Ui ((Use-UiView $window {param($view) $view.Focused()}) -ceq $selectedName) 'Disabling mixed sorting lost focus.'
    Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $true;Press-UiSettings 3
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'icon positions retained while enabling mixed sorting' 150
    Close-UiFolder $window
    $window=Open-UiFolder $folder 12
    Wait-Provider $window $false
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'canonical icon positions after reopening' 150
    Assert-Ui ((Get-UiPreference 'MixedFolderSorting') -eq 1) 'The preference did not persist.'
    Write-Host 'PASS: toggling and reopening preserve canonical native item positions and selection.'
    $empty=Join-Path $script:UiRoot 'empty';[void][IO.Directory]::CreateDirectory($empty)
    $emptyWindow=Open-UiFolder $empty 0
    Assert-Ui (Use-UiView $emptyWindow {param($view) $view.ContentVisible}) 'An empty mixed folder retained hidden content.'
    Use-UiView $emptyWindow {param($view) $view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$false}
    Wait-Provider $emptyWindow $false
    Use-UiView $emptyWindow {param($view) $view.SetViewModeAndIconSize(4,48)}
    Wait-Provider $emptyWindow $true
    Close-UiFolder $emptyWindow
    Write-Host 'PASS: an empty mixed folder becomes visible without an icon job.'
}catch{$failure=$_;Write-Host $_.Exception.ToString();throw}finally{
    try { Stop-Ui -Failure $failure } finally {
    if(!$registrationExisted -and (Test-Path -LiteralPath $providerKey)){Remove-Item -LiteralPath $providerKey -Recurse -Force}
    if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)} }
}
