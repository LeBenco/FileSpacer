@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tests\client\Invoke-HyperVRunner.ps1" -ProjectRoot "%~dp0."
set "SMOKE_RESULT=%errorlevel%"
if not "%SMOKE_RESULT%"=="0" echo Test did not pass. See the diagnostic and saved reports.
exit /b %SMOKE_RESULT%
