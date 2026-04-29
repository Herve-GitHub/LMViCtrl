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
    canvasview.cpp \
    projectpropertiesdialog.cpp \
    screentab.cpp \
    screenmanagerdock.cpp \
    welcomewidget.cpp \
    newprojectdialog.cpp

HEADERS += \
    mainwindow.h \
    WidgetMeta.h \
    luawidgetparser.h \
    projectmanager.h \
    widgettoolbox.h \
    canvasitem.h \
    canvasscene.h \
    canvasview.h \
    projectpropertiesdialog.h \
    screentab.h \
    screenmanagerdock.h \
    welcomewidget.h \
    newprojectdialog.h

FORMS += \
    mainwindow.ui

# Automatically compile .ts -> .qm and embed into the binary at :/i18n/
# Note: CONFIG += lrelease must come BEFORE TRANSLATIONS, otherwise qmake
# treats .ts files as compile sources and emits a bogus .obj target.
CONFIG += lrelease embed_translations

TRANSLATIONS += \
    designer_zh_CN.ts \
    designer_en.ts