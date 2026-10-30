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

RequestExecutionLevel user
InstallDir ""
!include MUI2.nsh
!include LogicLib.nsh
!include WinVer.nsh
!include FileFunc.nsh
!include x64.nsh
!include Sections.nsh
!define FILESPACER_DOCS "..\build\offline-docs"
; SPDX-License-Identifier: GPL-3.0-only; Copyright (C) 2026 Benco
!macro CleanInstalledDocumentation Mode FailureLabel
    IfErrors ${FailureLabel}
    InitPluginsDir
    File /oname=$PLUGINSDIR\FileSpacer-doc-cleanup.ps1 "RemoveDocumentation.ps1"
!if "${Mode}" == "Upgrade"
    File /oname=$PLUGINSDIR\FileSpacer-doc-current.json "${FILESPACER_DOCS}\build-info.json"
    IfErrors ${FailureLabel}
    nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\FileSpacer-doc-cleanup.ps1" -HelpRoot "$INSTDIR\help" -CurrentInventory "$PLUGINSDIR\FileSpacer-doc-current.json"'
!else
    IfErrors ${FailureLabel}
    nsExec::ExecToLog '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\FileSpacer-doc-cleanup.ps1" -HelpRoot "$INSTDIR\help"'
!endif
    Pop $0
    ${If} $0 != 0
        DetailPrint "Documentation cleanup failed: $0"
        Goto ${FailureLabel}
    ${EndIf}
    ClearErrors
!macroend
!include "..\build\offline-docs.nsh"
!include EnumUsersReg.nsh
!addplugindir /x86-unicode "plugins"

!define APP_ID "FileSpacer.FileSpacer"
!define EXECUTE_GUID "{c79db990-30a8-47e7-9f84-9db1316ae89b}"
!define REG_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\FileSpacer"
!define REG_APPPATH_KEY "Software\Microsoft\Windows\CurrentVersion\App Paths\FileSpacer.exe"
; Exit codes of the elevated worker (/MachineWorker), explained by the parent.
; 1 stays the cancellation code and 2 the generic failure of either process.
!define EXIT_CLOSED 3
!define EXIT_SCOPE 4
!define EXIT_FILES 5
!define EXIT_REGISTRY 6
!define EXIT_SHORTCUT 7
!define EXIT_CONTEXT 8
!define EXIT_OPTIONS 9
!macro WORKER_EXIT CODE
    ${If} $machineWorker == 1
        SetErrorLevel ${CODE}
    ${Else}
        SetErrorLevel 2
    ${EndIf}
!macroend
!getdllversion /productversion "..\build\release\FileSpacer.exe" PRODUCT_VERSION_
!define PRODUCT_VERSION "${PRODUCT_VERSION_1}.${PRODUCT_VERSION_2}.${PRODUCT_VERSION_3}"
VIProductVersion "${PRODUCT_VERSION}.${PRODUCT_VERSION_4}"
VIAddVersionKey /LANG=1033 "ProductName" "FileSpacer"
VIAddVersionKey /LANG=1033 "FileDescription" "FileSpacer installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 Benco; original ChromaFiler (c) 2023 J. van't Hoog"
BrandingText "FileSpacer ${PRODUCT_VERSION} (x64)"

!define MUI_ICON "..\build\installer-assets\FileSpacer-install.ico"
!define MUI_UNICON "..\src\res\FileSpacer.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_TEXTCOLOR "20252E"
!define MUI_BGCOLOR "FFFFFF"
!define MUI_CUSTOMFUNCTION_GUIINIT SetWizardArtwork
!define MUI_CUSTOMFUNCTION_UNGUIINIT un.SetWizardArtwork
Page Custom ScopePageShow ScopePageLeave
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SetPagePalette
!define MUI_LICENSEPAGE_BGCOLOR "FFFFFF"
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SetPagePalette
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SetPagePalette
!insertmacro MUI_PAGE_DIRECTORY
Page Custom LockedListShow LockedListLeave
!define MUI_PAGE_CUSTOMFUNCTION_SHOW SetPagePalette
!insertmacro MUI_PAGE_INSTFILES
Page Custom FinishPageShow
!define MUI_PAGE_CUSTOMFUNCTION_SHOW un.SetPagePalette
!insertmacro MUI_UNPAGE_CONFIRM
UninstPage Custom un.LockedListShow un.LockedListLeave
!define MUI_PAGE_CUSTOMFUNCTION_SHOW un.SetPagePalette
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "French"
ChangeUI IDD_INST "..\build\installer-assets\wizard-ui.dll"

LangString SCOPE_TITLE ${LANG_ENGLISH} "Install FileSpacer ${PRODUCT_VERSION}"
LangString SCOPE_TITLE ${LANG_FRENCH} "Installer FileSpacer ${PRODUCT_VERSION}"
LangString BRANDING_SUBTITLE ${LANG_ENGLISH} "Spatial file manager"
LangString BRANDING_SUBTITLE ${LANG_FRENCH} "Gestionnaire de fichiers$\r$\nspatial"
LangString SCOPE_SUBTITLE ${LANG_ENGLISH} "Choose who can use FileSpacer."
LangString SCOPE_SUBTITLE ${LANG_FRENCH} "Choisissez qui pourra utiliser FileSpacer."
LangString SCOPE_CURRENT ${LANG_ENGLISH} "Only for me"
LangString SCOPE_CURRENT ${LANG_FRENCH} "Pour moi uniquement"
LangString SCOPE_ALL ${LANG_ENGLISH} "All users (Windows will ask you to confirm)"
LangString SCOPE_ALL ${LANG_FRENCH} "Tous les utilisateurs (Windows demandera votre confirmation)"
LangString NOTES_TITLE ${LANG_ENGLISH} "What's new"
LangString NOTES_TITLE ${LANG_FRENCH} "Nouveautés"
LangString FINISH_TITLE ${LANG_ENGLISH} "FileSpacer is installed"
LangString FINISH_TITLE ${LANG_FRENCH} "FileSpacer est installé"
LangString FINISH_TEXT ${LANG_ENGLISH} "Installation is complete.$\r$\n$\r$\nClick Finish to close this wizard."
LangString FINISH_TEXT ${LANG_FRENCH} "L'installation est terminée.$\r$\n$\r$\nCliquez sur Terminer pour fermer l'assistant."
LangString FINISH_BUTTON ${LANG_ENGLISH} "Finish"
LangString FINISH_BUTTON ${LANG_FRENCH} "Terminer"
LangString ERROR_MACHINE ${LANG_ENGLISH} "Installation for all users could not be completed."
LangString ERROR_MACHINE ${LANG_FRENCH} "L'installation pour tous les utilisateurs n'a pas pu être terminée."
LangString SEC_START ${LANG_ENGLISH} "Start Menu shortcut"
LangString SEC_START ${LANG_FRENCH} "Raccourci dans le menu Démarrer"
LangString SEC_CONTEXT ${LANG_ENGLISH} "Folder context menu"
LangString SEC_CONTEXT ${LANG_FRENCH} "Menu contextuel des dossiers"
LangString SEC_DEFAULT ${LANG_ENGLISH} "Make FileSpacer the default file manager"
LangString SEC_DEFAULT ${LANG_FRENCH} "Définir FileSpacer comme gestionnaire de fichiers par défaut"
LangString DESC_BASE ${LANG_ENGLISH} "FileSpacer and its licenses."
LangString DESC_BASE ${LANG_FRENCH} "FileSpacer et ses licences."
LangString DESC_START ${LANG_ENGLISH} "Add FileSpacer to the Start Menu."
LangString DESC_START ${LANG_FRENCH} "Ajouter FileSpacer au menu Démarrer."
LangString DESC_CONTEXT ${LANG_ENGLISH} "Add a folder and drive menu command for the other file manager: FileSpacer or Windows File Explorer. Required for the default file manager option; does not enable it."
LangString DESC_CONTEXT ${LANG_FRENCH} "Ajouter aux dossiers et lecteurs une commande vers l'autre gestionnaire : FileSpacer ou l'explorateur Windows. Nécessaire pour l'option de gestionnaire par défaut, sans l'activer."
LangString DESC_DEFAULT ${LANG_ENGLISH} "Open folders and drives with FileSpacer instead of Windows File Explorer. You can also enable this option later or restore File Explorer at any time in FileSpacer's settings, under Integration."
LangString DESC_DEFAULT ${LANG_FRENCH} "Ouvrir les dossiers et les lecteurs avec FileSpacer à la place de l'Explorateur de fichiers Windows. Vous pouvez aussi activer cette option plus tard ou rétablir l'Explorateur à tout moment dans les paramètres de FileSpacer, onglet Intégration."
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
LangString ERROR_DEFAULT ${LANG_ENGLISH} "Unable to make FileSpacer the default file manager. You can try again in FileSpacer's settings, under Integration."
LangString ERROR_DEFAULT ${LANG_FRENCH} "Impossible de définir FileSpacer comme gestionnaire de fichiers par défaut. Vous pouvez réessayer dans les paramètres de FileSpacer, onglet Intégration."
LangString ERROR_UNINSTALL ${LANG_ENGLISH} "Uninstallation failed. Close FileSpacer and try again."
LangString ERROR_UNINSTALL ${LANG_FRENCH} "La désinstallation a échoué. Fermez FileSpacer puis réessayez."
LangString ERROR_SCOPE ${LANG_ENGLISH} "An installation already exists in a different mode. Keep its mode and location, or uninstall it before changing them."
LangString ERROR_SCOPE ${LANG_FRENCH} "Une installation existe déjà dans un autre mode. Conservez son mode et son emplacement, ou désinstallez-la avant de les changer."
LangString MACHINE_WAIT ${LANG_ENGLISH} "Installing for all users: confirm the Windows elevation prompt, then wait for the elevated installer to finish..."
LangString MACHINE_WAIT ${LANG_FRENCH} "Installation pour tous les utilisateurs en cours : validez l'invite d'élévation de Windows, puis patientez jusqu'à la fin de l'installeur élevé…"
LangString OPEN_LOG ${LANG_ENGLISH} "Open the installation log?"
LangString OPEN_LOG ${LANG_FRENCH} "Ouvrir le journal d'installation ?"
LangString ERROR_MACHINE_CLOSED ${LANG_ENGLISH} "FileSpacer or its folder provider is still loaded in a session on this computer. Close FileSpacer everywhere, then try again."
LangString ERROR_MACHINE_CLOSED ${LANG_FRENCH} "FileSpacer ou son fournisseur de dossiers est encore chargé dans une session de cet ordinateur. Fermez FileSpacer partout, puis réessayez."
LangString ERROR_MACHINE_FILES ${LANG_ENGLISH} "The elevated installer could not copy the program files. Check access to the installation folder."
LangString ERROR_MACHINE_FILES ${LANG_FRENCH} "L'installeur élevé n'a pas pu copier les fichiers du programme. Vérifiez l'accès au dossier d'installation."
LangString ERROR_MACHINE_REGISTRY ${LANG_ENGLISH} "The elevated installer could not write the registration of the program for all users."
LangString ERROR_MACHINE_REGISTRY ${LANG_FRENCH} "L'installeur élevé n'a pas pu écrire l'inscription du programme pour tous les utilisateurs."
LangString ERROR_MACHINE_SHORTCUT ${LANG_ENGLISH} "The elevated installer could not create the Start Menu shortcut."
LangString ERROR_MACHINE_SHORTCUT ${LANG_FRENCH} "L'installeur élevé n'a pas pu créer le raccourci du menu Démarrer."
LangString ERROR_MACHINE_CONTEXT ${LANG_ENGLISH} "The elevated installer could not register the folder context menu."
LangString ERROR_MACHINE_CONTEXT ${LANG_FRENCH} "L'installeur élevé n'a pas pu inscrire le menu contextuel des dossiers."
LangString ERROR_MACHINE_OPTIONS ${LANG_ENGLISH} "The elevated installer could not save the installation options."
LangString ERROR_MACHINE_OPTIONS ${LANG_FRENCH} "L'installeur élevé n'a pas pu enregistrer les options d'installation."

Var locked
Var choicesScope
Var upgrading
Var installMode
Var parameters
Var requestedDirectory
Var machineWorker
Var delegated
Var scopeCurrent
Var scopeAll
Var notesText
Var elevatedParameters
Var elevatedResult
Var brandingBitmap
Var brandingTitleFont
Var brandingCaptionFont
Var titleFont
Var processList
Var logPath

; The elevated worker runs silently: it leaves its trace in the log file the
; parent passed with /Log=, which the parent replays and offers to open.
!macro WORKER_LOG TEXT
    ${If} $machineWorker == 1
    ${AndIf} $logPath != ""
        FileOpen $R9 "$logPath" a
        ${IfNot} ${Errors}
            FileSeek $R9 0 END
            FileWriteUTF16LE $R9 "${TEXT}$\r$\n"
            FileClose $R9
        ${EndIf}
        ClearErrors
    ${EndIf}
!macroend
!include Elevation.nsh
!include ShortcutAppID.nsh

!macro WIZARD_STYLE PREFIX
Function ${PREFIX}SetWizardArtwork
    !if "${PREFIX}" == ""
        SetAutoClose true
    !endif
    InitPluginsDir
    ${If} $LANGUAGE == ${LANG_FRENCH}
        File /oname=$PLUGINSDIR\sidebar.bmp "..\build\installer-assets\wizard-fr.bmp"
    ${Else}
        File /oname=$PLUGINSDIR\sidebar.bmp "..\build\installer-assets\wizard-en.bmp"
    ${EndIf}
    System::Store S
    SetCtlColors $HWNDPARENT 20252E F1F2F6
    GetDlgItem $0 $HWNDPARENT 1034
    SetCtlColors $0 20252E FFFFFF
    System::Call 'user32::SetWindowPos(p r0,p 1,i 0,i 0,i 0,i 0,i 0x13)'
    GetDlgItem $0 $HWNDPARENT 1025
    ${NSD_SetStretchedImage} $0 "$PLUGINSDIR\sidebar.bmp" $brandingBitmap
    ; Native Segoe UI text avoids resampling glyphs with the sidebar bitmap.
    ; Match the wizard DPI and explicitly request CLEARTYPE_QUALITY (5).
    System::Call 'user32::GetDC(p $HWNDPARENT)p.r1'
    System::Call 'gdi32::GetDeviceCaps(p r1,i 90)i.r2'
    System::Call 'user32::ReleaseDC(p $HWNDPARENT,p r1)'
    System::Call 'kernel32::MulDiv(i 16,i r2,i -72)i.r3'
    System::Call 'gdi32::CreateFontW(i r3,i 0,i 0,i 0,i 600,i 0,i 0,i 0,i 0,i 0,i 0,i 5,i 0,w "Segoe UI")p.r4'
    StrCpy $brandingTitleFont $4
    System::Call 'kernel32::MulDiv(i 9,i r2,i -72)i.r3'
    System::Call 'gdi32::CreateFontW(i r3,i 0,i 0,i 0,i 400,i 0,i 0,i 0,i 0,i 0,i 0,i 5,i 0,w "Segoe UI")p.r4'
    StrCpy $brandingCaptionFont $4
    GetDlgItem $0 $HWNDPARENT 1050
    SetCtlColors $0 20252E F1F2F6
    SendMessage $0 ${WM_SETFONT} $brandingTitleFont 1
    GetDlgItem $0 $HWNDPARENT 1051
    SetCtlColors $0 69717C F1F2F6
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(BRANDING_SUBTITLE)"
    SendMessage $0 ${WM_SETFONT} $brandingCaptionFont 1
    GetDlgItem $0 $HWNDPARENT 1256
    SetCtlColors $0 69717C F1F2F6
    GetDlgItem $0 $HWNDPARENT 1028
    SetCtlColors $0 69717C F1F2F6
    CreateFont $titleFont "$(^Font)" 14 700
    GetDlgItem $0 $HWNDPARENT 1037
    SendMessage $0 ${WM_SETFONT} $titleFont 0
    System::Store L
FunctionEnd

Function ${PREFIX}SetPagePalette
    System::Store S
    System::Call 'user32::GetWindow(p $HWNDPARENT,i 5)p.r0'
    ${While} $0 != 0
        System::Call 'user32::GetClassNameW(p r0,w .r2,i ${NSIS_MAX_STRLEN})i.r9'
        ${If} $2 == "#32770"
            SetCtlColors $0 20252E FFFFFF
            System::Call 'user32::GetWindow(p r0,i 5)p.r1'
            ${While} $1 != 0
                System::Call 'user32::GetClassNameW(p r1,w .r2,i ${NSIS_MAX_STRLEN})i.r9'
                ${If} $2 == "Static"
                ${OrIf} $2 == "Button"
                ${OrIf} $2 == "Edit"
                    SetCtlColors $1 20252E FFFFFF
                ${EndIf}
                System::Call 'user32::GetWindow(p r1,i 2)p.r1'
            ${EndWhile}
        ${EndIf}
        System::Call 'user32::GetWindow(p r0,i 2)p.r0'
    ${EndWhile}
    System::Store L
FunctionEnd

Function ${PREFIX}FreeWizardArtwork
    ${If} $brandingBitmap != 0
        System::Call 'gdi32::DeleteObject(p $brandingBitmap)'
    ${EndIf}
    ${If} $titleFont != 0
        System::Call 'gdi32::DeleteObject(p $titleFont)'
    ${EndIf}
    ${If} $brandingTitleFont != 0
        System::Call 'gdi32::DeleteObject(p $brandingTitleFont)'
    ${EndIf}
    ${If} $brandingCaptionFont != 0
        System::Call 'gdi32::DeleteObject(p $brandingCaptionFont)'
    ${EndIf}
FunctionEnd
!macroend
!insertmacro WIZARD_STYLE ""
!insertmacro WIZARD_STYLE "un."

Function .onGUIEnd
    Call FreeWizardArtwork
FunctionEnd
Function un.onGUIEnd
    Call un.FreeWizardArtwork
FunctionEnd

Function FinishPageShow
    !insertmacro MUI_HEADER_TEXT $(FINISH_TITLE) ""
    System::Store S
    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(FINISH_BUTTON)"
    GetDlgItem $1 $HWNDPARENT 2
    System::Alloc 16
    Pop $2
    ${If} $2 != 0
        System::Call 'user32::GetWindowRect(p r1,p r2)'
        System::Call 'user32::MapWindowPoints(p 0,p $HWNDPARENT,p r2,i 2)'
        System::Call '*$2(i.r3,i.r4,i.r5,i.r6)'
        IntOp $5 $5 - $3
        IntOp $6 $6 - $4
        System::Call 'user32::SetWindowPos(p r0,p 0,i r3,i r4,i r5,i r6,i 0x14)'
        System::Free $2
    ${EndIf}
    ShowWindow $1 ${SW_HIDE}
    GetDlgItem $0 $HWNDPARENT 3
    ShowWindow $0 ${SW_HIDE}
    System::Store L
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateLabel} 0 12u 100% 90u "$(FINISH_TEXT)"
    Pop $0
    Call SetPagePalette
    nsDialogs::Show
FunctionEnd

Function ScopePageShow
    !insertmacro MUI_HEADER_TEXT $(SCOPE_TITLE) $(SCOPE_SUBTITLE)
    nsDialogs::Create 1018
    Pop $0
    ${If} $0 == "error"
        Abort
    ${EndIf}
    ${NSD_CreateRadioButton} 15u 4u -15u 12u "$(SCOPE_CURRENT)"
    Pop $scopeCurrent
    ${NSD_CreateRadioButton} 15u 22u -15u 14u "$(SCOPE_ALL)"
    Pop $scopeAll
    ${If} $installMode == "AllUsers"
        ${NSD_Check} $scopeAll
    ${Else}
        ${NSD_Check} $scopeCurrent
    ${EndIf}
    nsDialogs::CreateControl STATIC "${DEFAULT_STYLES}|${SS_LEFT}" 0 0 30% 100% 10u "$(NOTES_TITLE)"
    Pop $0
    SetCtlColors $0 20252E FFFFFF
    nsDialogs::CreateControl EDIT "${DEFAULT_STYLES}|${WS_TABSTOP}|${WS_VSCROLL}|${ES_MULTILINE}|${ES_AUTOVSCROLL}" "${WS_EX_CLIENTEDGE}" 0 40% 100% 60% ""
    Pop $notesText
    SendMessage $notesText ${EM_SETLIMITTEXT} 1048576 0
    FileOpen $1 "$PLUGINSDIR\changelog.txt" r
    IfErrors notes_ready
    FileSeek $1 2 SET ; Skip the UTF-16LE BOM.
notes_read:
    ClearErrors
    FileReadUTF16LE $1 $0
    IfErrors notes_close
    SendMessage $notesText ${EM_SETSEL} -1 -1
    SendMessage $notesText ${EM_REPLACESEL} 0 "STR:$0"
    Goto notes_read
notes_close:
    FileClose $1
notes_ready:
    SendMessage $notesText ${EM_SETREADONLY} 1 0
    SendMessage $notesText ${EM_SETSEL} 0 0
    SendMessage $notesText ${EM_SCROLLCARET} 0 0
    Call SetPagePalette
    nsDialogs::Show
FunctionEnd

Function ScopePageLeave
    ${NSD_GetState} $scopeAll $0
    ${If} $0 == ${BST_CHECKED}
        StrCpy $installMode "AllUsers"
    ${Else}
        StrCpy $installMode "CurrentUser"
    ${EndIf}
    Call SetInstallScope
FunctionEnd

Function .onInit
    ${IfNot} ${RunningX64}
    ${OrIfNot} ${AtLeastWin10}
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_OS) /SD IDOK
        SetErrorLevel 2
        Quit
    ${EndIf}
    SetRegView 64
    Call InitializeInstall
    InitPluginsDir
    File /oname=$PLUGINSDIR\changelog.txt "..\build\installer-assets\changelog.txt"
    ; The NSIS host is x86; this worker lets LockedList inspect x64 processes.
    File /oname=$PLUGINSDIR\LockedList64.dll "plugins\LockedList64.dll"
FunctionEnd

Function un.onInit
    SetRegView 64
    ReadINIStr $installMode "$INSTDIR\install-info.ini" "Installation" "Mode"
    ReadINIStr $1 "$INSTDIR\install-info.ini" "Installation" "Language"
    ${If} $1 == ${LANG_ENGLISH}
    ${OrIf} $1 == ${LANG_FRENCH}
        StrCpy $LANGUAGE $1
    ${EndIf}
    ${If} $installMode == "AllUsers"
        UserInfo::GetAccountType
        Pop $0
        ${If} $0 != "Admin"
            ${un.GetParameters} $parameters
            ClearErrors
            ${un.GetOptionsS} $parameters "/MachineWorker" $0
            ${IfNot} ${Errors}
                SetErrorLevel 2 ; Never loop if an elevated worker lacks its token.
                Quit
            ${EndIf}
            StrCpy $elevatedParameters "/MachineWorker"
            ${If} ${Silent}
                StrCpy $elevatedParameters "$elevatedParameters /S"
            ${EndIf}
            ; This process is already NSIS's temporary uninstaller copy.
            StrCpy $elevatedParameters "$elevatedParameters _?=$INSTDIR"
            Call un.RunElevated
            ${If} $elevatedResult == 1223
                StrCpy $elevatedResult 1
            ${EndIf}
            SetErrorLevel $elevatedResult
            Quit
        ${EndIf}
        SetShellVarContext all
    ${ElseIf} $installMode == "CurrentUser"
        SetShellVarContext current
    ${Else}
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_UNINSTALL) /SD IDOK
        SetErrorLevel 2
        Quit
    ${EndIf}
    InitPluginsDir
    File /oname=$PLUGINSDIR\LockedList64.dll "plugins\LockedList64.dll"
FunctionEnd

!macro LOCKED_PAGE PREFIX
Function ${PREFIX}LockedListShow
    !insertmacro MUI_HEADER_TEXT $(CLOSE_TITLE) $(CLOSE_TEXT)
    StrCpy $processList 0
    Call ${PREFIX}SearchModules
    ${If} $locked == 0
    ${AndIf} $R0 == "done"
        Abort ; Skip this page when there is nothing to close.
    ${EndIf}
    nsDialogs::Create 1018
    Pop $0
    ${NSD_CreateLabel} 0 4u 100% 28u "$(CLOSE_TEXT)"
    Pop $0
    ${NSD_CreateListBox} 0 40u 100% 88u ""
    Pop $processList
    Call ${PREFIX}SearchModules
    Call ${PREFIX}SetPagePalette
    nsDialogs::Show
    StrCpy $processList 0
FunctionEnd
Function ${PREFIX}LockedListLeave
    SendMessage $processList ${LB_RESETCONTENT} 0 0
    Call ${PREFIX}SearchModules
    ${If} $locked != 0
    ${OrIf} $R0 != "done"
        MessageBox MB_OK|MB_ICONSTOP $(CLOSE_TEXT) /SD IDOK
        Abort
    ${EndIf}
FunctionEnd
Function ${PREFIX}SearchCallback
    Pop $R0 ; process ID
    Pop $R1 ; path
    Pop $R2 ; description
    StrCpy $locked 1
    ${If} $processList != 0
        ${If} $R2 == ""
            StrCpy $R2 "FileSpacer"
        ${EndIf}
        SendMessage $processList ${LB_ADDSTRING} 0 "STR:$R2 ($R0)"
        Push true
    ${Else}
        Push false ; The final check only needs one match. Never terminate a process.
    ${EndIf}
FunctionEnd
Function ${PREFIX}SearchModules
    StrCpy $locked 0
    LockedList::AddModule "$INSTDIR\FileSpacer.exe"
    LockedList::AddModule "$INSTDIR\FileSpacerFolder.dll"
    GetFunctionAddress $R0 ${PREFIX}SearchCallback
    LockedList::SilentSearch $R0
    Pop $R0
FunctionEnd
Function ${PREFIX}CheckClosed
    StrCpy $processList 0
    Call ${PREFIX}SearchModules
    ${If} $locked != 0
    ${OrIf} $R0 != "done"
        !insertmacro WORKER_LOG "FileSpacer.exe or FileSpacerFolder.dll is still loaded (exit code ${EXIT_CLOSED})."
        DetailPrint $(CLOSE_TEXT)
        MessageBox MB_OK|MB_ICONSTOP $(CLOSE_TEXT) /SD IDOK
        !insertmacro WORKER_EXIT ${EXIT_CLOSED}
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
    DeleteRegValue ${ROOT} "${PREFIX}${TYPE}\shell\filespacer" "ProgrammaticAccessOnly"
    DeleteRegKey /ifempty ${ROOT} "${PREFIX}${TYPE}\shell\filespacer"
    DeleteRegKey ${ROOT} "${PREFIX}${TYPE}\shell\filespacer.explorer"
!macroend
!macro CLEAR_DEFAULT ROOT PREFIX TYPE
    !insertmacro CLEAR_CLASSES_DEFAULT ${ROOT} "${PREFIX}Software\Classes\" "${TYPE}"
!macroend

!macro REMOVE_VERBS
    DeleteRegKey SHCTX "Software\Classes\Directory\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\Directory\Background\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\CompressedFolder\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\Drive\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\NetServer\shell\filespacer"
    DeleteRegKey SHCTX "Software\Classes\NetShare\shell\filespacer"
!macroend

!macro CLEANUP_FUNCTIONS PREFIX
Function ${PREFIX}CleanupCurrentUser
    !insertmacro CLEAR_DEFAULT HKCU "" "Directory"
    !insertmacro CLEAR_DEFAULT HKCU "" "CompressedFolder"
    !insertmacro CLEAR_DEFAULT HKCU "" "Drive"
    !insertmacro CLEAR_DEFAULT HKCU "" "NetServer"
    !insertmacro CLEAR_DEFAULT HKCU "" "NetShare"
    !insertmacro CLEAR_DEFAULT HKCU "" "Directory\Background"
FunctionEnd
Function ${PREFIX}CleanupUser
    Pop $0
    StrCpy $1 $0 8 -8
    ${If} $1 == "_Classes"
        ; UsrClass.dat contains the Classes subtree itself, not Software\Classes.
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "Directory"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "CompressedFolder"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "Drive"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "NetServer"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "NetShare"
        !insertmacro CLEAR_CLASSES_DEFAULT HKU "$0\" "Directory\Background"
    ${Else}
        !insertmacro CLEAR_DEFAULT HKU "$0\" "Directory"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "CompressedFolder"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "Drive"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "NetServer"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "NetShare"
        !insertmacro CLEAR_DEFAULT HKU "$0\" "Directory\Background"
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
    !insertmacro CLEAR_DEFAULT SHCTX "" "NetServer"
    !insertmacro CLEAR_DEFAULT SHCTX "" "NetShare"
    !insertmacro CLEAR_DEFAULT SHCTX "" "Directory\Background"
    ${If} $installMode == "CurrentUser"
        Call ${PREFIX}CleanupCurrentUser
    ${Else}
        !insertmacro EnumUsersReg "${PREFIX}" ${PREFIX}CleanupUser filespacer.temp
    ${EndIf}
!macroend

Section "FileSpacer" SecBase
    SectionIn RO
    ; Prevent parallel installations from masking each other's COM registration.
    ; The parent checks the initiating user's HKCU, not an alternate admin's.
    ${If} $machineWorker != 1
        ${If} $installMode == "AllUsers"
            ReadRegStr $0 HKCU "Software\FileSpacer" "Install_Dir"
        ${Else}
            ReadRegStr $0 HKLM "Software\FileSpacer" "Install_Dir"
        ${EndIf}
        ${If} $0 != ""
            MessageBox MB_OK|MB_ICONSTOP $(ERROR_SCOPE) /SD IDOK
            !insertmacro WORKER_EXIT ${EXIT_SCOPE}
            Abort
        ${EndIf}
    ${EndIf}
    ReadRegStr $1 SHCTX "Software\FileSpacer" "Install_Dir"
    ${If} $1 != ""
    ${AndIf} $1 != $INSTDIR
        !insertmacro WORKER_LOG "An installation already exists at $1, not at $INSTDIR."
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_SCOPE) /SD IDOK
        !insertmacro WORKER_EXIT ${EXIT_SCOPE}
        Abort
    ${EndIf}
    Call CheckClosed ; Also covers silent setup and launches after the custom page.
    ${If} $installMode == "AllUsers"
    ${AndIf} $machineWorker != 1
        StrCpy $delegated 1
        Call InstallForAllUsers
        ${If} $elevatedResult != 0
            ${If} $elevatedResult == 1223
                StrCpy $elevatedResult 1 ; Match NSIS and the updater's cancellation code.
            ${Else}
                Call ExplainWorkerFailure
            ${EndIf}
            SetErrorLevel $elevatedResult
            Abort
        ${EndIf}
        Goto install_done
    ${EndIf}
    !insertmacro WORKER_LOG "Copying the program files to $INSTDIR (mode $installMode)."
    ClearErrors
    SetOutPath "$INSTDIR"
    IfErrors files_failed
    SetOverwrite on
    !insertmacro CleanInstalledDocumentation "Upgrade" files_failed
    File "..\build\release\FileSpacer.exe"
    File "..\build\release\FileSpacerFolder.dll"
    File /oname=LICENSE.txt "..\LICENSE"
    File /oname=LockedList-LICENSE.txt "plugins\LockedList.txt"
    File /oname=Lucide-LICENSE.txt "..\licenses\Lucide-LICENSE.txt"
    File /oname=SQLite-NOTICE.txt "..\third_party\sqlite\README.txt"
    File /oname=THIRD_PARTY_NOTICES.md "..\THIRD_PARTY_NOTICES.md"
    File /oname=privacy-policy.md "..\docs\privacy-policy.md"
    !insertmacro InstallDocumentation
    SetOutPath "$INSTDIR"
    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Mode" $installMode
    WriteINIStr "$INSTDIR\install-info.ini" "Installation" "Language" $LANGUAGE
    IfErrors files_failed
    !insertmacro WORKER_LOG "Files installed; writing the registration for all users."

    WriteRegStr SHCTX "Software\FileSpacer" "InstallMode" $installMode
    WriteRegStr SHCTX "Software\FileSpacer" "Install_Dir" "$INSTDIR"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayName" "FileSpacer"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "DisplayIcon" '"$INSTDIR\FileSpacer.exe",0'
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "VersionMajor" ${PRODUCT_VERSION_1}
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "VersionMinor" ${PRODUCT_VERSION_2}
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe" /$installMode'
    WriteRegStr SHCTX "${REG_UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /$installMode /S'
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "NoRepair" 1
    SectionGetSize ${SecBase} $0
    WriteRegDWORD SHCTX "${REG_UNINST_KEY}" "EstimatedSize" $0
    WriteRegStr SHCTX "${REG_APPPATH_KEY}" "" "$INSTDIR\FileSpacer.exe"
    WriteRegStr SHCTX "${REG_APPPATH_KEY}" "DropTarget" "${EXECUTE_GUID}"
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}" "" "FileSpacer"
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}\LocalServer32" "" '"$INSTDIR\FileSpacer.exe"'
    WriteRegStr SHCTX "Software\Classes\CLSID\${EXECUTE_GUID}\LocalServer32" "ServerExecutable" "$INSTDIR\FileSpacer.exe"
    IfErrors registry_failed
    !insertmacro WORKER_LOG "Registration written."
    Goto install_done
files_failed:
    !insertmacro WORKER_LOG "Copying the program files to $INSTDIR failed (exit code ${EXIT_FILES})."
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
    !insertmacro WORKER_EXIT ${EXIT_FILES}
    Abort
registry_failed:
    !insertmacro WORKER_LOG "Writing the registration failed (exit code ${EXIT_REGISTRY})."
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
    !insertmacro WORKER_EXIT ${EXIT_REGISTRY}
    Abort
install_done:
SectionEnd

Section "$(SEC_START)" SecStart
    ${If} $delegated == 1
        Goto shortcut_done
    ${EndIf}
    !insertmacro WORKER_LOG "Creating the Start Menu shortcut."
    ClearErrors
    CreateShortcut /NoWorkingDir "$SMPROGRAMS\FileSpacer.lnk" "$INSTDIR\FileSpacer.exe"
    IfErrors shortcut_failed
    Call SetShortcutAppID
    Pop $0
    ${If} $0 != 0
shortcut_failed:
        !insertmacro WORKER_LOG "Creating the Start Menu shortcut failed (exit code ${EXIT_SHORTCUT})."
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
        !insertmacro WORKER_EXIT ${EXIT_SHORTCUT}
        Abort
    ${EndIf}
shortcut_done:
SectionEnd

Section "$(SEC_CONTEXT)" SecContext
    ${If} $delegated == 1
        Goto context_done
    ${EndIf}
    !insertmacro WORKER_LOG "Registering the folder context menu."
    ClearErrors
    !insertmacro INSTALL_VERB "Directory" "%1"
    !insertmacro INSTALL_VERB "Directory\Background" "%V"
    !insertmacro INSTALL_VERB "CompressedFolder" "%1"
    !insertmacro INSTALL_VERB "Drive" "%1"
    !insertmacro INSTALL_VERB "NetServer" "%1"
    !insertmacro INSTALL_VERB "NetShare" "%1"
    Goto context_done
context_failed:
    !insertmacro WORKER_LOG "Registering the folder context menu failed (exit code ${EXIT_CONTEXT})."
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
    !insertmacro WORKER_EXIT ${EXIT_CONTEXT}
    Abort
context_done:
SectionEnd

Section /o "$(SEC_DEFAULT)" SecDefault
    ; Match settings::setDefaultBrowser: this choice belongs to the current user.
    ; Leaving this option unchecked preserves an existing default association.
    ; The unelevated parent applies HKCU choices for the original user.
    ${If} $machineWorker == 1
        Goto default_done
    ${EndIf}
    ; An unattended upgrade must not reclaim a default changed since installation.
    ${If} ${Silent}
    ${AndIf} $upgrading == 1
        Goto default_done
    ${EndIf}
    ClearErrors
    WriteRegStr HKCU "Software\Classes\Directory\shell" "" "filespacer"
    WriteRegStr HKCU "Software\Classes\CompressedFolder\shell" "" "filespacer"
    WriteRegStr HKCU "Software\Classes\Drive\shell" "" "filespacer"
    WriteRegStr HKCU "Software\Classes\NetServer\shell" "" "filespacer"
    WriteRegStr HKCU "Software\Classes\NetShare\shell" "" "filespacer"
    IfErrors 0 default_done
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
    MessageBox MB_OK|MB_ICONSTOP $(ERROR_DEFAULT) /SD IDOK
    SetErrorLevel 2
    Abort
default_done:
SectionEnd

Function .onSelChange
    ${If} ${SectionIsSelected} ${SecDefault}
        !insertmacro SelectSection ${SecContext}
    ${EndIf}
FunctionEnd

!macro RESTORE_COMPONENT SECTION KEY
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Components" "${KEY}"
    ${If} $0 == "1"
        !insertmacro SelectSection ${SECTION}
    ${ElseIf} $0 == "0"
        !insertmacro UnselectSection ${SECTION}
    ${EndIf}
!macroend

Function RestoreInstallChoices
    ; Called after selecting the scope and restoring its installation directory.
    ; Do not discard edits when revisiting the same scope.
    ${If} $choicesScope == $installMode
        Return
    ${EndIf}
    Push $0
    StrCpy $choicesScope $installMode
    StrCpy $upgrading 0
    !insertmacro SelectSection ${SecStart}
    !insertmacro SelectSection ${SecContext}
    !insertmacro UnselectSection ${SecDefault}
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Installation" "Mode"
    ${If} $0 == $installMode
        StrCpy $upgrading 1
        ; Migrate older installers without saved checkboxes from installed state.
        ${IfNot} ${FileExists} "$SMPROGRAMS\FileSpacer.lnk"
            !insertmacro UnselectSection ${SecStart}
        ${EndIf}
        ReadRegStr $0 SHCTX "Software\Classes\Directory\shell\filespacer\command" ""
        ${If} $0 == ""
            !insertmacro UnselectSection ${SecContext}
        ${EndIf}
        ReadRegStr $0 HKCR "Directory\shell" ""
        ${If} $0 == "filespacer"
            !insertmacro SelectSection ${SecDefault}
        ${EndIf}
        !insertmacro RESTORE_COMPONENT ${SecStart} "StartMenu"
        !insertmacro RESTORE_COMPONENT ${SecContext} "ContextMenu"
        !insertmacro RESTORE_COMPONENT ${SecDefault} "DefaultFileManager"
    ${EndIf}
    Call .onSelChange
    ClearErrors
    Pop $0
FunctionEnd

Function RestoreInstallLanguage
    ; LANGUAGE is changed only during .onInit, never while revisiting a page.
    ReadINIStr $0 "$INSTDIR\install-info.ini" "Installation" "Language"
    ${If} $0 == ${LANG_ENGLISH}
    ${OrIf} $0 == ${LANG_FRENCH}
        StrCpy $LANGUAGE $0
    ${EndIf}
    ClearErrors
FunctionEnd

Function SetInstallScope
    ${If} $choicesScope == $installMode
        Return ; Also preserve an edited installation directory on Back/Next.
    ${EndIf}
    ${If} $installMode == "AllUsers"
        SetShellVarContext all
        StrCpy $INSTDIR "$PROGRAMFILES64\FileSpacer"
    ${Else}
        SetShellVarContext current
        GetKnownFolderPath $INSTDIR {5CD7AEE2-2219-4A67-B85D-6C9CE15660CB}
        ${If} $INSTDIR == ""
            StrCpy $INSTDIR "$LOCALAPPDATA\Programs"
        ${EndIf}
        StrCpy $INSTDIR "$INSTDIR\FileSpacer"
    ${EndIf}
    ReadRegStr $0 SHCTX "Software\FileSpacer" "Install_Dir"
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
    Call RestoreInstallChoices
FunctionEnd

!macro WORKER_COMPONENT SECTION KEY
    ClearErrors
    ${GetOptionsS} $parameters "/${KEY}=" $0
    IfErrors worker_invalid
    ${If} $0 == "1"
        !insertmacro SelectSection ${SECTION}
    ${ElseIf} $0 == "0"
        !insertmacro UnselectSection ${SECTION}
    ${Else}
        Goto worker_invalid
    ${EndIf}
!macroend

Function InitializeInstall
    StrCpy $requestedDirectory $INSTDIR ; NSIS's optional /D=, kept last on the command line.
    ${GetParameters} $parameters
    ClearErrors
    ${GetOptionsS} $parameters "/MachineWorker" $0
    ${IfNot} ${Errors}
        StrCpy $machineWorker 1
        SetSilent silent
        ClearErrors
        ${GetOptionsS} $parameters "/Log=" $logPath
        ${If} ${Errors}
            StrCpy $logPath ""
        ${EndIf}
        ClearErrors
        !insertmacro WORKER_LOG "Elevated worker started with: $parameters"
        UserInfo::GetAccountType
        Pop $0
        ${If} $0 != "Admin"
        ${OrIf} $INSTDIR == ""
            Goto worker_invalid
        ${EndIf}
        StrCpy $installMode "AllUsers"
        SetShellVarContext all
        ClearErrors
        ${GetOptionsS} $parameters "/AllUsers" $0
        IfErrors worker_invalid
        !insertmacro WORKER_COMPONENT ${SecStart} "StartMenu"
        !insertmacro WORKER_COMPONENT ${SecContext} "ContextMenu"
        !insertmacro WORKER_COMPONENT ${SecDefault} "DefaultFileManager"
        ClearErrors
        ${GetOptionsS} $parameters "/Language=" $0
        IfErrors worker_invalid
        ${If} $0 != ${LANG_ENGLISH}
        ${AndIf} $0 != ${LANG_FRENCH}
            Goto worker_invalid
        ${EndIf}
        StrCpy $LANGUAGE $0
        Call .onSelChange
        Return
    ${EndIf}
    StrCpy $installMode "CurrentUser"
    ReadRegStr $0 HKCU "Software\FileSpacer" "Install_Dir"
    ${If} $0 == ""
        ReadRegStr $0 HKLM "Software\FileSpacer" "Install_Dir"
        ${If} $0 != ""
            StrCpy $installMode "AllUsers"
        ${EndIf}
    ${EndIf}
    ClearErrors
    ${GetOptionsS} $parameters "/CurrentUser" $0
    ${IfNot} ${Errors}
        StrCpy $installMode "CurrentUser"
    ${EndIf}
    ClearErrors
    ${GetOptionsS} $parameters "/AllUsers" $0
    ${IfNot} ${Errors}
        StrCpy $installMode "AllUsers"
    ${EndIf}
    Call SetInstallScope
    ${If} $requestedDirectory != ""
        StrCpy $INSTDIR $requestedDirectory
        StrCpy $choicesScope ""
        Call RestoreInstallChoices
    ${EndIf}
    Call RestoreInstallLanguage
    Return
worker_invalid:
    SetErrorLevel 2
    Quit
FunctionEnd

Function InstallForAllUsers
    SectionGetFlags ${SecStart} $0
    IntOp $0 $0 & ${SF_SELECTED}
    SectionGetFlags ${SecContext} $1
    IntOp $1 $1 & ${SF_SELECTED}
    SectionGetFlags ${SecDefault} $2
    IntOp $2 $2 & ${SF_SELECTED}
    ; The worker is silent: it logs into a file the parent replays afterwards.
    StrCpy $logPath "$TEMP\FileSpacer-install-machine.log"
    ClearErrors
    FileOpen $R9 "$logPath" w
    ${IfNot} ${Errors}
        FileWriteUTF16LE /BOM $R9 ""
        FileClose $R9
    ${EndIf}
    ClearErrors
    DetailPrint $(MACHINE_WAIT)
    FindWindow $R8 "#32770" "" $HWNDPARENT
    GetDlgItem $R8 $R8 1006
    SendMessage $R8 ${WM_SETTEXT} 0 "STR:$(MACHINE_WAIT)"
    StrCpy $elevatedParameters "/S /AllUsers /MachineWorker /StartMenu=$0 /ContextMenu=$1 /DefaultFileManager=$2 /Language=$LANGUAGE /Log=$\"$logPath$\" /D=$INSTDIR"
    Call RunElevated
    Call ReplayWorkerLog
FunctionEnd

Function ReplayWorkerLog
    ${If} $logPath == ""
        Return
    ${EndIf}
    ClearErrors
    FileOpen $R9 "$logPath" r
    ${If} ${Errors}
        ClearErrors
        Return
    ${EndIf}
    ${Do}
        FileReadUTF16LE $R9 $R8
        ${If} ${Errors}
            ${ExitDo}
        ${EndIf}
        StrCpy $R7 $R8 2 -2
        ${If} $R7 == "$\r$\n"
            StrCpy $R8 $R8 -2
        ${EndIf}
        ${If} $R8 != ""
            DetailPrint $R8
        ${EndIf}
    ${Loop}
    FileClose $R9
    ClearErrors
FunctionEnd

; Names the cause reported by the worker's exit code, then offers its log.
Function ExplainWorkerFailure
    ${Switch} $elevatedResult
        ${Case} ${EXIT_CLOSED}
            StrCpy $R8 $(ERROR_MACHINE_CLOSED)
            ${Break}
        ${Case} ${EXIT_SCOPE}
            StrCpy $R8 $(ERROR_SCOPE)
            ${Break}
        ${Case} ${EXIT_FILES}
            StrCpy $R8 $(ERROR_MACHINE_FILES)
            ${Break}
        ${Case} ${EXIT_REGISTRY}
            StrCpy $R8 $(ERROR_MACHINE_REGISTRY)
            ${Break}
        ${Case} ${EXIT_SHORTCUT}
            StrCpy $R8 $(ERROR_MACHINE_SHORTCUT)
            ${Break}
        ${Case} ${EXIT_CONTEXT}
            StrCpy $R8 $(ERROR_MACHINE_CONTEXT)
            ${Break}
        ${Case} ${EXIT_OPTIONS}
            StrCpy $R8 $(ERROR_MACHINE_OPTIONS)
            ${Break}
        ${Default}
            StrCpy $R8 "$(ERROR_MACHINE) ($elevatedResult)"
    ${EndSwitch}
    MessageBox MB_YESNO|MB_ICONSTOP "$R8$\r$\n$\r$\n$(OPEN_LOG)" /SD IDNO IDNO explain_done
    ExecShell "open" "$logPath"
explain_done:
FunctionEnd

!macro SAVE_COMPONENT SECTION KEY
    SectionGetFlags ${SECTION} $0
    IntOp $0 $0 & ${SF_SELECTED}
    WriteINIStr "$INSTDIR\install-info.ini" "Components" "${KEY}" $0
!macroend

Section -ApplyOptions
    ; An unchecked component also removes a component installed by an earlier version.
    ${If} $delegated != 1
        ${IfNot} ${SectionIsSelected} ${SecStart}
            Delete "$SMPROGRAMS\FileSpacer.lnk"
        ${EndIf}
        ${IfNot} ${SectionIsSelected} ${SecContext}
            !insertmacro REMOVE_VERBS
            !insertmacro CLEANUP_DEFAULTS ""
        ${EndIf}
    ${EndIf}
    ${If} $machineWorker != 1
    ${AndIf} ${SectionIsSelected} ${SecContext}
        ; Use the same per-user synchronization as Settings and normal startup.
        ; This also preserves and reflects a default selected before this upgrade.
        ClearErrors
        ExecWait '"$INSTDIR\FileSpacer.exe" /sync-context-menus' $0
        ${If} ${Errors}
        ${OrIf} $0 != 0
            MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
            SetErrorLevel 2
            Abort
        ${EndIf}
    ${EndIf}
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
    ${If} $delegated == 1
        Goto options_done ; The elevated worker already persisted all checkboxes.
    ${EndIf}
    ClearErrors
    !insertmacro SAVE_COMPONENT ${SecStart} "StartMenu"
    !insertmacro SAVE_COMPONENT ${SecContext} "ContextMenu"
    !insertmacro SAVE_COMPONENT ${SecDefault} "DefaultFileManager"
    ${If} ${Errors}
        !insertmacro WORKER_LOG "Saving the installation options failed (exit code ${EXIT_OPTIONS})."
        MessageBox MB_OK|MB_ICONSTOP $(ERROR_INSTALL) /SD IDOK
        !insertmacro WORKER_EXIT ${EXIT_OPTIONS}
        Abort
    ${EndIf}
    !insertmacro WORKER_LOG "Elevated worker finished successfully."
options_done:
SectionEnd

Section "Uninstall"
    Call un.CheckClosed
    ; Delete owned files only. No wildcards and no recursive installation-directory deletion.
    ClearErrors
    !insertmacro RemoveDocumentation
    IfFileExists "$INSTDIR\FileSpacer.exe" 0 exe_removed
    Delete "$INSTDIR\FileSpacer.exe"
    IfErrors uninstall_failed ; Keep the registration when the executable cannot be removed.
exe_removed:
    IfFileExists "$INSTDIR\FileSpacerFolder.dll" 0 provider_removed
    Delete "$INSTDIR\FileSpacerFolder.dll"
    IfErrors uninstall_failed
provider_removed:
    ReadRegStr $0 HKCU "Software\Classes\CLSID\{4FDD0CE1-6FD0-441C-AD2D-6DD056610DE3}\InprocServer32" ""
    ${If} $0 == "$INSTDIR\FileSpacerFolder.dll"
        DeleteRegKey HKCU "Software\Classes\CLSID\{4FDD0CE1-6FD0-441C-AD2D-6DD056610DE3}"
        RMDir "$LOCALAPPDATA\FileSpacer\ShellView.{4FDD0CE1-6FD0-441C-AD2D-6DD056610DE3}"
    ${EndIf}
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
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDefault} $(DESC_DEFAULT)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
