# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\ShellIntegration.ps1')
$testFailure=$null
$observations=New-Object Collections.ArrayList
function Set-WindowState([int]$Command,[bool]$Maximized,[bool]$Minimized=$false) {
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($script:window,0x112,[IntPtr]$Command,[IntPtr]::Zero)) 'Cannot post the native system command.'
    Wait-Ui {
        $style=[FileSpacerRegression.Native]::Style($script:window,-16)
        ($Minimized -or (($style -band 0x01000000) -ne 0) -eq $Maximized) -and
            (($style -band 0x20000000) -ne 0) -eq $Minimized
    } 'native window state' 200
}
function Assert-Layout([string]$Stage,[bool]$Maximized) {
    $client=[FileSpacerRegression.Native]::ClientBounds($script:window)
    $work=[FileSpacerRegression.Native]::WorkArea($script:window)
    $row=[FileSpacerRegression.Native]::Bounds((Get-UiControl $script:window 'ReBarWindow32'))
    $hostBounds=[FileSpacerRegression.Native]::ClientBounds((Get-UiControl $script:window 'FileSpacer Toolbar Host'))
    $commands=[FileSpacerRegression.Native]::Bounds((Get-UiCommandToolbar $script:window))
    $path=[FileSpacerRegression.Native]::Bounds((Get-UiControl $script:window 'FileSpacer Path Bar'))
    $search=[FileSpacerRegression.Native]::Bounds((Get-UiControl $script:window 'FileSpacer Search Box'))
    $captions=@([FileSpacerRegression.Native]::Children($script:window,'Static') | Where-Object {
        [FileSpacerRegression.Native]::GetParent($_) -eq $script:window -and
            ([FileSpacerRegression.Native]::Style($_,-16) -band 0x1f) -eq 0x0d # SS_OWNERDRAW
    })
    Assert-Ui ($captions.Count -eq 1) 'Cannot identify the folder title.'
    $caption=[FileSpacerRegression.Native]::Bounds($captions[0])
    $content=Use-UiView $script:window {param($view) $view.Bounds}
    [void]$observations.Add(@{Stage=$Stage;Dpi=[FileSpacerRegression.Native]::GetDpiForWindow($script:window);
        Client=$client;Work=$work;Row=$row;ToolbarHost=$hostBounds;Commands=$commands;
        Path=$path;Search=$search;Caption=$caption;Content=$content})
    Save-ShellCoverage 'layout.json' @($observations)
    if ($Maximized) { Save-ShellScreenshot $script:window ($Stage+'.png') -WithoutActivation }
    Write-Host "GEOMETRY $Stage : client=$($client.Width), row=$($row.Width), host=$($hostBounds.Width), commands right=$($commands.Right), title top=$($caption.Top), work top=$($work.Top)"
    Assert-Ui ([Math]::Abs($row.Width-$client.Width) -le 1) 'Toolbar row does not fill the client width.'
    Assert-Ui ([Math]::Abs($hostBounds.Width-$row.Width) -le 2) 'Rebar child kept a stale width.'
    Assert-Ui ([Math]::Abs($commands.Right-$hostBounds.Right) -le 1) 'Commands are not aligned with the right edge.'
    Assert-Ui ($path.Width -gt 0 -and $search.Width -gt 0 -and $path.Right -le $search.Left) 'Path and search overlap or have no usable width.'
    Assert-Ui ($content.Top -ge $row.Bottom-1 -and $content.Right -le $client.Right+1) 'Native folder view overlaps the toolbar or exceeds the client area.'
    if ($Maximized) {
        Assert-Ui ($caption.Top -ge $work.Top+1 -and $caption.Bottom -le $row.Top) 'Maximized title lacks a visible top margin or overlaps the toolbar.'
    }
    Write-Host "PASS: $Stage, toolbar fills the window and title stays visible."
}
try {
    Initialize-Ui 'maximized-layout'
    $folder=Join-Path $script:UiRoot 'Folder'
    [void][IO.Directory]::CreateDirectory($folder)
    [IO.File]::WriteAllText((Join-Path $folder 'item.txt'),'fixture')
    $script:window=Open-UiFolder $folder 1
    $dpi=[FileSpacerRegression.Native]::GetDpiForWindow($script:window)
    foreach ($width in @(600,950,1920)) {
        Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($script:window,[IntPtr]::Zero,30,30,
            [int]($width*$dpi/96),[int](500*$dpi/96),0x14)) 'Native resize failed.'
        Assert-Layout ('normal-'+$width) $false
    }
    Set-WindowState 0xf030 $true
    Assert-Layout 'maximized' $true
    Set-WindowState 0xf020 $true $true
    Set-WindowState 0xf120 $true
    Assert-Layout 'restored-maximized' $true
    Close-UiFolder $script:window
    $script:window=Open-UiFolder $folder 1
    Wait-Ui { ([FileSpacerRegression.Native]::Style($script:window,-16) -band 0x01000000) -ne 0 } 'reopened maximized window' 200
    Assert-Layout 'reopened-maximized' $true
    Set-WindowState 0xf120 $false
    Assert-Layout 'restored-normal' $false
} catch { $testFailure=$_;throw }
finally { Stop-Ui -Failure $testFailure }
