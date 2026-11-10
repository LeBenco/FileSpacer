# Test-Category: Internal
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $ProjectRoot 'tools\documentation-capture\CaptureDesktop.cs') -ReferencedAssemblies System.Drawing,System.Windows.Forms
$executable=Join-Path $ProjectRoot 'build\debug\FileSpacer.exe'
$header=Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\resource.h') -Raw
$unlockId=[int][regex]::Match($header,'(?m)^#define\s+IDS_UNLOCK_WINDOW_STATE\s+(\d+)').Groups[1].Value
$ids=@('IDS_LOCK_WINDOW_STATE','IDS_UNLOCK_WINDOW_STATE','IDS_TITLE_MENU_RESTORE')|ForEach-Object {[int][regex]::Match($header,('(?m)^#define\s+'+$_+'\s+(\d+)')).Groups[1].Value}
$greek='Ξεκλείδωμα κατάστασης παραθύρου'
foreach($workerLanguage in @('en-US','fr-FR')){
    [Threading.Thread]::CurrentThread.CurrentUICulture=[Globalization.CultureInfo]::GetCultureInfo($workerLanguage)
    foreach($id in $ids){
        $english=[CaptureDesktop]::Resource($executable,$id,'en-US')
        $seen=New-Object 'Collections.Generic.HashSet[string]'
    foreach($language in @('en','fr','de','it','es','pt','el','pl','nl','uk')){
        $neutral=[CaptureDesktop]::Resource($executable,$id,$language)
        $specific=[CaptureDesktop]::Resource($executable,$id,[Globalization.CultureInfo]::CreateSpecificCulture($language).Name)
        if(!$neutral -or $neutral.Contains([string][char]0) -or $specific-cne$neutral){throw ('Invalid or inconsistent localized resource: '+$language)}
        if($language-ne'en' -and $neutral-ceq$english){throw ('Unexpected English fallback: '+$language)}
        if(!$seen.Add($neutral)){throw ('Repeated resource text for distinct capture languages: '+$language)}
    }
    }
    if([CaptureDesktop]::Resource($executable,$unlockId,'el-GR')-cne$greek){throw 'The Greek reference follows the worker or system language.'}
}
Write-Output 'PASS: actual executable resources match all ten requested languages, including Greek under English and French workers, with neutral/regional locale names and no trailing NUL.'
