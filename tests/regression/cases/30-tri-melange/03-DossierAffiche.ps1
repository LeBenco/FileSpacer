# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
$failure=$null
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=Test-Path -LiteralPath $junction
$mixedClass=[guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604'
# The view must belong to the mixed provider and list exactly these names in this order.
function Wait-MixedView([IntPtr]$Window,[string[]]$Names) {
    $expected=$Names -join '|'
    $observed=@{}
    Write-Host ('EXPECT MIXED VIEW '+$expected)
    try { Wait-Ui {
        try { return Use-UiView $Window {param($view)
            $observed.Id=$view.FolderClassId; $observed.Names=$view.OrderedNames()
            ($observed.Id -eq $mixedClass) -and (($observed.Names -join '|') -ceq $expected)
        } }
        catch [InvalidCastException] { return $false }
        catch [Runtime.InteropServices.COMException] { return $false }
    } ('mixed provider listing: '+$expected) 300 } catch {Write-Host ($observed|ConvertTo-Json -Compress);throw}
}
try {
    Initialize-Ui 'mixed-sort-folder'
    Set-UiPreference 'MixedFolderSorting' 1
    $parent=Join-Path $script:UiRoot 'parent';$before=Join-Path $parent 'Before';$after=Join-Path $parent 'After'
    [void][IO.Directory]::CreateDirectory($before)
    # A file sorting before the folder in each view: folders-first order would differ.
    [IO.File]::WriteAllText((Join-Path $parent 'Aside.txt'),'fixture')
    [IO.File]::WriteAllText((Join-Path $before 'a-file.txt'),'fixture')
    [void][IO.Directory]::CreateDirectory((Join-Path $before 'b-folder'))
    [IO.File]::WriteAllText((Join-Path $before 'c-file.txt'),'fixture')
    $parentWindow=Open-UiFolder $parent 2
    Use-UiView $parentWindow {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true)}
    Wait-MixedView $parentWindow @('Aside.txt','Before')
    $window=Open-UiFolder $before 3
    Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true);$view.Select('c-file.txt')}
    Wait-MixedView $window @('a-file.txt','b-folder','c-file.txt')
    Assert-UiSelected $window 'c-file.txt'
    $geometry=[FileSpacerRegression.Native]::Bounds($window)

    # Rename the displayed folder from outside: only filesystem notifications reach the views.
    [IO.Directory]::Move($before,$after)
    $renamed=Track-UiFolder $after
    Assert-Ui ($renamed -eq $window) 'Renaming the displayed folder replaced or duplicated its window.'
    Wait-MixedView $renamed @('a-file.txt','b-folder','c-file.txt')
    Assert-UiSelected $renamed 'c-file.txt'
    $current=[FileSpacerRegression.Native]::Bounds($renamed)
    Assert-Ui ($current.Left -eq $geometry.Left -and $current.Top -eq $geometry.Top -and $current.Width -eq $geometry.Width -and $current.Height -eq $geometry.Height) 'Renaming the displayed folder changed its window geometry.'
    Wait-MixedView $parentWindow @('After','Aside.txt')
    Assert-Ui (@(Find-UiFolder $before).Count -eq 0) 'A window still carries the former folder name.'
    # The retargeted view follows changes under the new name.
    [IO.File]::WriteAllText((Join-Path $after 'd-file.txt'),'notification')
    Wait-MixedView $renamed @('a-file.txt','b-folder','c-file.txt','d-file.txt')
    Write-Host 'PASS: renaming the displayed folder on disk retargets its mixed window, keeps selection and geometry, and updates the mixed parent view.'

    # Delete the displayed folder from outside: its window closes, the parent view drops it.
    Remove-Item -LiteralPath $after -Recurse -Force
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($renamed) } 'window closed after its folder was deleted'
    Wait-MixedView $parentWindow @('Aside.txt')
    Assert-Ui (!(Test-Path -LiteralPath $after)) 'The deleted folder reappeared.'
    Write-Host 'PASS: deleting the displayed folder on disk closes its mixed window and updates the mixed parent view.'
    Close-UiFolder $parentWindow
}catch{$failure=$_;Write-Host $_.Exception.ToString();throw}finally{
    try { Stop-Ui -Failure $failure } finally {
    if(!$registrationExisted -and (Test-Path -LiteralPath $providerKey)){Remove-Item -LiteralPath $providerKey -Recurse -Force}
    if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)} }
}
