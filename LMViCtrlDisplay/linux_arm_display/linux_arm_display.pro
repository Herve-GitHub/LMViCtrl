TEMPLATE = subdirs
CONFIG += ordered

win32 {
    error("linux_arm_display is only supported on Linux/Unix targets")
}

SUBDIRS += \
    LvglLuaBindingHmi \
    hmi

LvglLuaBindingHmi.file = ../../LvglLuaBinding/LvglLuaBindingHmi.pro

hmi.file = hmi.pro
hmi.depends = LvglLuaBindingHmi