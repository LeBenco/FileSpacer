# Test-Category: Internal
# Dialog mnemonics must be unique within one dialog, in every language,
# including the labels the application substitutes at run time (each
# exclusive state of a button is checked separately). Pure PowerShell: it
# reads src\dialog.*.rc and src\resource.*.rc, with no compiler or Perl.
param([string]$ProjectRoot=(Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path)
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$source=Join-Path $ProjectRoot 'src'
$languages=@('en','fr','de','it','es','pt','el','pl','nl','uk')
# Dialog => list of states; a state maps a control to the string replacing its static text.
$states=@{
    IDD_SETTINGS_DISPLAY=@(@{IDC_FOLDER_WINDOW_WIDTH_LABEL='IDS_FOLDER_WIDTH_LABEL';IDC_FOLDER_WINDOW_HEIGHT_LABEL='IDS_FOLDER_HEIGHT_LABEL'})
    IDD_SETTINGS_ABOUT=@(@{IDC_UPDATES_LINK='IDS_UPDATE_SEARCH_BUTTON'},@{IDC_UPDATES_LINK='IDS_UPDATE_INSTALL_BUTTON'},@{IDC_UPDATES_LINK='IDS_UPDATE_RELEASE_BUTTON'})
    IDD_SHORTCUT_EDITOR=@(@{IDC_SHORTCUT_RECORD='IDS_SHORTCUT_RECORD'},@{IDC_SHORTCUT_RECORD='IDS_SHORTCUT_STOP_RECORD'})
    IDD_UPDATE_PROGRESS=@(@{IDCANCEL='IDS_UPDATE_CLOSE_BUTTON'})
}
$controlKeywords='CONTROL|LTEXT|RTEXT|CTEXT|PUSHBUTTON|DEFPUSHBUTTON|EDITTEXT|COMBOBOX|GROUPBOX|LISTBOX|CHECKBOX|AUTOCHECKBOX|RADIOBUTTON|AUTORADIOBUTTON'
function Read-Resource([string]$Path) {
    if(!(Test-Path -LiteralPath $Path -PathType Leaf)){throw ('Missing resource script: '+$Path)}
    $text=[IO.File]::ReadAllText($Path,[Text.Encoding]::UTF8).TrimStart([char]0xFEFF).Replace("`r`n","`n")
    return [regex]::Replace($text,'(?ms)^#if 0\b.*?^#endif[^\n]*\n','')
}
function Get-Mnemonics([string]$Text) {
    $result=@()
    foreach($match in [regex]::Matches($Text.Replace('&&',''),'&(.)')){$result+=$match.Groups[1].Value.ToLowerInvariant()}
    return $result
}
$duplicates=New-Object Collections.Generic.List[string]
$checked=0;$mnemonicsSeen=0;$substituted=0;$dialogsSeen=@{}
foreach($language in $languages) {
    $strings=@{}
    $resource=(Read-Resource (Join-Path $source ('resource.'+$language+'.rc'))).Replace('\n','')
    foreach($match in [regex]::Matches($resource,'\b(IDS_\w+)\s*,?\s*"((?:[^"]|"")*)"')){$strings[$match.Groups[1].Value]=$match.Groups[2].Value}
    $dialogs=Read-Resource (Join-Path $source ('dialog.'+$language+'.rc'))
    foreach($part in [regex]::Split($dialogs,'(?m)^(?=IDD_\w+\s+DIALOG)')) {
        $header=[regex]::Match($part,'^(IDD_\w+)\s+DIALOG')
        if(!$header.Success){continue}
        $dialog=$header.Groups[1].Value;$dialogsSeen[$dialog]=$true
        $controls=@()
        foreach($line in $part.Split("`n")) {
            if($line -notmatch ('^\s*('+$controlKeywords+')\b')){continue}
            $text='';$textMatch=[regex]::Match($line,'^\s*\w+\s+"((?:[^"]|"")*)"');if($textMatch.Success){$text=$textMatch.Groups[1].Value}
            $idMatch=[regex]::Match($line,'\b(IDC_\w+|IDOK|IDCANCEL)\b');$id=if($idMatch.Success){$idMatch.Groups[1].Value}else{'-1'}
            $controls+=,@($id,$text)
        }
        $dialogStates=if($states.ContainsKey($dialog)){$states[$dialog]}else{@(@{})}
        $stateIndex=0
        foreach($state in $dialogStates) {
            $byMnemonic=@{}
            foreach($control in $controls) {
                $id=$control[0];$text=$control[1]
                if($state.ContainsKey($id)) {
                    if(!$strings.ContainsKey($state[$id])){throw ('Missing runtime label '+$state[$id]+' in resource.'+$language+'.rc for '+$dialog+'/'+$id)}
                    $text=$strings[$state[$id]];$substituted++
                }
                foreach($mnemonic in (Get-Mnemonics $text)) {
                    $mnemonicsSeen++
                    if(!$byMnemonic.ContainsKey($mnemonic)){$byMnemonic[$mnemonic]=@()}
                    $byMnemonic[$mnemonic]+=($id+' "'+$text+'"')
                }
            }
            foreach($mnemonic in @($byMnemonic.Keys|Sort-Object)) {
                if($byMnemonic[$mnemonic].Count -le 1){continue}
                $dynamic=@($byMnemonic[$mnemonic]|Where-Object{$entry=$_;@($state.Keys|Where-Object{$entry.StartsWith($_+' ')}).Count}).Count -gt 0
                $kind=if($dynamic){'DYNAMIC'}else{'STATIC'}
                $duplicates.Add($kind+' '+$language+' '+$dialog+' state'+$stateIndex+" '&"+$mnemonic+"': "+($byMnemonic[$mnemonic] -join ' | '))
            }
            $stateIndex++;$checked++
        }
    }
}
foreach($dialog in $states.Keys){if(!$dialogsSeen.ContainsKey($dialog)){throw ('Runtime-label dialog not found in the dialog scripts: '+$dialog)}}
foreach($line in $duplicates){Write-Host ('DUPLICATE '+$line)}
Write-Host ('Checked '+$checked+' dialog states across '+$languages.Count+' languages and '+$dialogsSeen.Count+' dialogs; mnemonics='+$mnemonicsSeen+' runtime labels substituted='+$substituted+' duplicates='+$duplicates.Count)
# A silent parser would pass trivially: the scripts really contain mnemonics,
# and every runtime label of every state was substituted in every language.
$expectedSubstitutions=0;foreach($list in $states.Values){foreach($state in $list){$expectedSubstitutions+=$state.Count}};$expectedSubstitutions*=$languages.Count
if($mnemonicsSeen -lt 10*$languages.Count){throw ('The dialog scripts yielded too few mnemonics: '+$mnemonicsSeen)}
if($substituted -ne $expectedSubstitutions){throw ('Runtime labels substituted '+$substituted+' times, expected '+$expectedSubstitutions+': a state names an absent control.')}
if($duplicates.Count){throw ('Duplicate dialog mnemonics: '+$duplicates.Count)}
Write-Host 'PASS: every dialog has unique mnemonics in all ten languages, runtime-substituted labels included.'
