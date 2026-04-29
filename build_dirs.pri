ROOTDIR = $$PWD
VERSIONSTR = 1.0.0
BIN_DIR = $$PWD/bin
BUILD_DIR = $$PWD/build
# ============================================================
# build_dirs.pri
# 用法：在 .pro 文件中 include(build_dirs.pri)
# ============================================================

# ---------- 2. 判断平台与编译器 ----------
msvc{
    QMAKE_CFLAGS += /utf-8
    QMAKE_CXXFLAGS += /utf-8
    CONFIG(debug, debug|release) {
        TARDIR = $$BIN_DIR/msvc/$$QT_ARCH/debug
        BUILD_PATH = $$BUILD_DIR/msvc/$$QT_ARCH/debug
    }
    CONFIG(release, debug|release)  {
        TARDIR = $$BIN_DIR/msvc/$$QT_ARCH/release
        BUILD_PATH = $$BUILD_DIR/msvc/$$QT_ARCH/release
    }
}
gcc{
    linux{
        QMAKE_LFLAGS += -Wl,-rpath,\\\$$ORIGIN
        QMAKE_LFLAGS += -Wl,-rpath,\\\$$ORIGIN/basiclibrary
    }
    CONFIG(debug, debug|release) {
        TARDIR = $$BIN_DIR/gcc/$$QT_ARCH/debug
        BUILD_PATH = $$BUILD_DIR/gcc/$$QT_ARCH/debug
    }
    CONFIG(release, debug|release)  {
        TARDIR = $$BIN_DIR/gcc/$$QT_ARCH/release
        BUILD_PATH = $$BUILD_DIR/gcc/$$QT_ARCH/release
    }
}