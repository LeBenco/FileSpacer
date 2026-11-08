param([string]$ProjectRoot)
$ErrorActionPreference='Stop'
$source=[IO.File]::ReadAllText((Join-Path $ProjectRoot 'src/ShellViewSelection.cpp'))
function Insert-Phase([string]$Needle,[string]$Replacement) {
 if(([regex]::Matches($script:source,[regex]::Escape($Needle))).Count-ne1){throw ('Ambiguous phase insertion: '+$Needle)}
 $script:source=$script:source.Replace($Needle,$Replacement)
}
Insert-Phase '#include "ShellViewSelection.h"' "#include `"ShellViewSelection.h`"`nextern `"C`" void restorePhase(const char *name);"
Insert-Phase '    std::vector<uint8_t> bytes;' "    restorePhase(`"begin`");`n    std::vector<uint8_t> bytes;"
Insert-Phase '    if (anchor) {' "    restorePhase(`"decode`");`n    if (anchor) {"
Insert-Phase '        if (last) {' "        restorePhase(`"anchor-prepare`");`n        if (last) {"
Insert-Phase '            record(reveal(last));' "            record(reveal(last));`n            restorePhase(`"anchor-last`");"
Insert-Phase '        record(reveal(anchor));' "        record(reveal(anchor));`n        restorePhase(`"anchor-target`");"
Insert-Phase '    // Mode changes normally retain selection.' "    restorePhase(`"anchor-redraw`");`n    // Mode changes normally retain selection."
Insert-Phase '    bool useBulkSelection = clear && clear->acceleratesSelectAll();' "    restorePhase(`"same-selection`");`n    bool useBulkSelection = clear && clear->acceleratesSelectAll();"
Insert-Phase '            result = matchesViewItems(folderView, SVGIO_ALLVIEW, selectedItems, current);' "            result = matchesViewItems(folderView, SVGIO_ALLVIEW, selectedItems, current);`n            restorePhase(`"bulk-proof`");"
Insert-Phase '                result = bulk->Select(SFVS_SELECT_ALLITEMS);' "                result = bulk->Select(SFVS_SELECT_ALLITEMS);`n                restorePhase(`"bulk-call`");"
Insert-Phase '                    result = matchesViewItems(folderView, SVGIO_SELECTION, selectedItems, current);' "                    result = matchesViewItems(folderView, SVGIO_SELECTION, selectedItems, current);`n                    restorePhase(`"bulk-verify`");"
Insert-Phase '    CComPtr<IScrollProvider> scrollPattern;' "    restorePhase(`"selection`");`n    CComPtr<IScrollProvider> scrollPattern;"
Insert-Phase '    if (scrollPattern) {' "    restorePhase(`"focus`");`n    if (scrollPattern) {"
Insert-Phase '    return restored;' "    restorePhase(`"end`");`n    return restored;"
Insert-Phase '    const unsigned char *reserved=snapshot.data;' "    restorePhase(`"native-reserve`");`n    const unsigned char *reserved=snapshot.data;"
Insert-Phase '    std::memcpy(snapshot.data,construction.entries.data(),static_cast<SIZE_T>(count)*16);' "    restorePhase(`"native-build`");`n    std::memcpy(snapshot.data,construction.entries.data(),static_cast<SIZE_T>(count)*16);"
$output=Join-Path $ProjectRoot 'build/restore-free-phases'
[void][IO.Directory]::CreateDirectory($output)
[IO.File]::WriteAllText((Join-Path $output 'ShellViewSelection.cpp'),$source,[Text.UTF8Encoding]::new($false))
