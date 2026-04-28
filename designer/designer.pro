QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
# 引入目录配置
include($$PWD/../build_dirs.pri)

DESTDIR = $$TARDIR
DEPENDPATH += $$TARDIR
OBJECTS_DIR = $$BUILD_PATH/$$TARGET/obj
MOC_DIR = $$BUILD_PATH/$$TARGET/moc
RCC_DIR = $$BUILD_PATH/$$TARGET/rcc
UI_DIR =  $$BUILD_PATH/$$TARGET/ui

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    mainwindow_actions.cpp \
    luawidgetparser.cpp \
    projectmanager.cpp \
    widgettoolbox.cpp \
    canvasitem.cpp \
    canvasscene.cpp \
    canvasview.cpp

HEADERS += \
    mainwindow.h \
    WidgetMeta.h \
    luawidgetparser.h \
    projectmanager.h \
    widgettoolbox.h \
    canvasitem.h \
    canvasscene.h \
    canvasview.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    designer_zh_CN.ts \
    designer_en.ts

# .qm output directory
TRANSLATIONS_DIR = $$BUILD_PATH/$$TARGET/translations

# Custom lrelease build step (works with MSVC nmake / VS Qt plugin)
lrelease_step.name    = lrelease
lrelease_step.input   = TRANSLATIONS
lrelease_step.output  = $$TRANSLATIONS_DIR/${QMAKE_FILE_BASE}.qm
win32 {
    lrelease_step.commands = \
        $(CHK_DIR_EXISTS) $$shell_path($$TRANSLATIONS_DIR) $(MKDIR) $$shell_path($$TRANSLATIONS_DIR) \
        $$escape_expand(\\n\\t)$$shell_path($$[QT_INSTALL_BINS]/lrelease) ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
} else {
    lrelease_step.commands = \
        mkdir -p $$TRANSLATIONS_DIR \
        $$escape_expand(\\n\\t)$$[QT_INSTALL_BINS]/lrelease ${QMAKE_FILE_IN} -qm ${QMAKE_FILE_OUT}
}
lrelease_step.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += lrelease_step