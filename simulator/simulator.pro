QT = core

CONFIG += c++17 cmdline

TARGET = simulator

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
        main.cpp

# -------------------------------------------------------
# 仿真器依赖 LvglLuaBinding 动态库（其中已包含 LVGL + Lua + SDL2 集成）
# -------------------------------------------------------
INCLUDEPATH += $$PWD/../LvglLuaBinding
DEPENDPATH  += $$PWD/../LvglLuaBinding

LIBS += -L$$DESTDIR -lLvglLuaBinding
