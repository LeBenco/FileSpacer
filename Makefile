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
ICON_512 = $(OUTDIR)\FileSpacer-512.png
ICON_PNGS = \
    $(OUTDIR)\FileSpacer-16.png \
    $(OUTDIR)\FileSpacer-20.png \
    $(OUTDIR)\FileSpacer-24.png \
    $(OUTDIR)\FileSpacer-32.png \
    $(OUTDIR)\FileSpacer-40.png \
    $(OUTDIR)\FileSpacer-48.png \
    $(OUTDIR)\FileSpacer-64.png \
    $(OUTDIR)\FileSpacer-96.png \
    $(OUTDIR)\FileSpacer-128.png \
    $(OUTDIR)\FileSpacer-256.png
OBJECTS = \
    $(OUTDIR)\COMUtils.obj \
    $(OUTDIR)\CreateItemWindow.obj \
    $(OUTDIR)\DPI.obj \
    $(OUTDIR)\ExecuteCommand.obj \
    $(OUTDIR)\FolderWindow.obj \
    $(OUTDIR)\FolderIdentity.obj \
    $(OUTDIR)\FolderStateStore.obj \
    $(OUTDIR)\GDIUtils.obj \
    $(OUTDIR)\ItemWindow.obj \
    $(OUTDIR)\PathBar.obj \
    $(OUTDIR)\ProxyIcon.obj \
	$(OUTDIR)\SearchBox.obj \
    $(OUTDIR)\Settings.obj \
    $(OUTDIR)\SettingsDialog.obj \
    $(OUTDIR)\ShellUtils.obj \
    $(OUTDIR)\UIStrings.obj \
    $(OUTDIR)\Update.obj \
    $(OUTDIR)\WinUtils.obj \
	$(OUTDIR)\CrashRecovery.obj \
    $(OUTDIR)\main.obj

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	@if not exist "$(OUTDIR)" mkdir "$(OUTDIR)"

$(TARGET): $(OBJECTS) $(SQLITE_OBJECT) $(RESOURCE) Makefile
	cl.exe /nologo /Fe$(TARGET) $(OBJECTS) $(SQLITE_OBJECT) $(RESOURCE) /link /incremental:no /manifest:no /debug /pdb:$(OUTDIR)\FileSpacer.pdb $(LINKFLAGS)

# Header changes rebuild all C++ files; source changes rebuild only their objects.
$(OBJECTS): src\*.h $(SQLITE_DIR)\sqlite3.h Makefile

# Batch inference preserves parallel compilation through cl.exe /MP.
{src}.cpp{$(OUTDIR)}.obj::
	cl.exe $(CXXFLAGS) /I $(SQLITE_DIR) /Fd$(OUTDIR)\FileSpacer-compiler.pdb /Fo$(OUTDIR)\ /c $<

# Build the unmodified upstream C source separately from the C++ project.
$(SQLITE_OBJECT): $(SQLITE_DIR)\sqlite3.c $(SQLITE_DIR)\sqlite3.h Makefile
	cl.exe $(SQLITE_FLAGS) /Fd$(OUTDIR)\sqlite3-compiler.pdb /Fo$(SQLITE_OBJECT) /c $(SQLITE_DIR)\sqlite3.c

$(RESOURCE): src\*.rc src\resource.h src\dialog.h \
    src\res\FileSpacer.exe.manifest $(ICON_ICO) \
    Makefile
	rc.exe /D _UNICODE /D UNICODE /n /I src /fo $(RESOURCE) src\resource.rc

$(ICON_ICO): $(ICON_SVG) $(OUTDIR)
	"$(INKSCAPE)" "$(ICON_SVG)" --export-filename="$(ICON_512)" --export-width=512 --export-height=512 --export-background-opacity=0
	magick "$(ICON_512)" -filter Cubic -resize 16x16 "$(OUTDIR)\FileSpacer-16.png"
	magick "$(ICON_512)" -filter Cubic -resize 20x20 "$(OUTDIR)\FileSpacer-20.png"
	magick "$(ICON_512)" -filter Cubic -resize 24x24 "$(OUTDIR)\FileSpacer-24.png"
	magick "$(ICON_512)" -filter Cubic -resize 32x32 "$(OUTDIR)\FileSpacer-32.png"
	magick "$(ICON_512)" -filter Cubic -resize 40x40 "$(OUTDIR)\FileSpacer-40.png"
	magick "$(ICON_512)" -filter Cubic -resize 48x48 "$(OUTDIR)\FileSpacer-48.png"
	magick "$(ICON_512)" -filter Cubic -resize 64x64 "$(OUTDIR)\FileSpacer-64.png"
	magick "$(ICON_512)" -filter Cubic -resize 96x96 "$(OUTDIR)\FileSpacer-96.png"
	magick "$(ICON_512)" -filter Cubic -resize 128x128 "$(OUTDIR)\FileSpacer-128.png"
	magick "$(ICON_512)" -filter Cubic -resize 256x256 "$(OUTDIR)\FileSpacer-256.png"
	magick $(ICON_PNGS) "$(ICON_ICO)"
	del /q "$(ICON_512)" $(ICON_PNGS)

clean:
	@if exist "build\debug" rmdir /s /q "build\debug"
	@if exist "build\release" rmdir /s /q "build\release"
	@if exist "$(ICON_ICO)" del /q "$(ICON_ICO)"

!include installer\installer.mak