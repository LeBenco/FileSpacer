# Windows x64 / MSVC. Run with build.cmd or from an x64 developer prompt.
!IFNDEF CFG
CFG = Release
!ENDIF

!IF "$(CFG)" == "Debug"
OUTDIR = build\debug
CXXFLAGS = /nologo /EHsc /MP /W4 /Zi /MTd /D FILESPACER_DEBUG /I src
LINKFLAGS = /subsystem:console
!ELSEIF "$(CFG)" == "Release"
OUTDIR = build\release
CXXFLAGS = /nologo /EHsc /MP /W4 /O2 /Zi /MT /I src
LINKFLAGS = /subsystem:windows /OPT:REF /OPT:ICF
!ELSE
!ERROR CFG must be Debug or Release.
!ENDIF

!IFNDEF INKSCAPE
INKSCAPE = C:\Program Files\Inkscape\bin\inkscape.com
!ENDIF

!IFNDEF ICON_PYTHON
ICON_PYTHON = $(INKSCAPE:inkscape.com=python.exe)
!ENDIF

SQLITE_DIR = third_party\sqlite
SQLITE_COMMON_FLAGS = /nologo /W3 /TC /D _CRT_SECURE_NO_WARNINGS /D SQLITE_OMIT_LOAD_EXTENSION /D SQLITE_DQS=0
!IF "$(CFG)" == "Debug"
SQLITE_FLAGS = $(SQLITE_COMMON_FLAGS) /Zi /MTd
!ELSE
SQLITE_FLAGS = $(SQLITE_COMMON_FLAGS) /O2 /Zi /MT
!ENDIF
SQLITE_OBJECT = $(OUTDIR)\sqlite3.obj

TARGET = $(OUTDIR)\FileSpacer.exe
RESOURCE = $(OUTDIR)\resource.res
ICON_SVG = src\res\FileSpacer.svg
ICON_ICO = src\res\FileSpacer.ico
LOCK_BADGE_ICO = src\res\window-state-lock.ico
ICON_GENERATOR = generate-icon.py
OBJECTS = \
    $(OUTDIR)\ClosedWindows.obj \
    $(OUTDIR)\COMUtils.obj \
    $(OUTDIR)\CreateItemWindow.obj \
    $(OUTDIR)\DPI.obj \
    $(OUTDIR)\ExecuteCommand.obj \
    $(OUTDIR)\Everything.obj \
    $(OUTDIR)\FolderWindow.obj \
    $(OUTDIR)\FolderIdentity.obj \
    $(OUTDIR)\FolderStateStore.obj \
    $(OUTDIR)\GDIUtils.obj \
    $(OUTDIR)\ItemWindow.obj \
    $(OUTDIR)\PathBar.obj \
    $(OUTDIR)\ProxyIcon.obj \
    $(OUTDIR)\ProcessLifetime.obj \
	$(OUTDIR)\SearchBox.obj \
    $(OUTDIR)\Settings.obj \
    $(OUTDIR)\SettingsDialog.obj \
    $(OUTDIR)\ShortcutSettingsPage.obj \
    $(OUTDIR)\ShellUtils.obj \
    $(OUTDIR)\MixedFolder.obj \
    $(OUTDIR)\ShellViewHost.obj \
    $(OUTDIR)\ShellViewFilter.obj \
    $(OUTDIR)\ShellViewSettings.obj \
    $(OUTDIR)\ShellViewSelection.obj \
    $(OUTDIR)\ShellViewIcons.obj \
    $(OUTDIR)\UIStrings.obj \
    $(OUTDIR)\Update.obj \
    $(OUTDIR)\UpdateManifest.obj \
    $(OUTDIR)\UpdateInstaller.obj \
    $(OUTDIR)\UpdateInstallTarget.obj \
    $(OUTDIR)\UpdateSession.obj \
    $(OUTDIR)\UpdateSessionStore.obj \
    $(OUTDIR)\WinUtils.obj \
    $(OUTDIR)\WindowResizeGesture.obj \
    $(OUTDIR)\WindowPlacement.obj \
	$(OUTDIR)\CrashRecovery.obj \
    $(OUTDIR)\main.obj

all: $(OUTDIR) $(OUTDIR)\FileSpacerFolder.dll $(TARGET)

$(OUTDIR):
	@if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

$(TARGET): $(OBJECTS) $(SQLITE_OBJECT) $(RESOURCE) $(OUTDIR)\FileSpacerFolder.dll Makefile
	cl.exe /nologo /Fe$(TARGET) $(OBJECTS) $(SQLITE_OBJECT) $(RESOURCE) /link /incremental:no /manifest:no /debug /pdb:$(OUTDIR)\FileSpacer.pdb $(LINKFLAGS) Uuid.lib

$(OUTDIR)\FileSpacerFolder.dll: src\MixedFolderProvider.cpp src\MixedFolder.h src\common.h src\MixedFolderProvider.def Makefile
	cl.exe $(CXXFLAGS) /DSTRICT_TYPED_ITEMIDS /Fd$(OUTDIR)\MixedFolder-compiler.pdb /Fo$(OUTDIR)\MixedFolderProvider.obj /LD /Fe$@ src\MixedFolderProvider.cpp /link /incremental:no /debug /pdb:$(OUTDIR)\FileSpacerFolder.pdb /def:src\MixedFolderProvider.def User32.lib Shell32.lib Shlwapi.lib Ole32.lib OleAut32.lib Propsys.lib Uuid.lib

# Header changes rebuild all C++ files; source changes rebuild only their objects.
$(OBJECTS): src\*.h $(SQLITE_DIR)\sqlite3.h Makefile

# Batch inference preserves parallel compilation through cl.exe /MP.
{src}.cpp{$(OUTDIR)}.obj::
	cl.exe $(CXXFLAGS) /I $(SQLITE_DIR) /Fd$(OUTDIR)\FileSpacer-compiler.pdb /Fo$(OUTDIR)\ /c $<

# Build the unmodified upstream C source separately from the C++ project.
$(SQLITE_OBJECT): $(SQLITE_DIR)\sqlite3.c $(SQLITE_DIR)\sqlite3.h Makefile
	cl.exe $(SQLITE_FLAGS) /Fd$(OUTDIR)\sqlite3-compiler.pdb /Fo$(SQLITE_OBJECT) /c $(SQLITE_DIR)\sqlite3.c

$(RESOURCE): src\*.rc src\resource.h src\dialog.h \
    src\res\FileSpacer.exe.manifest $(ICON_ICO) $(LOCK_BADGE_ICO) \
    Makefile
	rc.exe /D _UNICODE /D UNICODE /n /I src /fo $(RESOURCE) src\resource.rc

# Regenerate the optical small sizes and the larger vector sizes together.
$(ICON_ICO): $(ICON_SVG) $(ICON_GENERATOR) Makefile
	"$(ICON_PYTHON)" "$(ICON_GENERATOR)" --source "$(ICON_SVG)" --output "$(ICON_ICO)" --work-dir "$(OUTDIR)" --inkscape "$(INKSCAPE)"

clean:
	@if exist "build\debug" rmdir /s /q "build\debug"
	@if exist "build\release" rmdir /s /q "build\release"
	@if exist "$(ICON_ICO)" del /q "$(ICON_ICO)"

!include installer\installer.mak
