@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\client\Connect-HyperV.ps1" -VMName AgentWin10
set "CONNECT_RESULT=%errorlevel%"
if not "%CONNECT_RESULT%"=="0" (
    echo Connection failed. Read the diagnostic above, then press a key to close.
    pause >nul
)
exit /b %CONNECT_RESULT%
