TEMPLATE = subdirs
SUBDIRS += \
    sdl2 \
    LvglLuaBinding \
    LMViCtrlSimulator\
    LMViCtrl
	
CONFIG += ordered \
    qt
	
DISTFILES += build_dirs.pri
QT += widgets