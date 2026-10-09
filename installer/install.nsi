; -*- coding: utf-8 -*-
; FileSpacer, Windows 10+ x64. Compile from the project root through NMAKE.
Unicode True
Name "FileSpacer"
OutFile "..\build\FileSpacer-install.exe"
SetCompressor /SOLID lzma
ManifestDPIAware True
ManifestSupportedOS Win10
ShowInstDetails show
ShowUninstDetails show

!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "Software\FileSpacer"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "InstallMode"
!define MULTIUSER_INSTALLMODE_INSTDIR "FileSpacer"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\FileSpacer"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "Install_Dir"
!include MultiUser.nsh
!include x64.nsh
!include EnumUsersReg.nsh
!addplugindir /x86-unicode "plugins"

!define APP_ID "FileSpacer.FileSpacer"
!define EXECUTE_GUID "{c79db990-30a8-47e7-9f84-9db1316ae89b}"
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FileSpacer"
!define REG_APPPATH_KEY "Software\Microsoft\Windows\CurrentVersion\App Paths\FileSpacer.exe"
!getdllversion /productversion "..\build\release\FileSpacer.exe" PRODUCT_VERSION_
!define PRODUCT_VERSION "${PRODUCT_VERSION_1}.${PRODUCT_VERSION_2}.${PRODUCT_VERSION_3}"
VIProductVersion "${PRODUCT_VERSION}.${PRODUCT_VERSION_4}"
VIAddVersionKey /LANG=1033 "ProductName" "FileSpacer"
VIAddVersionKey /LANG=1033 "FileDescription" "FileSpacer installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 LeBenco; original ChromaFiler (c) 2023 J. van't Hoog"
BrandingText "FileSpacer ${PRODUCT_VERSION} (x64)"

!define MUI_ICON "..\src\res\FileSpacer.ico"
!define MUI_UNICON "..\src\res\FileSpacer.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
Page Custom LockedListShow
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage Custom un.LockedListShow
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"

LangString SEC_START ${LANG_ENGLISH} "Start Menu shortcut"
LangString SEC_START ${LANG_FRENCH} "Raccourci dans le menu Démarrer"
LangString SEC_CONTEXT ${LANG_ENGLISH} "Folder context menu"
LangString SEC_CONTEXT ${LANG_FRENCH} "Menu contextuel des dossiers"
LangString DESC_BASE ${LANG_ENGLISH} "FileSpacer and its licenses."
LangString DESC_BASE ${LANG_FRENCH} "FileSpacer et ses licences."
LangString DESC_START ${LANG_ENGLISH} "Add FileSpacer to the Start Menu."
LangString DESC_START ${LANG_FRENCH} "Ajouter FileSpacer au menu Démarrer."
LangString DESC_CONTEXT ${LANG_ENGLISH} "Add 'Open in FileSpacer' to folders and drives. Required for the default browser option; does not enable it."
LangString DESC_CONTEXT ${LANG_FRENCH} "Ajouter « Ouvrir dans FileSpacer » aux dossiers et lecteurs. Nécessaire pour l'option de navigateur par défaut, sans l'activer."
LangString CONTEXT_TEXT ${LANG_ENGLISH} "Open in FileSpacer"
LangString CONTEXT_TEXT ${LANG_FRENCH} "Ouvrir dans FileSpacer"
LangString CLOSE_TITLE ${LANG_ENGLISH} "Close FileSpacer"
LangString CLOSE_TITLE ${LANG_FRENCH} "Fermer FileSpacer"
LangString CLOSE_TEXT ${LANG_ENGLISH} "Close all FileSpacer windows before continuing."
LangString CLOSE_TEXT ${LANG_FRENCH} "Fermez toutes les fenêtres FileSpacer avant de continuer."
LangString ERROR_OS ${LANG_ENGLISH} "FileSpacer requires Windows 10 or later, 64-bit."
LangString ERROR_OS ${LANG_FRENCH} "FileSpacer nécessite Windows 10 ou ultérieur, 64 bits."
LangString ERROR_INSTALL ${LANG_ENGLISH} "Installation failed. Close FileSpacer and check access to the installation folder."
LangString ERROR_INSTALL ${LANG_FRENCH} "L'installation a échoué. Fermez FileSpacer et vérifiez l'accès au dossier d'installation."
LangString ERROR_UNINSTALL ${LANG_ENGLISH} "Uninstallation failed. Close FileSpacer and try again."
LangString ERROR_UNINSTALL ${LANG_FRENCH} "La désinstallation a échoué. Fermez FileSpacer puis réessayez."
LangString ERROR_SCOPE ${LANG_ENGLISH} "An installation already exists in a different mode. Keep its mode and location, or uninstall it before changing them."
LangString ERROR_SCOPE ${LANG_FRENCH} "Une installation existe déjà dans un autre mode. Conservez son mode et son emplacement, ou désinstallez-la avant de les changer."

Var locked
Var uninstallDirectory
!include ShortcutAppID.nsh

Function .onInit
    ${IfNot} ${RunningX64}
    ${OrIfNot} ${AtLeastWin10}
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_OS) /SD IDOK
        SetErrorLevel 2
        Quit
    ${EndIf}
    SetRegView 64
    !insertmacro MULTIUSER_INIT
    InitPluginsDir
    ; The NSIS host is x86; this worker lets LockedList inspect x64 processes.
    File /oname=$PLUGINSDIR\LockedList64.dll "plugins\LockedList64.dll"
FunctionEnd

Function un.onInit
    SetRegView 64
    StrCpy $uninstallDirectory $INSTDIR
    ; Use the mode belonging to this uninstaller, even if both scopes exist.
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Installation" "Mode"
    ReadINIStr $1 "$INSTDIR\install-info.ini" "Installation" "Language"
    ${If} $1 == ${LANG_ENGLISH}
    ${OrIf} $1 == ${LANG_FRENCH}
        StrCpy $LANGUAGE $1
    ${EndIf}
    ${If} $0 != "AllUsers"
    ${AndIf} $0 != "CurrentUser"
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_UNINSTALL) /SD IDOK
        SetErrorLevel 2
        Quit
    ${EndIf}
    StrCpy $CMDLINE '"$EXEPATH" /$0'
    !insertmacro MULTIUSER_UNINIT
    StrCpy $INSTDIR $uninstallDirectory
    InitPluginsDir
    File /oname=$PLUGINSDIR\LockedList64.dll "plugins\LockedList64.dll"
FunctionEnd

!macro LOCKED_PAGE PREFIX
Function ${PREFIX}LockedListShow
    !insertmacro MUI_HEADER_TEXT $(CLOSE_TITLE) $(CLOSE_TEXT)
    LockedList::AddModule "$INSTDIR\FileSpacer.exe"
    LockedList::Dialog /autonext
    Pop $R0
FunctionEnd
Function ${PREFIX}SearchCallback
    Pop $R0 ; process ID
    Pop $R1 ; path
    Pop $R2 ; description
    StrCpy $locked 1
    Push false ; Stop searching. Never terminate a process.
FunctionEnd
Function ${PREFIX}CheckClosed
    StrCpy $locked 0
    LockedList::AddModule "$INSTDIR\FileSpacer.exe"
    GetFunctionAddress $R0 ${PREFIX}SearchCallback
    LockedList::SilentSearch $R0
    Pop $R0
    ${If} $locked != 0
    ${OrIf} $R0 != "done"
        DetailPrint $(CLOSE_TEXT)
        MessageBox MB_OK|MB_ICONSTOP $(CLOSE_TEXT) /SD IDOK
        SetErrorLevel 2
        Abort
    ${EndIf}
FunctionEnd
!macroend
!insertmacro LOCKED_PAGE ""
!insertmacro LOCKED_PAGE "un."

!macro INSTALL_VERB TYPE ARG
    ; Avoid making an added verb implicitly become the default.
    ReadRegStr $0 SHCTX "Software\Classes\${TYPE}\shell" ""
    ClearErrors ; A missing default value is normal for a new registration.
    ${If} $0 == ""
        WriteRegStr SHCTX "Software\Classes\${TYPE}\shell" "" "none"
    ${EndIf}
    WriteRegStr SHCTX "Software\Classes\${TYPE}\shell\filespacer" "" $(CONTEXT_TEXT)
    WriteRegStr SHCTX "Software\Classes\${TYPE}\shell\filespacer" "Icon" '"$INSTDIR\FileSpacer.exe",0'
    WriteRegStr SHCTX "Software\Classes\${TYPE}\shell\filespacer\command" "" '"$INSTDIR\FileSpacer.exe" "${ARG}"'
    WriteRegStr SHCTX "Software\Classes\${TYPE}\shell\filespacer\command" "DelegateExecute" "${EXECUTE_GUID}"
    IfErrors context_failed
!macroend

!macro CLEAR_CLASSES_DEFAULT ROOT PREFIX TYPE
    ReadRegStr $R0 ${ROOT} "${PREFIX}${TYPE}\shell" ""
    ${If} $R0 == "filespacer"
        WriteRegStr ${ROOT} "${PREFIX}${TYPE}\shell" "" "none"
    ${EndIf}
!macroend
!macro CLEAR_DEFAULT ROOT PREFIX TYPE
    !insertmacro CLEAR_CLASSES_DEFAULT ${ROOT} "${PREFIX}Software\Classes\" "${TYPE}"
!macroend

!macro REMOVE_VERBS
    DeleteRegKey SHCTX "Software\Classes\Directory\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\Directory\Background\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\CompressedFolder\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\Drive\shell\filespacer"
!macroend

!macro CLEANUP_FUNCTIONS PREFIX
Function ${PREFIX}CleanupCurrentUser
    !insertmacro CLEAR_DEFAULT HKCU "" "Directory"
    !insertmacro CLEAR_DEFAULT HKCU "" "CompressedFolder"
    !insertmacro CLEAR_DEFAULT HKCU "" "Drive"
FunctionEnd
Function ${PREFIX}CleanupUser
    Pop $0
    StrCpy $1 $0 8 -8
    ${If} $1 == "_Classes"
        ; UsrClass.dat contains the Classes subtree itself, not Software\Classes.
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "Directory"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "CompressedFolder"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "Drive"
    ${Else}
        !insertmacro CLEAR_DEFAULT HKU "$0\" "Directory"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "CompressedFolder"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "Drive"
    ${EndIf}
FunctionEnd
!macroend
!insertmacro CLEANUP_FUNCTIONS ""
!insertmacro CLEANUP_FUNCTIONS "un."
!insertmacro _EnumUsersReg ""

!macro CLEANUP_DEFAULTS PREFIX
    !insertmacro CLEAR_DEFAULT SHCTX "" "Directory"
    !insertmacro CLEAR_DEFAULT SHCTX "" "CompressedFolder"
    !insertmacro CLEAR_DEFAULT SHCTX "" "Drive"
    ${If} $MultiUser.InstallMode == "CurrentUser"
        Call ${PREFIX}CleanupCurrentUser
    ${Else}
        !insertmacro EnumUsersReg "${PREFIX}" ${PREFIX}CleanupUser filespacer.temp
    ${EndIf}
!macroend

Section "FileSpacer" SecBase
    SectionIn RO
    ; Prevent parallel installations from masking each other's COM registration.
    ${If} $MultiUser.InstallMode == "AllUsers"
        ReadRegStr $0 HKCU "Software\FileSpacer" "Install_Dir"
    ${Else}
        ReadRegStr $0 HKLM "Software\FileSpacer" "Install_Dir"
    ${EndIf}
    ${If} $0 != ""
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_SCOPE) /SD IDOK
        SetErrorLevel 2
        Abort
    ${EndIf}
    ReadRegStr $1 SHCTX "Software\FileSpacer" "Install_Dir"
    ${If} $1 != ""
    ${AndIf} $1 != $INSTDIR
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_SCOPE) /SD IDOK
        SetErrorLevel 2
        Abort
    ${EndIf}
    Call CheckClosed ; Also covers silent setup and launches after the custom page.
    ClearErrors
    SetOutPath "$INSTDIR"
    IfErrors install_failed
    SetOverwrite on
    File "..\build\release\FileSpacer.exe"
    File /oname=LICENSE.txt "..\LICENSE"
    File /oname=LockedList-LICENSE.txt "plugins\LockedList.txt"
    File /oname=Lucide-LICENSE.txt "..\licenses\Lucide-LICENSE.txt"
    File /oname=SQLite-NOTICE.txt "..\third_party\sqlite\README.txt"
    File /oname=THIRD_PARTY_NOTICES.md "..\THIRD_PARTY_NOTICES.md"
    File /oname=privacy-policy.md "..\docs\privacy-policy.md"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Mode" $MultiUser.InstallMode
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Language" $LANGUAGE
    IfErrors install_failed

    WriteRegStr SHCTX "Software\FileSpacer" "InstallMode" $MultiUser.InstallMode
    WriteRegStr SHCTX "Software\FileSpacer" "Install_Dir" "$INSTDIR"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayName" "FileSpacer"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\FileSpacer.exe",0'
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "VersionMajor" ${PRODUCT_VERSION_1}
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "VersionMinor" ${PRODUCT_VERSION_2}
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe" /$MultiUser.InstallMode'
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /$MultiUser.InstallMode /S'
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "NoRepair" 1
    SectionGetSize ${SecBase} $0
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "EstimatedSize" $0
    WriteRegStr SHCTX "${REG_APPPATH_KEY}" "" "$INSTDIR\FileSpacer.exe"
    WriteRegStr SHCTX "${REG_APPPATH_KEY}" "DropTarget" "${EXECUTE_GUID}"
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}" "" "FileSpacer"
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}\LocalServer32" "" '"$INSTDIR\FileSpacer.exe"'
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}\LocalServer32" "ServerExecutable" "$INSTDIR\FileSpacer.exe"
    IfErrors install_failed
    Goto install_done
install_failed:
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
    SetErrorLevel 2
    Abort
install_done:
SectionEnd

Section "$(SEC_START)" SecStart
    ClearErrors
    CreateShortcut /NoWorkingDir "$SMPROGRAMS\FileSpacer.lnk" "$INSTDIR\FileSpacer.exe"
    IfErrors shortcut_failed
    Call SetShortcutAppID
    Pop $0
    ${If} $0 != 0
shortcut_failed:
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
        SetErrorLevel 2
        Abort
    ${EndIf}
SectionEnd

Section "$(SEC_CONTEXT)" SecContext
    ClearErrors
    !insertmacro INSTALL_VERB "Directory" "%1"
    !insertmacro INSTALL_VERB "Directory\Background" "%V"
    !insertmacro INSTALL_VERB "CompressedFolder" "%1"
    !insertmacro INSTALL_VERB "Drive" "%1"
    Goto context_done
context_failed:
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
    SetErrorLevel 2
    Abort
context_done:
SectionEnd

Section -ApplyOptions
    ; An unchecked component also removes a component installed by an earlier version.
    ${IfNot} ${SectionIsSelected} ${SecStart}
        Delete "$SMPROGRAMS\FileSpacer.lnk"
    ${EndIf}
    ${IfNot} ${SectionIsSelected} ${SecContext}
        !insertmacro REMOVE_VERBS
        !insertmacro CLEANUP_DEFAULTS ""
    ${EndIf}
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
    Call un.CheckClosed
    ; Delete owned files only. No wildcards and no recursive installation-directory deletion.
    ClearErrors
    IfFileExists "$INSTDIR\FileSpacer.exe" 0 exe_removed
    Delete "$INSTDIR\FileSpacer.exe"
    IfErrors uninstall_failed ; Keep the registration when the executable cannot be removed.
exe_removed:
    Delete "$INSTDIR\LICENSE.txt"
    Delete "$INSTDIR\LockedList-LICENSE.txt"
    Delete "$INSTDIR\Lucide-LICENSE.txt"
    Delete "$INSTDIR\SQLite-NOTICE.txt"
    Delete "$INSTDIR\THIRD_PARTY_NOTICES.md"
    Delete "$INSTDIR\privacy-policy.md"
    Delete "$INSTDIR\ShortcutProperties-LICENSE.txt"
    Delete "$INSTDIR\install-info.ini"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
    Delete "$SMPROGRAMS\FileSpacer.lnk"
    DeleteRegKey SHCTX "${REG_UNINST_KEY}"
    DeleteRegKey SHCTX "${REG_APPPATH_KEY}"
    DeleteRegKey SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}"
    !insertmacro REMOVE_VERBS
    !insertmacro CLEANUP_DEFAULTS "un."
    DeleteRegValue SHCTX "Software\FileSpacer" "InstallMode"
    DeleteRegValue SHCTX "Software\FileSpacer" "Install_Dir"
    DeleteRegKey /ifempty SHCTX "Software\FileSpacer"
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
    Goto uninstall_done
uninstall_failed:
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_UNINSTALL) /SD IDOK
    SetErrorLevel 2
    Abort
uninstall_done:
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecBase} $(DESC_BASE)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecStart} $(DESC_START)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecContext} $(DESC_CONTEXT)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
