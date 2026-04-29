TEMPLATE = subdirs
SUBDIRS += \
    #sdl2 \
    #LvglLuaBinding \
    designer
	
CONFIG += ordered \
    qt
	
DISTFILES += build_dirs.pri
QT += widgets