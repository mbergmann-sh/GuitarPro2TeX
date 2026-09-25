# ---------------------------------------------------------------------------
#  GuitarPROtoTeX-Convert - MusicXML (Guitar Pro export) -> MusiXTeX snippets
#  Qt 6 + QScintilla, qmake
# ---------------------------------------------------------------------------

QT += core gui widgets printsupport
CONFIG += qscintilla2 c++17 release

TARGET = gPro8toTeX
TEMPLATE = app

# Single source of the version number: About dialog, Windows file info, installer
VERSION = 1.0.2
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    latexlexer.cpp \
    musicxmlconverter.cpp \
    musixtexgenerator.cpp

HEADERS += \
    mainwindow.h \
    latexlexer.h \
    musicxmlconverter.h

FORMS += \
    mainwindow.ui

RESOURCES += application.qrc

TRANSLATIONS += \
    GuitarPROtoTeX-Convert_en_150.ts \
    GuitarPROtoTeX-Convert_de.ts
CONFIG += lrelease
CONFIG += embed_translations

# ---------------------------------------------------------------------------
#  Windows: icon, file information, deployment, installer
# ---------------------------------------------------------------------------
win32 {
    RC_ICONS = images/app_icon.ico
    QMAKE_TARGET_PRODUCT     = GuitarPROtoTeX-Convert
    QMAKE_TARGET_DESCRIPTION = MusicXML to MusiXTeX converter
    QMAKE_TARGET_COPYRIGHT   = Michael Bergmann, GNU GPL v3

    INSTALLER_DIR = $$PWD/installer     # like AmigaED: installer\install_src + .iss in the project

    # QScintilla DLL: next to the Qt binaries by default; override with
    #   qmake "QSCINTILLA_BIN_DIR=C:/path/to/dir"
    isEmpty(QSCINTILLA_BIN_DIR): QSCINTILLA_BIN_DIR = $$[QT_INSTALL_BINS]
    QSCINTILLA_DLL = qscintilla2_qt6.dll
    !exists($$QSCINTILLA_BIN_DIR/$$QSCINTILLA_DLL): exists($$[QT_INSTALL_LIBS]/$$QSCINTILLA_DLL): \
        QSCINTILLA_BIN_DIR = $$[QT_INSTALL_LIBS]
    !exists($$QSCINTILLA_BIN_DIR/$$QSCINTILLA_DLL): \
        warning("$$QSCINTILLA_DLL not found in $$QSCINTILLA_BIN_DIR - set QSCINTILLA_BIN_DIR; the DLL is not copied")

    # The post-link commands run either in cmd.exe or - if an sh.exe is in the PATH
    # (e.g. from an m68k-amigaos-gcc toolchain) - in sh. qmake detects this itself
    # (QMAKE_SH) and adjusts $$shell_path / $$shell_quote / $$QMAKE_COPY and the make
    # variables $(DESTDIR) / $(DESTDIR_TARGET) (= output folder / .exe of THIS makefile,
    # release\ or debug\). The shell-specific call of the staging script is explicit.
    WINDEPLOYQT = $$shell_quote($$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe))

    # 1. QScintilla DLL next to the .exe - FIRST, so that windeployqt can analyse it
    exists($$QSCINTILLA_BIN_DIR/$$QSCINTILLA_DLL) {
        QMAKE_POST_LINK += $$QMAKE_COPY \
            $$shell_quote($$shell_path($$QSCINTILLA_BIN_DIR/$$QSCINTILLA_DLL)) $(DESTDIR) $$escape_expand(\\n\\t)
        # $(strip ...): qmake writes "DESTDIR = release/ #comment" - the blank belongs to the value
        DEPLOY_BINARIES = $(DESTDIR_TARGET) $(strip $(DESTDIR))$$QSCINTILLA_DLL
    } else {
        DEPLOY_BINARIES = $(DESTDIR_TARGET)
    }

    # 2. windeployqt: Qt DLLs, plugins and Qt translations next to the .exe.
    #    It only analyses the binaries it is given. The program itself does not import
    #    Qt6PrintSupport.dll, but QScintilla does - so the QScintilla DLL is passed too,
    #    and --printsupport makes sure the module is deployed in any case.
    #    (CONFIG above always contains "release")
    QMAKE_POST_LINK += $$WINDEPLOYQT --release --printsupport $$DEPLOY_BINARIES $$escape_expand(\\n\\t)

    # 3. stage everything for the installer into installer\install_src
    #    (install_stage.bat, uses robocopy; the folder is git-ignored)
    STAGE_ARGS = $$shell_quote($$system_path($$PWD/installer/install_stage.bat)) \
                 $(DESTDIR) \
                 $$shell_quote($$system_path($$INSTALLER_DIR)) \
                 $$shell_quote($$system_path($$PWD))
    isEmpty(QMAKE_SH) {
        QMAKE_POST_LINK += call $$STAGE_ARGS $$escape_expand(\\n\\t)
    } else {
        # sh (MSYS): "//c" becomes "/c" for cmd.exe. "call" first, because cmd /c
        # strips the outer quotes of a command line that starts with a quote.
        QMAKE_POST_LINK += cmd //c call $$STAGE_ARGS $$escape_expand(\\n\\t)
    }

    # 4. Inno Setup: open installer/GuitarPROtoTeX-Convert.iss and compile it.
    #    It reads the version from the staged .exe, so VERSION above is the only place.
}

# ---------------------------------------------------------------------------
#  Default rules for deployment (Linux etc.)
# ---------------------------------------------------------------------------
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    README.md \
    LICENSE \
    installer/GuitarPROtoTeX-Convert.iss \
    installer/install_stage.bat
