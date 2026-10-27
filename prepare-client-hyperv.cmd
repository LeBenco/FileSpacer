@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
powershell.exe -NoProfile -Command "if (Get-Process FileSpacer -ErrorAction SilentlyContinue) { Write-Error 'Close FileSpacer before building.'; exit 1 }"
if errorlevel 1 goto failed
call build.cmd debug
if errorlevel 1 goto failed
if not exist build\client-smoke mkdir build\client-smoke
"%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:library /platform:x64 /reference:System.Windows.Forms.dll /reference:"%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\WPF\UIAutomationClient.dll" /reference:"%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\WPF\UIAutomationTypes.dll" /reference:"%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\WPF\WindowsBase.dll" /out:build\client-smoke\FileSpacerRegression-HyperV.dll tests\regression\common\EssentialUi.cs tests\regression\cases\15-deplacement-alt\MouseInput.cs tests\regression\cases\15-deplacement-alt\NativeTargets.cs tests\regression\common\DesktopActivity.cs
if errorlevel 1 goto failed
"%SystemRoot%\Microsoft.NET\Framework64\v4.0.30319\csc.exe" /nologo /target:winexe /platform:x64 /out:build\client-smoke\NoConsoleRunner.exe tests\client\NoConsoleRunner.cs
if errorlevel 1 goto failed
echo Host compilation complete. No VM connection or UI test was started.
popd
exit /b 0
:failed
popd
exit /b 1
