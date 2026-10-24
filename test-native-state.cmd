@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 3
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto failed
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x64" goto failed
where.exe /q cl.exe
if errorlevel 1 (
    echo MSVC introuvable apres vcvars64.bat : cl.exe doit etre accessible dans PATH.
    goto failed
)
if not exist "build\native-state" mkdir "build\native-state"
cl.exe /nologo /TC /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /DSQLITE_OMIT_LOAD_EXTENSION /DSQLITE_DQS=0 /c third_party\sqlite\sqlite3.c /Fo"build\native-state\sqlite3.obj"
if errorlevel 1 goto failed
echo Compilation native-state : /Z7, aucun PDB de compilation.
cl.exe /nologo /std:c++17 /EHsc /W4 /O2 /MT /utf-8 /Z7 /DFILESPACER_STORAGE_TEST /I src /I third_party\sqlite /Fo"build\native-state\\" /Fe"build\native-state\NativeShellStateTests.exe" tests\regression\cases\18-vue-shell-native\NativeShellStateTests.cpp src\ShellViewHost.cpp src\ShellViewFilter.cpp src\ShellViewSettings.cpp src\ShellViewSelection.cpp src\ShellViewIcons.cpp src\FolderStateStore.cpp src\DPI.cpp build\native-state\sqlite3.obj /link /subsystem:console /incremental:no /manifest:embed /manifestinput:tests\regression\cases\18-vue-shell-native\NativeShellStateTests.manifest User32.lib Gdi32.lib Ole32.lib OleAut32.lib Shell32.lib Shlwapi.lib Uuid.lib
if errorlevel 1 goto failed
set "NATIVE_STATE_ARGS="
if /i "%~1"=="--diagnostic" set "NATIVE_STATE_ARGS=--diagnostic"
"build\native-state\NativeShellStateTests.exe" %NATIVE_STATE_ARGS%
set "NATIVE_STATE_EXIT=%errorlevel%"
popd
exit /b %NATIVE_STATE_EXIT%
:failed
popd
exit /b 3
