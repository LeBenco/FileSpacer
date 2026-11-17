# SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
function EverythingShot([IntPtr]$Window,[string]$Folder,$Names) {
    $session=[Diagnostics.Process]::GetCurrentProcess().SessionId
    $tool=Join-Path $script:UiRoot 'Everything'
    [void][IO.Directory]::CreateDirectory($tool)
    $exe=Join-Path $tool 'Everything.exe'
    Copy-Item -LiteralPath 'C:\Program Files\Everything\Everything.exe' -Destination $exe
    if(Test-Path -LiteralPath 'C:\Program Files\Everything\Everything.lng'){Copy-Item -LiteralPath 'C:\Program Files\Everything\Everything.lng' -Destination $tool}
    $lcid=@{en=1033;fr=1036;de=1031;it=1040;es=1034;pt=1046;el=1032;pl=1045;nl=1043;uk=1058}[$script:language]
    $ini=Join-Path $tool 'Everything.ini'
    $settings=@('[Everything]','app_data=0','run_as_admin=0','run_in_background=0','show_tray_icon=0','instance_name=Studio','check_for_updates_on_startup=0','auto_include_fixed_volumes=0','auto_include_removable_volumes=0','auto_include_fixed_refs_volumes=0','auto_include_removable_refs_volumes=0',('folders='+$Folder),'folder_monitor_changes=1',('language='+$lcid))-join"`r`n"
    [IO.File]::WriteAllText($ini,$settings,(New-Object Text.UTF8Encoding($false)))
    [IO.File]::WriteAllText((Join-Path $tool 'Everything-Studio.ini'),$settings,(New-Object Text.UTF8Encoding($false)))
    $external=Start-TestApplication -Executable $exe -Arguments ('-instance Studio -config "'+$ini+'" -db "'+(Join-Path $tool 'Everything.db')+'"') -OutputDirectory $env:FS_REGRESSION_RUN
    $handle=[IntPtr]::Zero
    try {
      Wait-Ui {$external.Refresh();$external.MainWindowHandle-ne[IntPtr]::Zero} 'real Everything window'
      $handle=$external.MainWindowHandle
      Assert-Ui ([FileSpacerRegression.Native]::Text($handle).Contains('Studio')) 'The documentation must use its own named Everything instance.'
      Set-SearchOptions $Window $true 5 $exe 0
      Submit-Search $Window $Names[10] @($Names[3])
      Select-SearchMenu $Window 0
      $query='"'+$Folder+'\" <'+$Names[10]+'>'
      Wait-Ui {@([FileSpacerRegression.Native]::Children($handle,'Edit')|Where-Object {[FileSpacerRegression.Native]::Text($_)-ceq$query}).Count-eq1} 'actual Everything query'
      Wait-Ui {(([FileSpacerRegression.SearchMenus]::ResultNames($handle)|Sort-Object)-join'|') -ceq ((@($Names[3],$Names[3])|Sort-Object)-join'|')} 'actual recursive Everything results' 300 60
      Submit-Search $Window $Names[10] @($Names[3])
      Place $Window 100 250 650 500;Place $handle 805 250 700 500
      Shot 'everything-search' @($Window,$handle) 32
      [void]$records.Add(@{Scene='Everything';Language=$script:language;Query=$query;ResultNames=@([FileSpacerRegression.SearchMenus]::ResultNames($handle));Version=(Get-Item -LiteralPath $exe).VersionInfo.FileVersion;QueryReenteredAfterSuccessfulLaunch=$true})
    } finally {
      if($handle-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($handle)){[void][FileSpacerRegression.Native]::PostMessage($handle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)}
      Assert-Ui ($external.WaitForExit(15000)) 'Documentation Everything did not close normally.'
      Complete-TestApplications @($external)
    }
}
function SelectStaticView([IntPtr]$Window,[int]$Index,[int]$Mode,[int]$Size=-1) {
    OpenRecordedMenu $Window 'IDM_VIEW_MENU'
    $menu=[FileSpacerViewMenuTests.MenuUi]::Active($Window)
    Assert-Ui ($menu.Entries.Count-ge8) 'The actual View menu must expose the eight display modes.'
    $bounds=$menu.Entries[$Index].Bounds
    ClickPoint $Window (Point (($bounds.Left+$bounds.Right)/2) (($bounds.Top+$bounds.Bottom)/2))
    Wait-Ui {$null-eq[FileSpacerViewMenuTests.MenuUi]::Active($Window)} 'the View menu has committed the chosen display mode'
    Wait-UiViewMode $Window $Mode $Size
}
function WaitStaticContents([IntPtr]$Window,[int]$Count) {
    Wait-Ui {
        $view=Get-UiView $Window
        if($null-eq$view){return $false}
        try{return $view.ContentVisible -and $view.Count-eq$Count}
        finally{$view.Dispose()}
    } 'the replacement view exposes the complete fictional folder'
}
function StaticViews {
    $n=(Get-Content -LiteralPath (Join-Path $source 'static-scenes.json') -Raw -Encoding UTF8|ConvertFrom-Json).($script:language)
    $other=(Get-Content -LiteralPath (Join-Path $source 'document-scenes.json') -Raw -Encoding UTF8|ConvertFrom-Json).($script:language)
    $staticRoot=Join-Path $script:UiRoot 'static'
    $project=Join-Path $staticRoot $n[0]
    $documents=Join-Path $staticRoot $n[1]
    $pictures=Join-Path $staticRoot $n[2]
    foreach($path in @($project,$documents,$pictures)){[void][IO.Directory]::CreateDirectory($path)}
    foreach($index in @(3,4,5)){[IO.File]::WriteAllText((Join-Path $project $n[$index]),'Fictional documentation example.')}
    foreach($index in @(8,9)){[void][IO.Directory]::CreateDirectory((Join-Path $project $n[$index]))}
    foreach($index in @(6,7)){
        $bitmap=New-Object Drawing.Bitmap(400,280)
        $graphics=[Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([Drawing.Color]::FromArgb(220,228,238))
            $brush=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb($(if($index-eq6){88}else{174}),125,155))
            try{$graphics.FillEllipse($brush,60,30,280,220)}finally{$brush.Dispose()}
            $bitmap.Save((Join-Path $project $n[$index]))
        }finally{$graphics.Dispose();$bitmap.Dispose()}
    }
    foreach($name in $other[0..6]){[IO.File]::WriteAllText((Join-Path $documents $name),'Separate fictional administrative document.')}
    $assets=Join-Path $ProjectRoot 'build\documentation-capture'
    Copy-Item -LiteralPath (Join-Path $assets 'nature-lake.jpg') -Destination (Join-Path $pictures $other[7])
    Copy-Item -LiteralPath (Join-Path $assets 'nature-mountain.jpg') -Destination (Join-Path $pictures $other[8])
    [IO.File]::WriteAllText((Join-Path (Join-Path $project $n[8]) $n[3]),'Fictional recursive search example.')
    $mapping=$null
    foreach($letter in @('P','Q','R')){if(!(Test-Path -LiteralPath ($letter+':\'))){$mapping=$letter;break}}
    Assert-Ui ($null-ne$mapping) 'No unused documentation fixture drive.'
    & "$env:WINDIR\System32\subst.exe" ($mapping+':') $staticRoot
    Assert-Ui ($LASTEXITCODE-eq0) 'Cannot map the isolated documentation fixture.'
    try {
        $project=Join-Path ($mapping+':\') $n[0]
        $documents=Join-Path ($mapping+':\') $n[1]
        $pictures=Join-Path ($mapping+':\') $n[2]
        # Each scene uses a fresh test composition, not the preceding animation's edits.
        $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test',$true)
        try{$key.DeleteValue('TitleMenu',$false)}finally{$key.Dispose()}
        Set-UiPreference 'MixedFolderSorting' 0
        Set-UiPreference 'EverythingEnabled' 0
        $window=Open-UiFolder $project 7
        Place $window 130 100 660 650
        SelectStaticView $window 2 1 48
        WaitStaticContents $window 7
        Use-UiView $window {param($view)$view.AutoArrange=$false;for($i=0;$i-lt7;$i++){$view.Position($view.Names()[$i],30+($i%3)*180,30+[Math]::Floor($i/3)*160)}}
        $documentWindow=Open-UiFolder $documents 7
        Place $documentWindow 830 100 408 330
        SelectStaticView $documentWindow 4 3
        WaitStaticContents $documentWindow 7
        $imageWindow=Open-UiFolder $pictures 2
        Place $imageWindow 850 545 640 425
        SelectStaticView $imageWindow 0 1 256
        WaitStaticContents $imageWindow 2
        Start-Sleep -Milliseconds 1500
        Shot 'spatial-windows' @($window,$documentWindow,$imageWindow) 32
        Close-UiFolder $documentWindow
        Close-UiFolder $imageWindow
        Place $window 260 150 900 550
        Activate-Ui $window
        Shot 'overview' @($window)
        OpenRecordedMenu $window 'IDM_PROXY_BUTTON'
        $menu=[FileSpacerViewMenuTests.MenuUi]::Active($window,$true)
        Shot 'window-menu' @($window,$menu.Window)
        CloseMenu $window
        $actualNames=@(Use-UiView $window {param($view)$view.Names()})
        $selected=@($actualNames|Where-Object {$_-in@($n[3],[IO.Path]::GetFileNameWithoutExtension($n[3]))})
        Assert-Ui ($selected.Count-eq1) 'The long selected filename must have one actual Shell display name.'
        Use-UiView $window {param($view)$view.Select($selected[0])}
        if($script:language-ne'fr'){Shot 'selected-name' @($window)}
        [void](Show-UiSettings $window)
        $page=Set-UiPage 1
        Set-UiCheck $page 'IDC_MIXED_FOLDER_SORTING' $true
        Press-UiSettings 3
        Wait-UiViewMode $window 1 48
        WaitStaticContents $window 7
        Use-UiView $window {param($view)$view.SetViewModeAndIconSize(1,48);$view.AutoArrange=$true;$view.SortBy($false,$true)}
        if($script:language-ne'fr'){Shot 'mixed-sorting' @($window)}
        [void](Show-UiSettings $window)
        [void](Set-UiPage 1)
        Shot 'display' @($script:UiSettings)
        Press-UiSettings 5
        if($script:language-eq'fr'){FrenchReferenceShots ($mapping+':\')}
        Set-SearchOptions $window $false 5
        Submit-Search $window $n[10] @($n[3])
        [void](Show-SearchMenu $window)
        $popup=@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,0))
        Shot 'search' (@($window)+$popup)
        CloseMenu $window
        EverythingShot $window $project $n
        Submit-Search $window '' @($n[3],$n[4],$n[5],$n[6],$n[7],$n[8],$n[9])
        NilesoftShots $window $project
    }finally{
        foreach($handle in @($script:UiWindows)){
            if([FileSpacerRegression.Native]::IsWindow($handle)){Close-UiFolder $handle}
        }
        & "$env:WINDIR\System32\subst.exe" ($mapping+':') /d
        Assert-Ui ($LASTEXITCODE-eq0) 'The temporary fixture drive could not be removed.'
    }
}
function FrenchReferenceShots([string]$Root) {
    # These two older French illustrations have their own useful fixtures.
    $notes=Join-Path $Root 'Notes du projet'
    [void][IO.Directory]::CreateDirectory($notes)
    $long="Notes du projet - revue des maquettes, liste de livraison finale et décisions pour l’atelier d’automne.txt"
    foreach($name in @('Cadrage du projet.txt','Jalons.csv','Liste de contrôle.txt',$long,'Notes.md')){
        [IO.File]::WriteAllText((Join-Path $notes $name),'Fictional project notes for the guide.')
    }
    $window=Open-UiFolder $notes 5
    Place $window 260 150 740 460
    SelectStaticView $window 2 1 48
    WaitStaticContents $window 5
    Use-UiView $window {param($view)$view.AutoArrange=$true}
    $names=@(Use-UiView $window {param($view)$view.Names()})
    $selected=@($names|Where-Object {$_-in@($long,[IO.Path]::GetFileNameWithoutExtension($long))})
    Assert-Ui ($selected.Count-eq1) 'The full-name example must contain its original long French filename.'
    Use-UiView $window {param($view)$view.Select($selected[0])}
    Shot 'selected-name' @($window)
    Close-UiFolder $window
    $fixture=(Get-Content -LiteralPath (Join-Path $source 'mixed-sorting-fr.json') -Raw -Encoding UTF8|ConvertFrom-Json)
    $library=Join-Path $Root ('Exemples\Projets\Studio\'+$fixture.Title)
    [void][IO.Directory]::CreateDirectory($library)
    foreach($name in $fixture.Files){[IO.File]::WriteAllText((Join-Path $library $name),'Fictional studio document.')}
    foreach($name in $fixture.Folders){[void][IO.Directory]::CreateDirectory((Join-Path $library $name))}
    $window=Open-UiFolder $library 28
    Place $window 260 150 960 560
    SelectStaticView $window 2 1 48
    WaitStaticContents $window 28
    Use-UiView $window {param($view)$view.AutoArrange=$true;$view.SortBy($false,$true)}
    Shot 'mixed-sorting' @($window)
    Close-UiFolder $window
}

function OpenNilesoftMenu([IntPtr]$Window,[string]$Button) {
    Activate-Ui $Window
    $id=Get-UiId $Button
    $bars=@([FileSpacerRegression.Native]::Children($Window,'ToolbarWindow32')|Where-Object {[FileSpacerRegression.Native]::Message($_,0x419,$id,0)-ge0})
    Assert-Ui ($bars.Count-eq1) 'Exactly one toolbar button must match the Nilesoft comparison.'
    $rect=[FileSpacerRegression.Native]::ToolbarButton($bars[0],$id)
    ClickPoint $Window (Point (($rect.Left+$rect.Right)/2) (($rect.Top+$rect.Bottom)/2))
    # Nilesoft supplies its own menu rendering. Its popup need not expose the
    # native menu-bar metadata used by the ordinary View-menu inspector.
    $ownerId=[uint32]0
    [void][FileSpacerRegression.Native]::GetWindowThreadProcessId($Window,[ref]$ownerId)
    Wait-Ui {@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,$ownerId)|Where-Object {[FileSpacerRegression.Native]::IsWindowVisible($_)}).Count-eq1} 'visible Nilesoft popup belonging to the captured application'
}
function NilesoftShots([IntPtr]$Window,[string]$Folder) {
    Close-UiFolder $Window
    $installation='C:\Program Files\Nilesoft Shell'
    $configuration=Join-Path $installation 'shell.nss'
    $originalHash=(Get-FileHash -LiteralPath $configuration -Algorithm SHA256).Hash
    $fixture=Join-Path $script:UiRoot 'Nilesoft'
    Copy-Item -LiteralPath $installation -Destination $fixture -Recurse
    $dll=Join-Path $fixture 'shell.dll'
    $handler=[Microsoft.Win32.Registry]::ClassesRoot.OpenSubKey('Directory\shellex\ContextMenuHandlers\ @nilesoft.shell')
    try {$classId=([guid]$handler.GetValue('')).ToString('B')} finally {$handler.Dispose()}
    $classPath='Software\Classes\CLSID\'+$classId
    $serverPath=$classPath+'\InprocServer32'
    $originalClass=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($classPath)
    $classExisted=$null-ne$originalClass;if($originalClass){$originalClass.Dispose()}
    $original=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($serverPath)
    $serverExisted=$null-ne$original
    $defaultExisted=$serverExisted -and ('' -in @($original.GetValueNames()))
    if($defaultExisted){$value=$original.GetValue('',$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames);$kind=$original.GetValueKind('')}
    if($original){$original.Dispose()}
    $template=@'
settings {
    priority = 1
    exclude.where = !process.is_explorer && !window.is_explorer && process.name != 'FileSpacer'
}
$filespacer = (process.name == 'FileSpacer')
$filespacer_title = (filespacer && window.parent.name == 'FileSpacer Item Window')
$filespacer_toolbar = (filespacer && window.parent.name == 'FileSpacer Toolbar Host')
theme {
    name = 'modern'
    dark = true
    {RADIUS}
}
import 'imports/images.nss'
'@
    $active=[IntPtr]::Zero
    $env:FILESPACER_TEST_NILESOFT='1'
    try {
      $key=[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($serverPath)
      try{$key.SetValue('',$dll,[Microsoft.Win32.RegistryValueKind]::String)}finally{$key.Dispose()}
      $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\FileSpacer\Test',$true)
      try{$key.DeleteValue('TitleMenu',$false)}finally{$key.Dispose()}
      foreach($variant in @('default','adapted')) {
        $radius=if($variant-eq'adapted'){"border.radius = if(process.name == 'FileSpacer' && (window.parent.name == 'FileSpacer Item Window' || window.parent.name == 'FileSpacer Toolbar Host'), 0, default)"}else{''}
        $text=$template.Replace('{RADIUS}',$radius)
        [IO.File]::WriteAllText((Join-Path $fixture 'shell.nss'),$text,(New-Object Text.UTF8Encoding($true)))
        [IO.File]::WriteAllText((Join-Path $env:FS_REGRESSION_RUN ('nilesoft-'+$script:language+'-'+$variant+'.nss')),$text,(New-Object Text.UTF8Encoding($false)))
        $active=Open-UiFolder $Folder 7
        Place $active 260 150 900 550
        foreach($scene in @('main-menu','navigation')) {
          OpenNilesoftMenu $active $(if($scene-eq'main-menu'){'IDM_CONTEXT_MENU'}else{'IDM_PROXY_BUTTON'})
          [FileSpacerRegression.AltDragInput]::Move((Point 1560 80));Start-Sleep -Milliseconds 900
          $ownerId=[uint32]0;[void][FileSpacerRegression.Native]::GetWindowThreadProcessId($active,[ref]$ownerId)
          $popups=@([FileSpacerRegression.Native]::Top('#32768',[NullString]::Value,$ownerId)|Where-Object {[FileSpacerRegression.Native]::IsWindowVisible($_)})
          Assert-Ui ($popups.Count-eq1) 'The comparison must capture the visible popup from this application.'
          $popup=$popups[0]
          $modules=@((Get-Process -Id $ownerId).Modules | Where-Object {$_.FileName-eq$dll})
          Assert-Ui ($modules.Count-eq1) 'The real Nilesoft fixture module must be loaded in FileSpacer.'
          $frame=[CaptureDesktop]::Frame($active);$menu=[FileSpacerRegression.Native]::Bounds($popup)
          $left=if($scene-eq'navigation'){[Math]::Max($frame.Left,$menu.Left-170)}else{[Math]::Max($frame.Left,$menu.Left-12)}
          $right=$menu.Right+12;$bottom=$menu.Bottom+12
          @{Scene=$scene;Language=$script:language;Frame=$frame;Menu=$menu;Left=$left;Right=$right;Bottom=$bottom}|ConvertTo-Json -Depth 5|Add-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'nilesoft-bounds.log') -Encoding UTF8
          Assert-Ui ($right-le1600 -and $bottom-le1200 -and $menu.Width-gt100 -and $left-le$menu.Left -and $right-gt$menu.Right) 'The complete Nilesoft menu and its margin must fit in the capture.'
          [FileSpacerRegression.AltDragInput]::Move((Point 1560 80));Start-Sleep -Milliseconds 700
          $file='nilesoft-'+$scene+'-'+$variant+'.png'
          [CaptureDesktop]::Region((Join-Path $script:captures $file),$left,$frame.Top,($right-$left),($bottom-$frame.Top))
          [void]$records.Add(@{File=$file;Language=$script:language;Nilesoft=$true;Variant=$variant;NilesoftVersion=$modules[0].FileVersionInfo.FileVersion;MenuBounds=$menu;Crop=@{Left=$left;Top=$frame.Top;Right=$right;Bottom=$bottom}})
          CloseMenu $active
        }
        Close-UiFolder $active;$active=[IntPtr]::Zero
      }
    } catch {
      try{[CaptureDesktop]::Desktop((Join-Path $env:FS_REGRESSION_RUN 'nilesoft-first-error.png'))}catch{Write-Warning ('Nilesoft diagnostics: '+$_)}
      throw
    } finally {
      if($active-ne[IntPtr]::Zero -and [FileSpacerRegression.Native]::IsWindow($active)){Close-UiFolder $active}
      $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($serverPath,$true)
      if($key){try{if($defaultExisted){$key.SetValue('',$value,$kind)}else{$key.DeleteValue('',$false)}}finally{$key.Dispose()}}
      if(!$serverExisted){$parent=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($classPath,$true);if($parent){try{$child=$parent.OpenSubKey('InprocServer32');if($child){try{$empty=$child.ValueCount-eq0 -and $child.SubKeyCount-eq0}finally{$child.Dispose()};if($empty){$parent.DeleteSubKey('InprocServer32',$false)}}}finally{$parent.Dispose()}}}
      if(!$classExisted){$parent=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Classes\CLSID',$true);if($parent){try{$child=$parent.OpenSubKey($classId);if($child){try{$empty=$child.ValueCount-eq0 -and $child.SubKeyCount-eq0}finally{$child.Dispose()};if($empty){$parent.DeleteSubKey($classId,$false)}}}finally{$parent.Dispose()}}}
      $env:FILESPACER_TEST_NILESOFT='0'
      $restored=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($serverPath)
      try {
        Assert-Ui (($null-ne$restored)-eq$serverExisted) 'The original Nilesoft registration key was not restored.'
        if($restored){
          Assert-Ui (('' -in @($restored.GetValueNames()))-eq$defaultExisted) 'The Nilesoft registration value was not restored.'
          if($defaultExisted){Assert-Ui ($restored.GetValue('',$null,[Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)-ceq$value -and $restored.GetValueKind('')-eq$kind) 'The Nilesoft registration data differs after cleanup.'}
        }
      } finally {if($restored){$restored.Dispose()}}
      Assert-Ui ((Get-FileHash -LiteralPath $configuration -Algorithm SHA256).Hash-ceq$originalHash) 'The installed Nilesoft configuration changed.'
      [void]$records.Add(@{Scene='NilesoftCleanup';Language=$script:language;InstalledConfigurationUnchanged=$true;PerUserClassRestored=$true;FixtureOnly=$true})
    }
}
