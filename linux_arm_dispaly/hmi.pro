TEMPLATE = app
TARGET = hmi

QT -= core gui

CONFIG += c++17 console
CONFIG -= app_bundle

win32 {
    error("hmi is only supported on Linux/Unix targets")
}

include($$PWD/../build_dirs.pri)

DESTDIR = $$TARDIR
DEPENDPATH += $$TARDIR
OBJECTS_DIR = $$BUILD_PATH/$$TARGET/obj
MOC_DIR = $$BUILD_PATH/$$TARGET/moc
RCC_DIR = $$BUILD_PATH/$$TARGET/rcc
UI_DIR =  $$BUILD_PATH/$$TARGET/ui

INCLUDEPATH += $$PWD/../LvglLuaBinding
DEPENDPATH  += $$PWD/../LvglLuaBinding

LIBS += -L$$DESTDIR -lLvglLuaBindingHmi

SOURCES += \
    main.cpp