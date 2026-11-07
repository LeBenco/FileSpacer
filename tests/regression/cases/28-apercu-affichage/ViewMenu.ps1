. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -Path (Join-Path $PSScriptRoot 'ViewMenuUi.cs') -ReferencedAssemblies ([FileSpacerRegression.Native].Assembly.Location)
function Read-PreviewMode([IntPtr]$Window) {
    $result=@{Value=$null}
    Wait-Ui {
        $view=$null
        try {
            $view=Get-UiView $Window
            if($null-eq$view){return $false}
            $result.Value=@{Mode=$view.Mode;Size=$view.IconSize}
            return $true
        } catch {
            $error=$_.Exception
            while($error.InnerException){$error=$error.InnerException}
            # A renderer change briefly removes the registered view service.
            if($error.HResult -notin @(-2147467262,-2147467259,-2147417848,-2147483638)){throw}
            return $false
        } finally {if($null-ne$view){$view.Dispose()}}
    } 'registered native view available after a representation change'
    return $result.Value
}
function Wait-PreviewMode([IntPtr]$Window,$Expected) {
    Wait-Ui {$now=Read-PreviewMode $Window; $now.Mode -eq $Expected.Mode -and $now.Size -eq $Expected.Size} 'native display mode and icon size' 150
}
function Open-PreviewMenu([IntPtr]$Window) {
    Activate-Ui $Window
    return [FileSpacerViewMenuTests.MenuUi]::Open($Window,(Get-UiCommandToolbar $Window),(Get-UiId 'IDM_VIEW_MENU'))
}
function Wait-PreviewMenuClosed([IntPtr]$Window) {
    Wait-Ui {$null -eq [FileSpacerViewMenuTests.MenuUi]::Active($Window)} 'native display menu closed'
}
function New-PreviewFolder([int]$Count=300) {
    $folder=Join-Path $script:UiRoot 'Preview'
    [void][IO.Directory]::CreateDirectory($folder)
    for($i=0;$i -lt $Count;++$i){[IO.File]::WriteAllText((Join-Path $folder ('item-{0:D4}.txt' -f $i)),'fixture')}
    return $folder
}
function Reset-PreviewMode([IntPtr]$Window) {
    Use-UiView $Window {param($view) $view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$false}
    Wait-PreviewMode $Window @{Mode=1;Size=48}
}
function Get-PreviewModes([IntPtr]$Window) {
    Set-UiPreference 'ViewMenuPreviewEnabled' 0
    $modes=@{}
    foreach($index in @(0,3,4,5)) {
        $before=Read-PreviewMode $Window
        $popup=Open-PreviewMenu $Window
        [FileSpacerViewMenuTests.MenuUi]::Choose($popup,$index,$Window)
        Wait-PreviewMenuClosed $Window
        Wait-Ui {$now=Read-PreviewMode $Window; $now.Mode -ne $before.Mode -or $now.Size -ne $before.Size} 'native command changed the representation' 150
        $modes[$index]=Read-PreviewMode $Window
    }
    @($modes.GetEnumerator() | ForEach-Object { [pscustomobject]@{Index=$_.Key;Mode=$_.Value.Mode;Size=$_.Value.Size} })|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'native-modes.json') -Encoding UTF8
    Reset-PreviewMode $Window
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($script:UiSettingsKey,$true)
    try{$key.DeleteValue('ViewMenuPreviewEnabled',$false)}finally{$key.Dispose()}
    return $modes
}
function Read-PreviewState([IntPtr]$Window) {
    return Use-UiView $Window {param($view) @{Selected=(@($view.Selected())|Sort-Object) -join ';';Focus=$view.Focused();Positions=$view.Positions()}}
}
function Set-PreviewState([IntPtr]$Window) {
    Use-UiView $Window {
        param($view)
        $names=@($view.Names()|Sort-Object)
        $view.Select($names[0]);$view.Select($names[2],$true);$view.Select($names[4],$true)
        $view.Position($names[0],100,140);$view.Position($names[2],280,220);$view.Position($names[4],100,1800)
    }
    Start-Sleep -Milliseconds 250
    return Read-PreviewState $Window
}
function Assert-PreviewState([IntPtr]$Window,$Expected) {
    Wait-Ui {
        $actual=Read-PreviewState $Window
        $actual.Selected -eq $Expected.Selected -and $actual.Focus -eq $Expected.Focus -and $actual.Positions -eq $Expected.Positions
    } 'original selection, focus and every icon position' 250
}
function Assert-PreviewDisabled([IntPtr]$Window,$Expected) {
    $popup=Open-PreviewMenu $Window
    [FileSpacerViewMenuTests.MenuUi]::Hover($popup,0)
    $watch=[Diagnostics.Stopwatch]::StartNew()
    while($watch.ElapsedMilliseconds -lt 600) {
        $actual=Read-PreviewMode $Window
        Assert-Ui ($actual.Mode -eq $Expected.Mode -and $actual.Size -eq $Expected.Size) 'Disabled preview changed the representation on hover.'
        Start-Sleep -Milliseconds 40
    }
    return $popup
}
function Save-PreviewSettingsImage([IntPtr]$Sheet) {
    Add-Type -AssemblyName System.Drawing
    Start-Sleep -Milliseconds 400 # Let the native dialog opening animation complete before its screenshot.
    $bounds=[FileSpacerRegression.Native]::Bounds($Sheet)
    $bitmap=New-Object Drawing.Bitmap($bounds.Width,$bounds.Height)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left,$bounds.Top,0,0,$bitmap.Size)
        $bitmap.Save((Join-Path $env:FS_REGRESSION_RUN 'display-settings.png'),[Drawing.Imaging.ImageFormat]::Png)
    }finally{$graphics.Dispose();$bitmap.Dispose()}
}
