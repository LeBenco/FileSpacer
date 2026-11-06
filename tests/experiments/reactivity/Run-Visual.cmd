@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Run-VisualMeasurements.ps1" %*
exit /b %errorlevel%
