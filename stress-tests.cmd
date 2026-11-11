@echo off
setlocal
call "%~dp0tests.cmd" -Suite Stress %*
exit /b %errorlevel%
