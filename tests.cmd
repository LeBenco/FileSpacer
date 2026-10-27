@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
rem Local by default. -VM "name" dispatches the filtered suite to a configured VM.
powershell.exe -NoProfile -Command "if (Get-Process FileSpacer -ErrorAction SilentlyContinue) { Write-Error 'Close FileSpacer before building.'; exit 1 }"
if errorlevel 1 (
    popd
    exit /b 1
)
call "%~dp0build.cmd" debug
if errorlevel 1 (
    popd
    exit /b 1
)
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    popd
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\regression\Run-Regression.ps1" -ProjectRoot "%~dp0." %*
set "TEST_EXIT_CODE=%errorlevel%"
popd
exit /b %TEST_EXIT_CODE%
