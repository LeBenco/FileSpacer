@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1

set "BUILD_CONFIG=Release"
set "BUILD_TARGET=all"
if not "%~2"=="" goto usage
if "%~1"=="" goto build
if /i "%~1"=="release" goto build
if /i "%~1"=="debug" (
    set "BUILD_CONFIG=Debug"
    goto build
)
if /i "%~1"=="installer" (
    set "BUILD_TARGET=installer"
    goto build
)
if /i "%~1"=="clean" (
    set "BUILD_TARGET=clean clean-installer"
    goto build
)
goto usage

:build
if /i "%BUILD_TARGET%"=="installer" (
    if not defined MAKENSIS if exist "%ProgramFiles(x86)%\NSIS\makensis.exe" set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
)
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    popd
    exit /b 1
)
nmake.exe /nologo /f Makefile CFG=%BUILD_CONFIG% %BUILD_TARGET%
set "BUILD_EXIT_CODE=%errorlevel%"
popd
exit /b %BUILD_EXIT_CODE%

:usage
echo Usage: build.cmd [debug ^| release ^| installer ^| clean]
popd
exit /b 2
