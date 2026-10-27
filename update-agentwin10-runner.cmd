@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\client\Install-HyperVRunner.ps1" -UpdateController
set "UPDATE_RESULT=%errorlevel%"
if not "%UPDATE_RESULT%"=="0" exit /b %UPDATE_RESULT%
call "%~dp0test-agentwin10.cmd"
exit /b %errorlevel%
