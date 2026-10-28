# Included at the end of the main Makefile. NSIS is required only for installer.
!IFNDEF MAKENSIS
MAKENSIS = makensis.exe
!ENDIF

!IFNDEF NODE
NODE = node
!ENDIF

!IF "$(CFG)" == "Release"
installer: all installer-assets installer-documentation FORCE_INSTALLER
	"$(MAKENSIS)" /NOCONFIG /WX /V3 installer\install.nsi
!ELSE
installer: FORCE_INSTALLER
	@echo The installer requires CFG=Release.
	@exit /b 2
!ENDIF

# Force packaging despite the existing installer directory; no file is created.
FORCE_INSTALLER:

installer-documentation: FORCE_INSTALLER
	"$(NODE)" .github\scripts\build-docs.mjs --target offline

installer-assets: $(ICON_ICO) FORCE_INSTALLER
	"$(ICON_PYTHON)" generate-installer-assets.py --source "$(ICON_SVG)" --icon "$(ICON_ICO)" --output build\installer-assets --inkscape "$(INKSCAPE)"
	"$(ICON_PYTHON)" -c "from pathlib import Path; lines=Path('CHANGELOG.md').read_text(encoding='utf-8-sig').splitlines(); text='\r\n'.join(line.lstrip(chr(35)+' ') if line.startswith(chr(35)) else line for line in lines if line.lstrip(chr(35)+' ') not in ('Changelog','Unreleased') and not line.startswith('[Full changes]')).strip()+'\r\n'; Path('build/installer-assets/changelog.txt').write_bytes(text.encode('utf-16'))"
	rc.exe /nologo /fo build\installer-assets\wizard-ui.res installer\wizard-ui.rc
	link.exe /nologo /dll /noentry /machine:x86 /out:build\installer-assets\wizard-ui.dll build\installer-assets\wizard-ui.res

clean-installer:
	@if exist "build\FileSpacer-install.exe" del /q "build\FileSpacer-install.exe"
	@if exist "build\installer-assets\*.bmp" del /q "build\installer-assets\*.bmp"
	@if exist "build\installer-assets\*.ico" del /q "build\installer-assets\*.ico"
	@if exist "build\installer-assets\*.png" del /q "build\installer-assets\*.png"
	@if exist "build\installer-assets\*.svg" del /q "build\installer-assets\*.svg"
	@if exist "build\installer-assets\changelog.txt" del /q "build\installer-assets\changelog.txt"
	@if exist "build\installer-assets\wizard-ui.res" del /q "build\installer-assets\wizard-ui.res"
	@if exist "build\installer-assets\wizard-ui.dll" del /q "build\installer-assets\wizard-ui.dll"
