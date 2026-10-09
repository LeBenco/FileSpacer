; Adapted from nsis-shortcut-properties, revision b5ede73fc49665449731df5d0f945192329b0ef6.
; Copyright Safing ICS Technologies GmbH. Apache License 2.0, see licenses/.
; FileSpacer modification: AppUserModelID only, with cleanup on every exit.
!include Win\COM.nsh
!include Win\Propkey.nsh

Function SetShortcutAppID
    System::Store S
    StrCpy $1 0
    StrCpy $2 0
    StrCpy $3 0
    StrCpy $4 0
    StrCpy $5 0
    StrCpy $6 0
    StrCpy $8 0
    System::Call 'ole32::CoInitialize(p 0)i.r0'
    IntCmp $0 0 initialized cleanup initialized
initialized:
    StrCpy $8 1
    !insertmacro ComHlpr_CreateInProcInstance ${CLSID_ShellLink} ${IID_IShellLink} r1 ".r0"
    IntCmp $0 0 0 cleanup cleanup
    ${IUnknown::QueryInterface} $1 '("${IID_IPersistFile}",.r2)i.r0'
    IntCmp $0 0 0 cleanup cleanup
    ${IPersistFile::Load} $2 '("$SMPROGRAMS\FileSpacer.lnk",2)i.r0'
    IntCmp $0 0 0 cleanup cleanup
    ${IUnknown::QueryInterface} $1 '("${IID_IPropertyStore}",.r3)i.r0'
    IntCmp $0 0 0 cleanup cleanup

    System::Call '*${SYSSTRUCT_PROPERTYKEY}(${PKEY_AppUserModel_ID})p.r4'
    System::Call '*${SYSSTRUCT_PROPVARIANT}(${VT_LPWSTR},,p 0)p.r5'
    StrLen $7 "${APP_ID}"
    IntOp $7 $7 + 1
    IntOp $9 $7 * 2
    System::Call 'ole32::CoTaskMemAlloc(p r9)p.r6'
    ${If} $4 P= 0
    ${OrIf} $5 P= 0
    ${OrIf} $6 P= 0
        StrCpy $0 0x8007000E ; E_OUTOFMEMORY
        Goto cleanup
    ${EndIf}
    System::Call '*$6(&w$7 "${APP_ID}")'
    System::Call '*$5${SYSSTRUCT_PROPVARIANT}(${VT_LPWSTR},,p r6)'
    ${IPropertyStore::SetValue} $3 '(r4,r5)i.r0'
    IntCmp $0 0 0 cleanup cleanup
    ${IPropertyStore::Commit} $3 '()i.r0'
    IntCmp $0 0 0 cleanup cleanup
    ${IPersistFile::Save} $2 '("$SMPROGRAMS\FileSpacer.lnk",1)i.r0'

cleanup:
    ; Free only the storage we allocated; SetValue copies the property value.
    System::Call 'ole32::CoTaskMemFree(p r6)'
    System::Free $5
    System::Free $4
    !insertmacro ComHlpr_SafeRelease $3
    !insertmacro ComHlpr_SafeRelease $2
    !insertmacro ComHlpr_SafeRelease $1
    ${If} $8 == 1
        System::Call 'ole32::CoUninitialize()'
    ${EndIf}
    Push $0
    System::Store L
FunctionEnd
