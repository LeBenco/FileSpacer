# Test-Category: UI
# M-17 probe: can the mixed comparator be limited to PIDL columns? Observes, on
# the real DefView, whether SHCOLSTATE_NOSORTBYFOLDERNESS is still honoured when
# the provider keeps SHCOLSTATE_PREFER_VARCMP (DefView compares values itself):
#   current : build\debug\FileSpacerFolder.dll (every comparison through CompareIDs);
#   counted : same flags as current, built from a copy that counts CompareIDs calls;
#   slow    : variant keeping PREFER_VARCMP on SHCOLSTATE_SLOW columns only;
#   all     : variant keeping PREFER_VARCMP on every column (PIDL columns included);
#   native  : mixed sorting disabled (DefView + CFSFolder, folders-first baseline).
# Each run reports the real view order, the folder/file mixing and sort timings.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
. (Join-Path $PSScriptRoot '..\..\..\regression\common\EssentialUi.ps1')
Add-Type -AssemblyName System.Drawing
Add-Type -Path (Join-Path $PSScriptRoot '..\..\..\regression\cases\30-tri-melange\SlowColumnProbe.cs')
$variantRoot=Join-Path $ProjectRoot 'build\regression\slow-column-provider'
if(!$env:FS_REGRESSION_PREPARED){[void](& (Join-Path $PSScriptRoot '..\..\..\regression\common\Prepare-SlowColumnMixedProvider.ps1') -ProjectRoot $ProjectRoot)}
$providers=[ordered]@{
    current=(Join-Path $ProjectRoot 'build\debug\FileSpacerFolder.dll')
    counted=(Join-Path $variantRoot 'counted\FileSpacerFolder.dll')
    slow=(Join-Path $variantRoot 'slow\FileSpacerFolder.dll')
    all=(Join-Path $variantRoot 'all\FileSpacerFolder.dll')
    native=(Join-Path $ProjectRoot 'build\debug\FileSpacerFolder.dll')
}
foreach($entry in $providers.GetEnumerator()){Assert-Ui (Test-Path -LiteralPath $entry.Value -PathType Leaf) ('Host-prepared provider missing for run '+$entry.Key+': '+$entry.Value)}
$providerClass=[guid]'E129740C-52FD-4C7B-98AA-9ED6EBF9A604'
$class='HKCU\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$providerKey='Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$registrationExisted=Test-Path -LiteralPath $providerKey
$backup=Join-Path $RunDirectory 'slow-column-provider-registration.reg'
if($registrationExisted){& reg.exe export $class $backup /y|Out-Host;if($LASTEXITCODE){throw 'Provider registry backup failed.'}}
$junction=Join-Path $env:LOCALAPPDATA 'FileSpacer\TestShellView.{E129740C-52FD-4C7B-98AA-9ED6EBF9A604}'
$junctionExisted=[IO.Directory]::Exists($junction)
$storage=[guid]'B725F130-47EF-101A-A5F1-02608C9EEBAC'   # PKEY_ItemNameDisplay 10, PKEY_DateModified 14
$imageSummary=[guid]'6444048F-4C8B-11D1-8B70-080036B11A03' # PKEY_Image_Dimensions 13
$columns=@(
    @{Label='Dimensions';Format=$imageSummary;Id=13},
    @{Label='Name';Format=$storage;Id=10}
)
$failure=$null
$cleanupErrors=New-Object Collections.Generic.List[string]
$observations=New-Object Collections.Generic.List[object]
$verdicts=New-Object Collections.Generic.List[string]
function Diag([string]$Text){Write-Host ('DIAG: '+$Text)}
function Wait-Provider([IntPtr]$Window,[bool]$Mixed) {
    Wait-Ui {
        try { return Use-UiView $Window {param($view) $view.ContentVisible -and (($view.FolderClassId -eq $providerClass) -eq $Mixed)} }
        catch [InvalidCastException] { return $false }
        catch [Runtime.InteropServices.COMException] { return $false }
    } ('provider active='+$Mixed) 300
}
# Folder block placement relative to files, from the real view order.
function Describe-Placement([string[]]$Names,[Collections.Generic.HashSet[string]]$Folders) {
    $indices=@();for($i=0;$i-lt$Names.Count;$i++){if($Folders.Contains($Names[$i])){$indices+=$i}}
    if(!$indices.Count){return 'none'}
    $min=($indices|Measure-Object -Minimum).Minimum;$max=($indices|Measure-Object -Maximum).Maximum
    if($max-eq$indices.Count-1){return 'first'}
    if($min-eq$Names.Count-$indices.Count){return 'last'}
    if($max-$min+1-eq$indices.Count){return 'block-at-'+$min}
    return 'interleaved'
}
# Sort through IFolderView2::SetSortColumns, then watch the real order until it
# is stable (DefView may re-sort after background extraction of slow columns).
function Get-CompareCount { $value=Get-UiPreference 'ProbeCompareIds'; if($null-eq$value){return 0}; return [int64][uint32]$value }
function Measure-Sort([IntPtr]$Window,$Column,[bool]$Ascending,[string]$Run,[int]$Round) {
    $comparesBefore=Get-CompareCount
    $watch=[Diagnostics.Stopwatch]::StartNew()
    $call=Use-UiView $Window {param($view) $view.SortByColumn($Column.Format,[uint32]$Column.Id,$Ascending)}
    # DefView may empty the list while it sorts a slow column in the background:
    # the order counts as final only with every item present and 3 s unchanged.
    $previous=$null;$lastChange=$watch.Elapsed.TotalMilliseconds;$polls=0;$changes=0;$incompletePolls=0;$completeAt=-1
    while($true) {
        $names=@(Use-UiView $Window {param($view) $view.OrderedNames()});$polls++
        $joined=$names -join '|'
        if($joined -cne $previous){ if($null-ne$previous){$changes++}; $previous=$joined;$lastChange=$watch.Elapsed.TotalMilliseconds }
        if($names.Count -ne $script:items.Count){$incompletePolls++;$completeAt=-1}
        elseif($completeAt -lt 0){$completeAt=[Math]::Round($watch.Elapsed.TotalMilliseconds,0)}
        elseif($watch.Elapsed.TotalMilliseconds-$lastChange -ge 3000){break}
        if($watch.Elapsed.TotalSeconds -ge 120){throw ('The view order did not become stable within 120 s: '+$Run+' '+$Column.Label+' (items='+$names.Count+')')}
        Start-Sleep -Milliseconds 100
    }
    $direction=if($Ascending){'asc'}else{'desc'}
    $expectedFiles=[string[]]$(if($Column.Label-eq'Dimensions'){$script:filesByDimension}else{$script:filesByName}).Clone()
    if(!$Ascending){[array]::Reverse($expectedFiles)}
    $filesOnly=@($names|Where-Object{!$script:folderSet.Contains($_)})
    $filesByValue=(($filesOnly -join '|') -ceq ($expectedFiles -join '|'))
    $expectedMixed=$null
    if($Column.Label-eq'Name'){$mixedOrder=[string[]]$script:allByName.Clone();if(!$Ascending){[array]::Reverse($mixedOrder)};$expectedMixed=(($names -join '|') -ceq ($mixedOrder -join '|'))}
    $placement=Describe-Placement $names $script:folderSet
    # CompareIDs calls made by the instrumented copies during this sort (0 for the shipped DLL, which is not instrumented).
    $compares=(Get-CompareCount)-$comparesBefore
    $record=[pscustomobject]@{Run=$Run;Column=$Column.Label;Direction=$direction;Round=$Round;CallMs=[Math]::Round([double]$call,1);LastChangeMs=[Math]::Round([double]$lastChange,0);CompleteAtMs=$completeAt;IncompletePolls=$incompletePolls;Polls=$polls;Changes=$changes;CompareIds=$compares;Count=$names.Count;FilesByValue=$filesByValue;Placement=$placement;NameOrderMixed=$expectedMixed;Order=($names -join '|')}
    $observations.Add($record)
    Diag ('run='+$Run+' column='+$Column.Label+' '+$direction+' round='+$Round+' call='+$record.CallMs+'ms lastChange='+$record.LastChangeMs+'ms completeAt='+$completeAt+'ms incompletePolls='+$incompletePolls+' polls='+$polls+' reorders='+$changes+' compareIds='+$compares+' items='+$names.Count+' filesByValue='+$filesByValue+' folders='+$placement+$(if($null-ne$expectedMixed){' nameOrderMixed='+$expectedMixed}else{''}))
    Diag ('order '+$Run+' '+$Column.Label+' '+$direction+': '+($names -join ' '))
    return $record
}
try {
    Initialize-Ui 'slowcol' -FixtureParent (Join-Path $ProjectRoot 'build')
    Set-UiPreference 'KeepSourceWindowOpen' 1
    # Fixture: 256 PNG files and 16 folders (odd indices below 32), named so that
    # the Name order interleaves folders and files, and the Dimensions order
    # (distinct widths, all three digits) differs from the Name order.
    $script:items=@()
    $fileIndex=0
    for($index=0;$index-lt272;$index++) {
        $isFolder=($index%2-eq1) -and ($index-lt32)
        if($isFolder){$script:items+=[pscustomobject]@{Name=('item-{0:d3}'-f$index);Folder=$true;Width=0;Height=0};continue}
        $width=100+(($fileIndex*97)%256);$height=100+(($fileIndex*11)%256)
        $script:items+=[pscustomobject]@{Name=('item-{0:d3}.png'-f$index);Folder=$false;Width=$width;Height=$height}
        $fileIndex++
    }
    $script:folderSet=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach($item in $script:items){if($item.Folder){[void]$script:folderSet.Add($item.Name)}}
    $script:filesByDimension=[string[]]@($script:items|Where-Object{!$_.Folder}|Sort-Object {[int]$_.Width}|ForEach-Object Name)
    $script:filesByName=[string[]]@($script:items|Where-Object{!$_.Folder}|ForEach-Object Name);[array]::Sort($script:filesByName,[StringComparer]::OrdinalIgnoreCase)
    $script:allByName=[string[]]@($script:items|ForEach-Object Name);[array]::Sort($script:allByName,[StringComparer]::OrdinalIgnoreCase)
    function New-Fixture([string]$Name) {
        $folder=Join-Path $script:UiRoot $Name;[void][IO.Directory]::CreateDirectory($folder)
        foreach($item in $script:items) {
            $path=Join-Path $folder $item.Name
            if($item.Folder){[void][IO.Directory]::CreateDirectory($path);continue}
            $bitmap=New-Object Drawing.Bitmap $item.Width,$item.Height
            try{$bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
        }
        return $folder
    }
    $probeFolder=New-Fixture 'probe'
    # Fact 1: which columns does Windows itself mark SLOW / PREFER_VARCMP on this folder?
    $nativeStates=@([FileSpacerRegression.SlowColumnProbe]::ColumnStates($probeFolder,1000))
    [IO.File]::WriteAllLines((Join-Path $env:FS_REGRESSION_RUN 'native-columns.txt'),[string[]]@($nativeStates|ForEach-Object{$_.ToString()}))
    $slowCount=@($nativeStates|Where-Object Slow).Count
    Diag ('native CFSFolder columns='+$nativeStates.Count+' slow='+$slowCount+' varcmp='+@($nativeStates|Where-Object PreferVarCmp).Count+' fmtcmp='+@($nativeStates|Where-Object PreferFmtCmp).Count+' (full list: native-columns.txt)')
    $interesting=@{}
    foreach($state in $nativeStates){
        $key=$state.Format.ToString('D')+':'+$state.Id
        if($key -in @(($imageSummary.ToString('D')+':13'),($storage.ToString('D')+':10'),($storage.ToString('D')+':14'),($storage.ToString('D')+':12'),($storage.ToString('D')+':4'))){$interesting[$key]=$state;Diag ('native column '+$state.ToString())}
    }
    $dimensionsState=$interesting[($imageSummary.ToString('D')+':13')]
    Assert-Ui ($null-ne$dimensionsState) 'The native folder exposes no Dimensions column (PKEY_Image_Dimensions).'
    Diag ('Dimensions native state: slow='+$dimensionsState.Slow+' varcmp='+$dimensionsState.PreferVarCmp+' fmtcmp='+$dimensionsState.PreferFmtCmp+'; the slow variant keeps VARCMP only if slow=True and varcmp=True')
    $probeIndices=[uint32[]]@($interesting.Values|ForEach-Object Index)
    foreach($runName in @($providers.Keys)) {
        $mixed=($runName -ne 'native')
        # Keep executable + fixture paths short enough for Shell relaunch properties.
        $application=Join-Path $ProjectRoot ('build\ip-'+[guid]::NewGuid().ToString('N').Substring(0,8))
        [void][IO.Directory]::CreateDirectory($application)
        Copy-Item -LiteralPath (Join-Path $ProjectRoot 'build\debug\FileSpacer.exe') -Destination (Join-Path $application 'FileSpacer.exe')
        $script:UiExecutable=Join-Path $application 'FileSpacer.exe'
        $provider=Join-Path $application 'FileSpacerFolder.dll'
        Copy-Item -LiteralPath $providers[$runName] -Destination $provider
        if($mixed) {
            [void](New-Item -Path ($providerKey+'\InprocServer32') -Force)
            Set-Item -LiteralPath ($providerKey+'\InprocServer32') -Value $provider
            New-ItemProperty -LiteralPath ($providerKey+'\InprocServer32') -Name ThreadingModel -Value Apartment -PropertyType String -Force|Out-Null
        }
        Set-UiPreference 'MixedFolderSorting' $(if($mixed){1}else{0})
        # The instrumented copies count per process: start each run from zero.
        Set-UiPreference 'ProbeCompareIds' 0
        $folder=New-Fixture ('items-'+$runName)
        Write-Host ('STEP: run '+$runName+' provider='+$providers[$runName]+' mixed='+$mixed)
        $window=Open-UiFolder $folder $script:items.Count
        Wait-Provider $window $mixed
        if($mixed) {
            $registered=(Get-Item -LiteralPath ($providerKey+'\InprocServer32')).GetValue('')
            Assert-Ui ($registered -ceq $provider) ('The application did not register the run''s DLL: '+$registered)
            # Fact 2: states the shown provider reports for the interesting columns (through the COM proxy).
            try {
                $pointer=Use-UiView $window {param($view) $view.FolderInterface([guid]'93F2F68C-1D1B-11d3-A30E-00C04F79ABD1')}
                foreach($state in @([FileSpacerRegression.SlowColumnProbe]::FolderColumnStates($pointer,$probeIndices))){Diag ('provider('+$runName+') column '+$state.ToString())}
            } catch { Diag ('provider('+$runName+') column states unavailable through the proxy: '+$_.Exception.Message) }
        }
        Use-UiView $window {param($view) $view.SetViewModeAndIconSize(4,48);$view.SortBy($false,$true);$view.ShowColumn($imageSummary,[uint32]13)}
        $visible=Use-UiView $window {param($view) $view.VisibleColumns()}
        Assert-Ui ($visible -contains ($imageSummary.ToString('D')+':13')) ('Dimensions column is not visible: '+($visible -join ' '))
        Diag ('run='+$runName+' visible columns: '+($visible -join ' '))
        $watch=[Diagnostics.Stopwatch]::StartNew();[void](Use-UiView $window {param($view) $view.Mode});Diag ('run='+$runName+' cross-process probe baseline='+[Math]::Round($watch.Elapsed.TotalMilliseconds,1)+'ms')
        $records=@{}
        foreach($round in 1..2){foreach($ascending in @($true,$false)){$records[('Dimensions-'+$round+'-'+$ascending)]=Measure-Sort $window $columns[0] $ascending $runName $round}}
        foreach($ascending in @($true,$false)){$records[('Name-1-'+$ascending)]=Measure-Sort $window $columns[1] $ascending $runName 1}
        $summary=[ordered]@{Run=$runName;DimensionsAscFolders=$records['Dimensions-2-True'].Placement;DimensionsDescFolders=$records['Dimensions-2-False'].Placement;DimensionsFilesByValue=($records['Dimensions-2-True'].FilesByValue -and $records['Dimensions-2-False'].FilesByValue);NameAscMixed=$records['Name-1-True'].NameOrderMixed;NameDescMixed=$records['Name-1-False'].NameOrderMixed;NameAscFolders=$records['Name-1-True'].Placement;NameDescFolders=$records['Name-1-False'].Placement}
        $verdicts.Add((($summary.GetEnumerator()|ForEach-Object{$_.Key+'='+$_.Value}) -join ' '))
        Diag ('SUMMARY '+$verdicts[$verdicts.Count-1])
        Close-UiFolder $window
        $last=$script:UiProcesses[$script:UiProcesses.Count-1]
        Wait-Ui { $last.HasExited } ('application exit after run '+$runName) 0 30
    }
    # Evidence file; the DIAG lines above already carry every observation.
    try {
        $rows=@(foreach($record in $observations){$row=@{};foreach($property in $record.PSObject.Properties){$row[$property.Name]=if($null-eq$property.Value){$null}else{[string]$property.Value}};$row})
        ConvertTo-Json -InputObject $rows -Depth 3 | Set-Content -LiteralPath (Join-Path $env:FS_REGRESSION_RUN 'sort-observations.json') -Encoding UTF8
    } catch { Diag ('sort-observations.json not written: '+$_.Exception.Message) }
    # Verdict: the current DLL defines the expected mixing; each variant must reproduce it.
    $byRun=@{};foreach($line in $verdicts){$parts=@{};foreach($pair in $line.Split(' ')){$k,$v=$pair.Split('=',2);$parts[$k]=$v};$byRun[$parts['Run']]=$parts}
    $reference=$byRun['current']
    Assert-Ui ($reference['DimensionsFilesByValue'] -eq 'True' -and $reference['NameAscMixed'] -eq 'True' -and $reference['NameDescMixed'] -eq 'True') ('The current provider does not produce the expected mixed order: '+$verdicts[0])
    $problems=@()
    foreach($runName in @('counted','slow','all')) {
        $run=$byRun[$runName]
        if($run['DimensionsFilesByValue'] -ne 'True'){$problems+=($runName+': files are not ordered by Dimensions value')}
        if($run['DimensionsAscFolders'] -ne $reference['DimensionsAscFolders'] -or $run['DimensionsDescFolders'] -ne $reference['DimensionsDescFolders']){$problems+=($runName+': folder placement on Dimensions ('+$run['DimensionsAscFolders']+'/'+$run['DimensionsDescFolders']+') differs from the current provider ('+$reference['DimensionsAscFolders']+'/'+$reference['DimensionsDescFolders']+')')}
        if($run['NameAscMixed'] -ne 'True' -or $run['NameDescMixed'] -ne 'True'){$problems+=($runName+': Name order is not the mixed order (asc='+$run['NameAscMixed']+' desc='+$run['NameDescMixed']+', folders '+$run['NameAscFolders']+'/'+$run['NameDescFolders']+')')}
    }
    Diag ('native baseline: Dimensions folders '+$byRun['native']['DimensionsAscFolders']+'/'+$byRun['native']['DimensionsDescFolders']+', Name folders '+$byRun['native']['NameAscFolders']+'/'+$byRun['native']['NameDescFolders'])
    Assert-Ui (!$problems.Count) ('Variants do not reproduce the expected mixing: '+($problems -join '; '))
    Write-Host 'PASS: both provider variants keep the mixed order of the current comparator on Dimensions and Name; see DIAG lines and sort-observations.json for timings.'
} catch {$failure=$_;Write-Host $_.Exception.ToString()} finally {
    try {Stop-Ui -Failure $failure}catch{$cleanupErrors.Add($_.Exception.Message)}
    try {
        if(Test-Path -LiteralPath $providerKey){Remove-Item -LiteralPath $providerKey -Recurse -Force}
        if($registrationExisted){& reg.exe import $backup|Out-Host;if($LASTEXITCODE){throw 'Provider registry restore failed.'}}
        if(!$junctionExisted -and [IO.Directory]::Exists($junction)){[IO.Directory]::Delete($junction,$false)}
        Assert-Ui ((Test-Path -LiteralPath $providerKey) -eq $registrationExisted) 'Provider registration was not restored.'
        Assert-Ui ([IO.Directory]::Exists($junction) -eq $junctionExisted) 'The test junction was not cleaned up.'
    } catch {$cleanupErrors.Add($_.Exception.Message)}
}
if($failure){foreach($errorMessage in $cleanupErrors){Write-Host ('Cleanup failure: '+$errorMessage)};throw $failure}
if($cleanupErrors.Count){throw ($cleanupErrors -join '; ')}
