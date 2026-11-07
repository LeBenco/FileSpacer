# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class PlacementSnap {
    [StructLayout(LayoutKind.Sequential)] struct Keyboard {
        public ushort Key,Scan; public uint Flags,Time; public UIntPtr Extra;
    }
    [StructLayout(LayoutKind.Explicit,Size=32)] struct InputUnion {
        [FieldOffset(0)] public Keyboard Keyboard;
    }
    [StructLayout(LayoutKind.Sequential)] struct Input { public uint Type; public InputUnion Data; }
    [DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint count,Input[] inputs,int size);
    [DllImport("user32.dll")] public static extern bool IsWindowArranged(IntPtr window);
    static Input Key(ushort key,uint flags) {
        var input=new Input(); input.Type=1; input.Data.Keyboard.Key=key;
        input.Data.Keyboard.Flags=flags; return input;
    }
    public static void Left() {
        var inputs=new Input[]{Key(0x5b,0),Key(0x25,1),Key(0x25,3),Key(0x5b,2)};
        if(SendInput(4,inputs,Marshal.SizeOf(typeof(Input)))!=4) {
            int error=Marshal.GetLastWin32Error();
            SendInput(2,new Input[]{Key(0x25,3),Key(0x5b,2)},Marshal.SizeOf(typeof(Input)));
            throw new Win32Exception(error,"Snap input incomplete.");
        }
    }
}
'@
$testFailure=$null
$script:PlacementConfirmation=[IntPtr]::Zero
function Set-PlacementRect([IntPtr]$Window,[int]$X,[int]$Y) {
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$X,$Y,$script:PlacementWidth,$script:PlacementHeight,0x14)) 'Fixture positioning failed.'
    Wait-Ui { $r=[FileSpacerRegression.Native]::Bounds($Window); $r.Left -eq $X -and $r.Top -eq $Y -and $r.Width -eq $script:PlacementWidth -and $r.Height -eq $script:PlacementHeight } 'fixture geometry'
}
function Overlap-Area($First,$Second) {
    return [Math]::Max(0,[Math]::Min($First.Right,$Second.Right)-[Math]::Max($First.Left,$Second.Left))*[Math]::Max(0,[Math]::Min($First.Bottom,$Second.Bottom)-[Math]::Max($First.Top,$Second.Top))
}
try {
    Initialize-Ui 'smart-placement' -FixtureParent (Join-Path $ProjectRoot 'build')
    # Earlier preference cases may have applied this value. Exercise the absent
    # value default independently, before opening any window in this scenario.
    $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($script:UiSettingsKey,$true)
    try { $key.DeleteValue('SmartWindowPlacement',$false) } finally { $key.Dispose() }
    Set-UiPreference 'FolderWindow2Width' 400; Set-UiPreference 'FolderWindow2Height' 300
    Set-UiPreference 'KeepSourceWindowOpen' 0
    $parentPath=Join-Path $script:UiRoot 'Parent'; $childPath=Join-Path $parentPath 'Child'
    $otherPath=Join-Path $script:UiRoot 'Other'; $thirdPath=Join-Path $script:UiRoot 'Third'
    foreach ($path in @($childPath,$otherPath,$thirdPath)) { [void][IO.Directory]::CreateDirectory($path) }
    $parent=Open-UiFolder $parentPath 1
    $initial=[FileSpacerRegression.Native]::Bounds($parent)
    $script:PlacementWidth=$initial.Width; $script:PlacementHeight=$initial.Height
    $work=[FileSpacerRegression.Native]::WorkArea($parent)
    Assert-Ui ($work.Width -ge 2*$script:PlacementWidth+60 -and $work.Height -ge 2*$script:PlacementHeight+60) 'Placement fixture requires room for two default windows on each axis at the current DPI.'
    Set-PlacementRect $parent $work.Left $work.Top
    $other=Open-UiFolder $otherPath 0
    $parentRect=[FileSpacerRegression.Native]::Bounds($parent); $otherRect=[FileSpacerRegression.Native]::Bounds($other)
    Assert-Ui ((Overlap-Area $parentRect $otherRect) -eq 0) 'A new window overlaps another process despite free space.'
    Write-Host 'PASS: first placement avoids an existing window in another process.'

    [void](Show-UiSettings $other); $general=Set-UiPage 0
    $smart=[FileSpacerRegression.Native]::GetDlgItem($general,(Get-UiId 'IDC_SMART_WINDOW_PLACEMENT'))
    Assert-Ui ($smart -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::Message($smart,0xf0,0,0) -eq 1) 'Smart placement is not enabled by default in General.'
    $placementLabel=[FileSpacerRegression.Native]::Text($smart).Replace('&','')
    $frenchLabel='Placer les nouvelles fen'+[char]0xea+'tres sur un espace libre'
    Assert-Ui ($placementLabel -in @('Place new windows in free space',$frenchLabel)) 'The placement option label is incorrect.'
    Assert-Ui ([FileSpacerRegression.Native]::GetDlgItem($general,(Get-UiId 'IDC_RESET_FOLDER_STATE')) -eq [IntPtr]::Zero) 'Maintenance remains in General.'
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $false
    Press-UiSettings 5
    Assert-Ui ($null -eq (Get-UiPreference 'SmartWindowPlacement')) 'Cancel saved the pending placement option.'
    [void](Show-UiSettings $other); $general=Set-UiPage 0
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $false
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'SmartWindowPlacement') -eq 0 } 'placement disabled by Apply'
    Set-UiCheck $general 'IDC_SMART_WINDOW_PLACEMENT' $true
    Press-UiSettings 4
    Wait-Ui { (Get-UiPreference 'SmartWindowPlacement') -eq 1 } 'placement reenabled by Apply'
    $about=Set-UiPage 4
    $maintenance=[FileSpacerRegression.Native]::GetDlgItem($about,(Get-UiId 'IDC_RESET_FOLDER_STATE'))
    Assert-Ui ($maintenance -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindowVisible($maintenance)) 'Maintenance is missing from About.'
    $button=[FileSpacerRegression.Native]::Bounds($maintenance); $client=[FileSpacerRegression.Native]::ClientBounds($about)
    Assert-Ui ($button.Left -ge $client.Left -and $button.Right -le $client.Right -and $button.Bottom -le $client.Bottom) 'Maintenance is clipped.'
    Assert-Ui ([FileSpacerRegression.Native]::PostMessage($about,0x111,[IntPtr](Get-UiId 'IDC_RESET_FOLDER_STATE'),$maintenance)) 'Cannot invoke maintenance confirmation from About.'
    $dialogProcessId=[uint32]0; [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($other,[ref]$dialogProcessId)
    Add-Type -Path (Join-Path $PSScriptRoot '..\01-stockage-sqlite\StorageRecoveryUi.cs')
    $confirmationTitles=@([MutexRecoveryNative]::Messages($script:UiExecutable,(Get-UiId 'IDS_CONFIRM_CAPTION')))
    $confirmation=@{Window=[IntPtr]::Zero}
    Wait-Ui {
        $dialogs=@([FileSpacerRegression.Native]::Top('#32770',[NullString]::Value,$dialogProcessId) | Where-Object { $_ -ne $script:UiSettings -and [FileSpacerRegression.Native]::IsWindowEnabled($_) -and [FileSpacerRegression.Native]::Text($_) -in $confirmationTitles })
        if ($dialogs.Count -ne 1) { return $false }; $confirmation.Window=$dialogs[0]; $script:PlacementConfirmation=$dialogs[0]; return $true
    } 'maintenance confirmation'
    [void][FileSpacerRegression.Native]::Message($confirmation.Window,0x466,7,0)
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($confirmation.Window) } 'maintenance confirmation declined'
    Assert-Ui ([FileSpacerRegression.Native]::IsWindow($parent) -and [FileSpacerRegression.Native]::IsWindow($other)) 'Declining maintenance closed folder windows.'
    Press-UiSettings 5
    Write-Host 'PASS: default placement option, Cancel/Apply, maintenance moved to About without clipping.'

    $third=Open-UiFolder $thirdPath 0
    $reference=[FileSpacerRegression.Native]::Bounds($third)
    Assert-Ui ((Overlap-Area $reference $parentRect) -eq 0 -and (Overlap-Area $reference $otherRect) -eq 0) 'Reference placement is not free.'
    Close-UiFolder $third
    foreach ($mode in @('maximized','minimized','maximized-minimized','snapped')) {
        if ($mode -in @('maximized','maximized-minimized')) {
            [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf030,0)
            Wait-Ui { ([FileSpacerRegression.Native]::Style($parent,-16) -band 0x01000000) -ne 0 } 'parent maximized'
        }
        if ($mode -in @('minimized','maximized-minimized')) {
            [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf020,0)
            Wait-Ui { ([FileSpacerRegression.Native]::Style($parent,-16) -band 0x20000000) -ne 0 } 'parent minimized'
        }
        if ($mode -eq 'snapped') {
            Activate-Ui $parent
            [PlacementSnap]::Left()
            Wait-Ui { [PlacementSnap]::IsWindowArranged($parent) } 'parent snapped by Windows'
        }
        $probePath=Join-Path $script:UiRoot ('Third-'+$mode)
        [void][IO.Directory]::CreateDirectory($probePath)
        $third=Open-UiFolder $probePath 0
        $actual=[FileSpacerRegression.Native]::Bounds($third)
        Assert-Ui ($actual.Left -eq $reference.Left -and $actual.Top -eq $reference.Top -and $actual.Width -eq $reference.Width -and $actual.Height -eq $reference.Height) ('Placement changed with parent state: '+$mode)
        Close-UiFolder $third
        [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf120,0)
        if (([FileSpacerRegression.Native]::Style($parent,-16) -band 0x01000000) -ne 0) {
            [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf120,0)
        }
        Wait-Ui {
            $r=[FileSpacerRegression.Native]::Bounds($parent)
            return (([FileSpacerRegression.Native]::Style($parent,-16) -band 0x21000000) -eq 0 -and ![PlacementSnap]::IsWindowArranged($parent) -and $r.Left -eq $parentRect.Left -and $r.Top -eq $parentRect.Top -and $r.Width -eq $parentRect.Width -and $r.Height -eq $parentRect.Height)
        } ('parent restored after '+$mode)
        Write-Host ('PASS: '+$mode+' parent contributes the same normal geometry across processes.')
    }
    Set-PlacementRect $parent ($work.Left+20) ($work.Top+20)
    Set-PlacementRect $other ($work.Left+$script:PlacementWidth+40) ($work.Top+$script:PlacementHeight+40)
    $sourceRect=[FileSpacerRegression.Native]::Bounds($parent)
    [void][FileSpacerRegression.Native]::Message($parent,0x112,0xf030,0)
    Wait-Ui { ([FileSpacerRegression.Native]::Style($parent,-16) -band 0x01000000) -ne 0 } 'replacement source maximized'
    Activate-Ui $parent
    Use-UiView $parent { param($view) $view.Select('Child'); $view.Focus() }
    [FileSpacerRegression.Native]::Keys($parent,0x0d,$false,$false)
    $child=Track-UiFolder $childPath
    Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($parent) } 'automatic source closure'
    $childRect=[FileSpacerRegression.Native]::Bounds($child)
    Assert-Ui ($childRect.Left -eq $sourceRect.Left -and $childRect.Top -eq $sourceRect.Top) 'Replacement placement counted the closing source.'
    Write-Host 'PASS: spatial replacement reuses the normal position of the maximized withdrawn source.'

    Close-UiFolder $child
    Set-PlacementRect $other $childRect.Left $childRect.Top
    $child=Open-UiFolder $childPath 0
    $restored=[FileSpacerRegression.Native]::Bounds($child)
    Assert-Ui ($restored.Left -eq $childRect.Left -and $restored.Top -eq $childRect.Top) 'Saved position lost priority over overlap reduction.'
    Assert-Ui ((Overlap-Area $restored ([FileSpacerRegression.Native]::Bounds($other))) -gt 0) 'Saved-position priority was not tested against a conflicting window.'
    Write-Host 'PASS: saved spatial position restored despite occupied area.'
} catch { $testFailure=$_; throw }
finally {
    if ($script:PlacementConfirmation -ne [IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($script:PlacementConfirmation)) {
        [void][FileSpacerRegression.Native]::PostMessage($script:PlacementConfirmation,0x466,[IntPtr]7,[IntPtr]::Zero)
        Wait-Ui { ![FileSpacerRegression.Native]::IsWindow($script:PlacementConfirmation) } 'maintenance confirmation cleanup'
    }
    Stop-Ui -Failure $testFailure
}
