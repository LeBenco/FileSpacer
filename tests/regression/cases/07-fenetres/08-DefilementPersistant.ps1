# Test-Category: UI
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\common\EssentialUi.ps1')
. (Join-Path $PSScriptRoot '..\..\common\ViewLayout.ps1')
function Read-Viewport([IntPtr]$Window) {
 try {
  $body=Use-UiView $Window {param($v) $v.Window}
  $targetProcess=[uint32]0
  [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$targetProcess)
  return [FileSpacerRegression.ViewLayout]::TryReadScrollPosition($body,[int]$targetProcess)
 }
 catch {
  $failure=$_.Exception.GetBaseException()
  if($failure -is [System.Windows.Automation.ElementNotAvailableException]-or$failure -is [InvalidCastException]){return $null}
  throw
 }
}
function Same-Viewport($Left,$Right) {
 if($null-eq$Left-or$null-eq$Right){return $false}
 return [Math]::Abs($Left.Horizontal-$Right.Horizontal)-lt0.5-and[Math]::Abs($Left.Vertical-$Right.Vertical)-lt0.5
}
function Test-ViewModeReady([IntPtr]$Window,$Mode) {
 try {
  return (Use-UiView $Window {param($v) $v.Mode})-eq$Mode.Mode-and(Use-UiView $Window {param($v) $v.IconSize})-eq$Mode.Size
 } catch {
  # The renderer is recreated asynchronously; an old automation object can
  # briefly cease to implement IFolderView. Poll readiness, never replay the command.
  if($_.Exception.GetBaseException() -is [InvalidCastException]){return $false}
  throw
 }
}
$testFailure=$null
try {
 Initialize-Ui 'persistent-scroll'
 $modes=@(
  @{Name='extra-large';Mode=1;Size=256;Free=$false},
  @{Name='large';Mode=1;Size=96;Free=$false},
  @{Name='medium';Mode=1;Size=48;Free=$false},
  @{Name='small';Mode=2;Size=16;Free=$false},
  @{Name='list';Mode=3;Size=16;Free=$false},
  @{Name='details';Mode=4;Size=16;Free=$false},
  @{Name='tiles';Mode=6;Size=48;Free=$false},
  @{Name='free-icons';Mode=1;Size=48;Free=$true},
  @{Name='free-tiles';Mode=6;Size=48;Free=$true})
 foreach($mode in $modes) {
  $folder=Join-Path $script:UiRoot $mode.Name;[void][IO.Directory]::CreateDirectory($folder)
  foreach($index in 1..200){[IO.File]::WriteAllText((Join-Path $folder ('item-{0:000}.txt'-f$index)),'fixture')}
  $window=Open-UiFolder $folder 200
  $work=[FileSpacerRegression.Native]::WorkArea($window)
  Assert-Ui ([FileSpacerRegression.Native]::SetWindowPos($window,[IntPtr]::Zero,($work.Left+40),($work.Top+40),600,400,0x14)) 'Cannot place the viewport fixture.'
  Use-UiView $window {param($v) $v.SetViewModeAndIconSize([uint32]$mode.Mode,[int]$mode.Size)}
  Wait-Ui {Test-ViewModeReady $window $mode} ($mode.Name+' mode and icon size') 300 10
  if($mode.Free){Use-UiView $window {param($v) $v.AutoArrange=$false};Wait-Ui {!(Use-UiView $window {param($v) $v.AutoArrange})} 'free layout' 200 5}
  $top=$null
  Wait-Ui {$script:currentViewport=Read-Viewport $window;$null-ne$script:currentViewport} 'scroll provider ready' 200 5
  $top=$script:currentViewport
  Use-UiView $window {param($v) $v.Reveal('item-150.txt')}
  $seen=@{Before=$null;After=$null}
  Wait-Ui {$seen.Before=Read-Viewport $window;$null-ne$seen.Before-and$seen.Before.Signature-ne$top.Signature} ($mode.Name+' viewport moved before closing') 300 10
  Assert-UiSelected $window ''
  $seen.Before|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $RunDirectory ($mode.Name+'-before.json')) -Encoding UTF8
  Close-UiFolder $window
  # Ordinary open, deliberately not IDM_REOPEN_WINDOW or the closed-window history.
  $window=Open-UiFolder $folder 200
  Wait-Ui {Test-ViewModeReady $window $mode} ($mode.Name+' persisted mode') 300 10
  Wait-Ui {$seen.After=Read-Viewport $window;Same-Viewport $seen.After $seen.Before} ($mode.Name+' persisted horizontal and vertical scroll positions') 500 10
  Assert-UiSelected $window ''
  $seen.After|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $RunDirectory ($mode.Name+'-after.json')) -Encoding UTF8
  # Refresh must keep a subsequent viewport, not replay the persisted one.
  Use-UiView $window {param($v) $v.Reveal('item-020.txt')}
  Wait-Ui {(Read-Viewport $window).Signature-ne$seen.Before.Signature} 'viewport changed after restoration' 200 5
  Assert-Ui ([FileSpacerRegression.Native]::PostMessage($window,0x111,[IntPtr](Get-UiId 'IDM_REFRESH'),[IntPtr]::Zero)) 'Cannot refresh the window.'
  Wait-Ui {$read=Read-Viewport $window;$null-ne$read-and!(Same-Viewport $read $seen.Before)} 'refresh did not replay the persisted viewport' 500 10
  Close-UiFolder $window
  Write-Host ('PASS: '+$mode.Name+' ordinary reopen restores the viewport, without selection or replay on refresh.')
 }
} catch {$testFailure=$_;throw}
finally {Stop-Ui -Failure $testFailure}
