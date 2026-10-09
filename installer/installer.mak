# Included at the end of the main Makefile. NSIS is required only for installer.
!IFNDEF MAKENSIS
MAKENSIS = makensis.exe
!ENDIF

!IF "$(CFG)" == "Release"
installer: all FORCE_INSTALLER
	"$(MAKENSIS)" /NOCONFIG /WX /V3 installer\install.nsi
!ELSE
installer: FORCE_INSTALLER
	@echo The installer requires CFG=Release.
	@exit /b 2
!ENDIF

# Force packaging despite the existing installer directory; no file is created.
FORCE_INSTALLER:

clean-installer:
	@if exist "build\FileSpacer-install.exe" del /q "build\FileSpacer-install.exe"
