# Included at the end of the main Makefile. NSIS is required only for installer.
!IFNDEF MAKENSIS
MAKENSIS = makensis.exe
!ENDIF

!IF "$(CFG)" == "Release"
installer: all installer-assets FORCE_INSTALLER
	"$(MAKENSIS)" /NOCONFIG /WX /V3 installer\install.nsi
!ELSE
installer: FORCE_INSTALLER
	@echo The installer requires CFG=Release.
	@exit /b 2
!ENDIF

# Force packaging despite the existing installer directory; no file is created.
FORCE_INSTALLER:

installer-assets: FORCE_INSTALLER
	"$(ICON_PYTHON)" generate-installer-assets.py --source "$(ICON_SVG)" --output build\installer-assets --inkscape "$(INKSCAPE)"

clean-installer:
	@if exist "build\FileSpacer-install.exe" del /q "build\FileSpacer-install.exe"
	@if exist "build\installer-assets\*.bmp" del /q "build\installer-assets\*.bmp"
	@if exist "build\installer-assets\*.ico" del /q "build\installer-assets\*.ico"
	@if exist "build\installer-assets\*.png" del /q "build\installer-assets\*.png"
	@if exist "build\installer-assets\*.svg" del /q "build\installer-assets\*.svg"
