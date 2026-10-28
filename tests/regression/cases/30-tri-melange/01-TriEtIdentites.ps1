# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$failure=$null
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=Test-Path -LiteralPath $junction
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
    Use-UiView $window {param($view) $view.Select($selectedName);$view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$false;$view.Position($selectedName,310,170)}
    $positions=Use-UiView $window {param($view) $view.Positions()}
    [void](Show-UiSettings $window);$page=Set-UiPage 1
    Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $false;Press-UiSettings 4
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'native icon positions retained while disabling mixed sorting' 150
    Assert-Ui ((Use-UiView $window {param($view) $view.Selected()}) -eq $selectedName) 'Disabling mixed sorting lost selection.'
    Assert-Ui ((Use-UiView $window {param($view) $view.Focused()}) -ceq $selectedName) 'Disabling mixed sorting lost focus.'
    Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $true;Press-UiSettings 3
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'icon positions retained while enabling mixed sorting' 150
    Close-UiFolder $window
    $window=Open-UiFolder $folder 12
    Wait-Ui {(Use-UiView $window {param($view) $view.Positions()}) -ceq $positions} 'canonical icon positions after reopening' 150
    Assert-Ui ((Get-UiPreference 'MixedFolderSorting') -eq 1) 'The preference did not persist.'
    Write-Host 'PASS: toggling and reopening preserve canonical native item positions and selection.'
    $empty=Join-Path $script:UiRoot 'empty';[void][IO.Directory]::CreateDirectory($empty)
    $emptyWindow=Open-UiFolder $empty 0
    Assert-Ui (Use-UiView $emptyWindow {param($view) $view.ContentVisible}) 'An empty mixed folder retained hidden content.'
    Close-UiFolder $emptyWindow
    Write-Host 'PASS: an empty mixed folder becomes visible without an icon job.'
}catch{$failure=$_;Write-Host $_.Exception.ToString();throw}finally{
    try { Stop-Ui -Failure $failure } finally {
    if(!$registrationExisted -and (Test-Path -LiteralPath $providerKey)){Remove-Item -LiteralPath $providerKey -Recurse -Force}
    if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)} }
}
