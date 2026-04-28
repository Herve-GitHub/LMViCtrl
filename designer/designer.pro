QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
# 引入目录配置
include($$PWD/../build_dirs.pri)
SOURCES += \
    main.cpp \
    mainwindow.cpp \
    mainwindow_actions.cpp \
    luawidgetparser.cpp \
    widgettoolbox.cpp \
    canvasitem.cpp \
    canvasscene.cpp \
    canvasview.cpp \
    widgetpainter.cpp

HEADERS += \
    mainwindow.h \
    WidgetMeta.h \
    luawidgetparser.h \
    widgettoolbox.h \
    canvasitem.h \
    canvasscene.h \
    canvasview.h \
    widgetpainter.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    designer_zh_CN.ts \
    designer_en.ts
CONFIG += lrelease
CONFIG += embed_translations