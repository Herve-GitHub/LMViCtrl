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
    propertypaneldock.cpp \
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
    propertypaneldock.h \
    screentab.h \
    screenmanagerdock.h \
    welcomewidget.h \
    newprojectdialog.h

FORMS += \
    mainwindow.ui

# 编译完成后，将 lua 和 img 文件夹拷贝到输出目录
win32 {
    QMAKE_POST_LINK += xcopy /E /I /Y $$shell_quote($$shell_path($$PWD/lua)) $$shell_quote($$shell_path($$TARDIR/lua)) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += xcopy /E /I /Y $$shell_quote($$shell_path($$PWD/img)) $$shell_quote($$shell_path($$TARDIR/img))
}
unix {
    QMAKE_POST_LINK += cp -r $$shell_quote($$PWD/lua) $$shell_quote($$TARDIR) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += cp -r $$shell_quote($$PWD/img) $$shell_quote($$TARDIR)
}