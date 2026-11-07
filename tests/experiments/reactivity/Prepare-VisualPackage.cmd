@echo off
setlocal
pushd "%~dp0..\..\.."
if errorlevel 1 exit /b 1
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    popd
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\experiments\reactivity\Prepare-VisualPackage.ps1 %*
set "VISUAL_PACKAGE_EXIT_CODE=%errorlevel%"
popd
exit /b %VISUAL_PACKAGE_EXIT_CODE%
