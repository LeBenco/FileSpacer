# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco

function RecorderText([IntPtr]$Window) {
    try {return [FileSpacerRegression.Native]::Text($Window)}
    catch {
        $cause=$_.Exception
        while($cause.InnerException){$cause=$cause.InnerException}
        if($cause-is[ComponentModel.Win32Exception] -and $cause.NativeErrorCode-eq1400){return ''}
        throw
    }
}
function WindowCenter([IntPtr]$Window) {
    $bounds=[FileSpacerRegression.Native]::Bounds($Window)
    return Point ($bounds.Left+$bounds.Width/2) ($bounds.Top+$bounds.Height/2)
}
function MovePointer($Target,[int]$Milliseconds=700) {
    $start=[Windows.Forms.Cursor]::Position
    $steps=[Math]::Max(1,[Math]::Ceiling($Milliseconds/25.0))
    for($step=1;$step-le$steps;$step++) {
        [FileSpacerRegression.AltDragInput]::Move((Point ($start.X+($Target.X-$start.X)*$step/$steps) ($start.Y+($Target.Y-$start.Y)*$step/$steps)))
        Start-Sleep -Milliseconds 25
    }
}
function ClickPoint([IntPtr]$Owner,$Point) {
    MovePointer $Point
    [FileSpacerViewMenuTests.MenuUi]::Click($Point.X,$Point.Y,$Owner)
    Start-Sleep -Milliseconds 300
}
function ClickControl([IntPtr]$Owner,[IntPtr]$Control) {
    Assert-Ui ($Control-ne[IntPtr]::Zero) 'Missing visible control.'
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowEnabled($Owner)} 'the dialog owner is enabled after closing its child dialog'
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    do {
        $rect=[FileSpacerRegression.Native]::Bounds($Control)
        $point=Point ($rect.Left+$rect.Width/2) ($rect.Top+$rect.Height/2)
        MovePointer $point
        # Localized preview rendering can resize the dialog while the pointer moves.
        $current=[FileSpacerRegression.Native]::Bounds($Control)
        $center=Point ($current.Left+$current.Width/2) ($current.Top+$current.Height/2)
        $ready=[FileSpacerRegression.Native]::IsWindowEnabled($Control) -and $point.X-eq$center.X -and $point.Y-eq$center.Y
        if(!$ready -and [DateTime]::UtcNow-ge$deadline){throw 'The visible control did not settle under the pointer.'}
    }while(!$ready)
    [FileSpacerViewMenuTests.MenuUi]::Click($point.X,$point.Y,$Owner)
    Start-Sleep -Milliseconds 300
}
function OpenRecordedMenu([IntPtr]$Window,[string]$Button) {
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowEnabled($Window)} 'the folder is enabled after closing its dialogs'
    $id=Get-UiId $Button
    $bars=@([FileSpacerRegression.Native]::Children($Window,'ToolbarWindow32')|Where-Object {[FileSpacerRegression.Native]::Message($_,0x419,$id,0)-ge0})
    Assert-Ui ($bars.Count-eq1) 'Exactly one actual toolbar button must match.'
    $rect=[FileSpacerRegression.Native]::ToolbarButton($bars[0],$id)
    ClickPoint $Window (Point (($rect.Left+$rect.Right)/2) (($rect.Top+$rect.Bottom)/2))
    Wait-Ui {$null-ne[FileSpacerViewMenuTests.MenuUi]::Active($Window,$true)} 'native menu opened by the visible pointer'
}

function Place([IntPtr]$Window,[int]$X,[int]$Y,[int]$Width,[int]$Height) {
    Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($Window,[IntPtr]::Zero,$X,$Y,$Width,$Height,0x14)) 'Cannot place capture window.'
    [void][FileSpacerRegression.Native]::Message($Window,0x232,0,0)
}
function Shot([string]$Name,[IntPtr[]]$Windows,[int]$Padding=0) {
    $point=New-Object FileSpacerRegression.MousePoint;$point.X=1560;$point.Y=80
    [FileSpacerRegression.AltDragInput]::Move($point)
    Start-Sleep -Milliseconds 500
    $frames=@($Windows | ForEach-Object {
        if([FileSpacerRegression.Native]::Class($_)-eq'#32768'){[FileSpacerRegression.Native]::Bounds($_)}
        else{[CaptureDesktop]::Frame($_)}
    })
    $left=($frames | Measure-Object Left -Minimum).Minimum-$Padding
    $top=($frames | Measure-Object Top -Minimum).Minimum-$Padding
    $right=($frames | Measure-Object Right -Maximum).Maximum+$Padding
    $bottom=($frames | Measure-Object Bottom -Maximum).Maximum+$Padding
    $file=$Name+'.png'
    [CaptureDesktop]::Region((Join-Path $script:captures $file),$left,$top,($right-$left),($bottom-$top))
    [void]$records.Add(@{File=$file;Language=$script:language;Windows=@($frames);Padding=$Padding;
        BinarySha256=(Get-FileHash -LiteralPath $script:UiExecutable -Algorithm SHA256).Hash;Nilesoft=$false})
    Write-Host ('CAPTURED '+$file)
}
function CloseMenu([IntPtr]$Window){[FileSpacerRegression.Native]::Keys($Window,0x1b,$false,$false);Wait-Ui {@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0)).Count-eq0} 'menu dismissed'}
function Point([int]$X,[int]$Y){$p=New-Object FileSpacerRegression.MousePoint;$p.X=$X;$p.Y=$Y;return $p}
function UiaControl([IntPtr]$Window,[string]$Id) {
    $root=[Windows.Automation.AutomationElement]::FromHandle($Window)
    $element=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::AutomationIdProperty,$Id)))
    if($null-eq$element -and $Id-eq'SaveButton') {
      $element=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::NameProperty,'Save as')))
    }
    return $element
}
function RecorderWindows([int]$OwnerId) {
    $condition=New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::ProcessIdProperty,$OwnerId)
    foreach($element in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,$condition)) {
      if($element.Current.NativeWindowHandle){[IntPtr]$element.Current.NativeWindowHandle}
    }
}
function Background([IntPtr]$Window) {
    $bounds=[FileSpacerRegression.Native]::ClientBounds($Window)
    # The seven-item icon fixture leaves this lower band empty. Use native hit
    # testing: accessibility treats the recorder's hollow area as its window.
    $point=Point ($bounds.Left+45) ($bounds.Bottom-55)
    Assert-Ui ([CaptureDesktop]::OwnsPoint($Window,$point.X,$point.Y)) 'The actual empty folder background must be exposed.'
    Write-Host ('Empty folder background: window='+$Window+', point='+$point.X+','+$point.Y)
    return $point
}
function CloseRecorder([Diagnostics.Process]$Process) {
    $Process.Refresh()
    if(!$Process.HasExited) {
      Wait-Ui {
        $Process.Refresh();if($Process.HasExited){return $true}
        foreach($handle in RecorderWindows $Process.Id) {
          if((RecorderText $handle)-eq'ScreenToGif - Editor') {
            $play=UiaControl $handle 'PlayPauseButton'
            return $null-ne$play -and $play.Current.IsEnabled
          }
        };return $false
      } 'recorder completed before normal editor shutdown' 0 45
      $Process.Refresh();if($Process.HasExited){return}
      [void]$Process.CloseMainWindow()
      if(!$Process.WaitForExit(15000)) {
        [CaptureDesktop]::Desktop((Join-Path $env:FS_REGRESSION_RUN 'recorder-close-failure.png'))
        foreach($handle in RecorderWindows $Process.Id) {
          [Windows.Automation.AutomationElement]::FromHandle($handle).FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition) |
            ForEach-Object {"$($_.Current.AutomationId)`t$($_.Current.Name)"} | Add-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'recorder-close-failure.tsv')
        }
        throw 'ScreenToGif did not close normally.'
      }
    }
    Write-Host ('ScreenToGif exit='+$Process.ExitCode)
    if($Process.ExitCode-notin@(0,2)){throw ('ScreenToGif exited unexpectedly: '+$Process.ExitCode)}
}
function StopRecording([IntPtr]$Window,[Diagnostics.Process]$Recorder) {
    $foreground=[FileSpacerRegression.Native]::GetForegroundWindow()
    $foregroundId=[uint32]0;$folderId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($foreground,[ref]$foregroundId)
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$folderId)
    Assert-Ui ($foregroundId-eq$folderId -or $foregroundId-eq$Recorder.Id) 'Recording stop must target an owned capture window.'
    [FileSpacerRegression.Native]::Keys($foreground,0x77,$false,$false)
}
function Record([IntPtr]$Window,[string]$Kind,[scriptblock]$Action=$null,[Drawing.Rectangle]$Area=[Drawing.Rectangle]::Empty) {
    Wait-Ui {[FileSpacerRegression.Native]::IsWindowEnabled($Window)} 'the folder is enabled before recording'
    if($null-eq$Action){Place $Window 340 280 720 440}
    Activate-Ui $Window
    $point=WindowCenter $Window
    [FileSpacerRegression.AltDragInput]::Move($point)
    if($Area.IsEmpty){$Area=New-Object Drawing.Rectangle(308,218,900,600)}
    Assert-Ui ([CaptureDesktop]::IsDesktopPoint($Area.Left+3,$Area.Top+3)) 'The recording must show the real desktop around the folder, without another application covering the wallpaper.'
    $recorderRoot=Join-Path $ProjectRoot 'build\documentation-capture\reusable-recorder'
    $cache=Join-Path $env:FS_REGRESSION_RUN ('recording\'+$script:language+'\'+$Kind)
    [void][IO.Directory]::CreateDirectory($cache)
    $folder=[Security.SecurityElement]::Escape($script:captures)
    $temporary=[Security.SecurityElement]::Escape($cache)
    $settings=@"
<ResourceDictionary xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
 xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" xmlns:s="clr-namespace:System;assembly=mscorlib"
 xmlns:c="clr-namespace:System.Collections;assembly=mscorlib" xmlns:t="clr-namespace:ScreenToGif.ViewModel.Tasks;assembly=ScreenToGif.ViewModel"
 xmlns:g="clr-namespace:ScreenToGif.ViewModel.ExportPresets.AnimatedImage.Gif;assembly=ScreenToGif.ViewModel">
 <s:String x:Key="Version">2.43.2</s:String>
 <s:Boolean x:Key="ShowCursor">True</s:Boolean><s:Boolean x:Key="UsePreStart">False</s:Boolean>
 <s:Boolean x:Key="UseDesktopDuplication">False</s:Boolean><s:Boolean x:Key="UseMemoryCache">False</s:Boolean>
 <s:Boolean x:Key="OnlyCaptureChanges">False</s:Boolean><s:Boolean x:Key="FixedFrameRate">True</s:Boolean>
 <s:Boolean x:Key="NotifyWhileClosingEditor">False</s:Boolean><s:Boolean x:Key="NotifyWhileClosingApp">False</s:Boolean>
 <s:Boolean x:Key="NotifyProjectDiscard">False</s:Boolean><s:Boolean x:Key="AutomaticCleanUp">False</s:Boolean>
 <s:Boolean x:Key="AskDeleteCacheWhenClosing">False</s:Boolean><s:String x:Key="TemporaryFolder">$temporary</s:String>
 <c:ArrayList Capacity="2" x:Key="AutomatedTasksList">
  <t:MouseEventsViewModel TaskType="MouseEvents" LeftButtonForegroundColor="#78FFFF00" RightButtonForegroundColor="#78FF0000" MiddleButtonForegroundColor="#7800FFFF" HighlightForegroundColor="#000000FF" Width="12" Height="12"/>
  <t:KeyStrokesViewModel TaskType="KeyStrokes" KeyStrokesIgnoreNonModifiers="False" KeyStrokesIgnoreInjected="False" KeyStrokesTranslate="False" KeyStrokesExtended="True" KeyStrokesDelay="3500" KeyStrokesSeparator="  " KeyStrokesFontFamily="Segoe UI" KeyStrokesFontSize="24" KeyStrokesFontColor="#FFFFFFFF" KeyStrokesFontWeight="Bold" KeyStrokesBackgroundColor="#BB000000" KeyStrokesHorizontalAlignment="Center" KeyStrokesVerticalAlignment="Bottom" KeyStrokesPadding="5" KeyStrokesMinHeight="36"/>
 </c:ArrayList>
 <c:ArrayList Capacity="1" x:Key="ExportPresets"><g:EmbeddedGifPreset Title="Documentation" IsSelected="True" IsSelectedForEncoder="True" HasAutoSave="True" OutputFolder="$folder" OutputFilename="$Kind" OverwriteMode="Allow" SamplingFactor="1" MaximumColorCount="256" DetectUnchanged="True"/></c:ArrayList>
</ResourceDictionary>
"@
    [IO.File]::WriteAllText((Join-Path $recorderRoot 'Settings.xaml'),$settings,(New-Object Text.UTF8Encoding($false)))
    $region=@($Area.X,$Area.Y,$Area.Width,$Area.Height)-join','
    $limit='00:01:30'
    $recorder=Start-Process -FilePath (Join-Path $recorderRoot 'ScreenToGif.exe') -ArgumentList ('-n -lang en -softmode -o screen-recorder -r '+$region+' -f 12fps -l '+$limit) -PassThru
    $recordingActive=$false
    $editor=[IntPtr]::Zero;$recordFailure=$null
    try {
      Wait-Ui {$recorder.Refresh();$recorder.MainWindowHandle-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindowVisible($recorder.MainWindowHandle)} 'the ScreenToGif recorder is ready' 0 30
      Assert-Ui ($recorder.WaitForInputIdle(10000)) 'ScreenToGif must finish startup before recording.'
      Activate-Ui $Window
      [FileSpacerRegression.AltDragInput]::Move($point)
      Start-Sleep -Milliseconds 5000
      [FileSpacerRegression.Native]::Keys($Window,0x76,$false,$false) # F7 starts the prepared recorder without taking focus.
      $recordingActive=$true
      [FileSpacerRegression.AltDragInput]::Move($point)
      Wait-Ui {@(Get-ChildItem -LiteralPath $cache -Recurse -File -Filter '*.png').Count-ge2} 'real ScreenToGif frames' 0 40
      Assert-Ui ([FileSpacerRegression.Native]::GetForegroundWindow()-eq$Window) 'The recording must start with the folder active.'
      Start-Sleep -Milliseconds 600
      $before=[FileSpacerRegression.Native]::Bounds($Window)
      if($null-ne$Action){& $Action}
      else {
      if($Kind-eq'resize-window'){[FileSpacerRegression.AltDragInput]::BeginRight($Window,$point,$true,$false)}
      else{[FileSpacerRegression.AltDragInput]::Begin($Window,$point,$true,$false)}
      try {for($step=1;$step-le48;$step++){
        [FileSpacerRegression.AltDragInput]::Move((Point ($point.X+120*$step/48) ($point.Y+70*$step/48)))
        Start-Sleep -Milliseconds 50
      }}finally{[FileSpacerRegression.AltDragInput]::End()}
      Wait-Ui {[FileSpacerRegression.AltDragInput]::Idle($Window)} 'recorded gesture completed' 0 5
      if($Kind-eq'move-window'){
        Wait-Ui {$actual=[FileSpacerRegression.Native]::Bounds($Window);$actual.Left-gt$before.Left -and $actual.Top-gt$before.Top -and $actual.Width-eq$before.Width -and $actual.Height-eq$before.Height} 'actual native movement without resizing' 150 5
      } else {
        Wait-Ui {$actual=[FileSpacerRegression.Native]::Bounds($Window);$actual.Left-eq$before.Left -and $actual.Top-eq$before.Top -and $actual.Width-eq$before.Width+120 -and $actual.Height-eq$before.Height+70} 'actual native resize from the pointer delta' 150 5
      }
      }
      $after=[FileSpacerRegression.Native]::Bounds($Window)
      if($null-eq$Action){Assert-Ui ($Area.Contains((New-Object Drawing.Rectangle($after.Left,$after.Top,$after.Width,$after.Height)))) 'The entire final folder window must remain inside the recorded region.'}
      Start-Sleep -Milliseconds 2000
      StopRecording $Window $recorder
      $recordingActive=$false
      $found=@{Value=[IntPtr]::Zero}
      Wait-Ui {foreach($handle in RecorderWindows $recorder.Id){if((RecorderText $handle)-eq'ScreenToGif - Editor'){$found.Value=$handle;return $true}};return $false} 'ScreenToGif editor' 0 45
      $editor=$found.Value
      Wait-Ui {$play=UiaControl $editor 'PlayPauseButton';$null-ne$play -and $play.Current.IsEnabled} 'processed recording' 0 45
      Activate-Ui $editor
      [FileSpacerRegression.Native]::Keys($editor,0x53,$true,$false) # Official Command.SaveAs: Ctrl+S.
      Wait-Ui {$null-ne(UiaControl $editor 'FilenameTextBox')} 'export panel' 0 10
      (UiaControl $editor 'FolderTextBox').GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($script:captures)
      (UiaControl $editor 'FilenameTextBox').GetCurrentPattern([Windows.Automation.ValuePattern]::Pattern).SetValue($Kind)
      Start-Sleep -Milliseconds 600
      [Windows.Automation.AutomationElement]::FromHandle($editor).FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition) |
        ForEach-Object {"$($_.Current.AutomationId)`t$($_.Current.Name)`t$($_.Current.ControlType.ProgrammaticName)"} |
        Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'recorder-controls.tsv') -Encoding UTF8
      [CaptureDesktop]::Desktop((Join-Path $env:FS_REGRESSION_RUN 'recorder-editor.png'))
      (UiaControl $editor 'ApplyButton').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
      $gif=Join-Path $script:captures ($Kind+'.gif')
      Wait-Ui {
        if(!(Test-Path -LiteralPath $gif)){return $false}
        try {$stream=[IO.File]::Open($gif,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None);try{if($stream.Length-lt20){return $false};[void]$stream.Seek(-1,[IO.SeekOrigin]::End);return $stream.ReadByte()-eq0x3b}finally{$stream.Dispose()}}catch [IO.IOException]{return $false}
      } 'complete exported GIF from ScreenToGif' 500 120
      $image=[Drawing.Image]::FromFile($gif)
      try {
        $frameCount=$image.GetFrameCount([Drawing.Imaging.FrameDimension]::Time)
        Assert-Ui ($frameCount-gt1 -and $image.Width-eq$Area.Width -and $image.Height-eq$Area.Height) 'The exported GIF must decode as an animation with the complete capture dimensions.'
      }finally{$image.Dispose()}
      [void]$records.Add(@{File=[IO.Path]::GetFileName($gif);Language=$script:language;Before=$before;After=$after;CaptureArea=@{X=$Area.X;Y=$Area.Y;Width=$Area.Width;Height=$Area.Height};Frames=$frameCount;MotionSteps=48;StepPauseMilliseconds=50;FinalPauseMilliseconds=2000;Nilesoft=$false;Recorder='ScreenToGif 2.43.2'})
      Write-Host ('RECORDED '+$gif)
    } catch {
      $recordFailure=$_
      try {[CaptureDesktop]::Desktop((Join-Path $env:FS_REGRESSION_RUN 'recording-first-error.png'))}catch{Write-Warning ('Capture diagnostics: '+$_)}
      if($recordingActive){try{StopRecording $Window $recorder;$recordingActive=$false}catch{Write-Warning ('Recorder stop: '+$_)}}
      try {
        $recordFailure | Format-List * -Force | Out-String | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'recording-first-error.txt')
        if($editor-ne[IntPtr]::Zero){[Windows.Automation.AutomationElement]::FromHandle($editor).FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition) | ForEach-Object {"$($_.Current.AutomationId)`t$($_.Current.Name)`t$($_.Current.ControlType.ProgrammaticName)"} | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'recorder-error-controls.tsv') -Encoding UTF8}
      }catch{Write-Warning ('Capture diagnostics: '+$_)}
      throw $recordFailure
    } finally {
      [FileSpacerRegression.AltDragInput]::End()
      $recorderClosed=$false
      try {CloseRecorder $recorder;$recorderClosed=$true} catch {if($null-eq$recordFailure){throw};Write-Host ('CLEANUP ERROR: '+$_)}
      if($null-eq$recordFailure -and $recorderClosed) {
        $cacheRoot=[IO.Path]::GetFullPath((Join-Path $env:FS_REGRESSION_RUN 'recording')).TrimEnd('\')+'\'
        $resolvedCache=(Resolve-Path -LiteralPath $cache).Path
        Assert-Ui ($resolvedCache.StartsWith($cacheRoot,[StringComparison]::OrdinalIgnoreCase)) 'The recorder cache must remain inside this capture run.'
        Remove-Item -LiteralPath $resolvedCache -Recurse -Force
      }
    }
}
function MenuRows {
    foreach($directory in @(Get-ChildItem -LiteralPath ([IO.Path]::GetTempPath()) -Directory -Filter 'FileSpacerMenu-*' | Where-Object {$_.CreationTime-ge$script:menuStarted} | Sort-Object LastWriteTime -Descending)) {
      $file=Join-Path $directory.FullName 'render\frame.tsv'
      if(Test-Path -LiteralPath $file) {
        $rows=@{}
        foreach($line in @(Get-Content -LiteralPath $file | Select-Object -Skip 1)){$row=$line.Split("`t");$rows[[int]$row[0]]=$row}
        return $rows
      }
    }
    return @{}
}
function MenuPoint([int]$Command) {
    $row=(MenuRows)[$Command]
    Assert-Ui ($null-ne$row) ('Rendered menu command missing: '+$Command)
    $preview=[FileSpacerRegression.Native]::GetDlgItem($script:editor,(Get-UiId 'IDC_TITLE_MENU_PREVIEW'))
    $rect=[FileSpacerRegression.Native]::Bounds($preview)
    return Point ($rect.Left+48+([int]$row[5]+[int]$row[7])/2) ($rect.Top+41+([int]$row[6]+[int]$row[8])/2)
}
function SlowDrag([IntPtr]$Window,$From,$To) {
    MovePointer $From
    [FileSpacerRegression.AltDragInput]::Begin($Window,$From,$false,$false)
    try {for($step=1;$step-le48;$step++){
      [FileSpacerRegression.AltDragInput]::Move((Point ($From.X+($To.X-$From.X)*$step/48) ($From.Y+($To.Y-$From.Y)*$step/48)))
      Start-Sleep -Milliseconds 50
    }}finally{[FileSpacerRegression.AltDragInput]::End()}
}
