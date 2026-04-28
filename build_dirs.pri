ROOTDIR = $$PWD
# ============================================================
# build_dirs.pri
# 用法：在 .pro 文件中 include(build_dirs.pri)
# ============================================================

# ---------- 1. 判断构建类型 ----------
CONFIG(debug, debug|release) {
    BUILD_TYPE = debug
} else {
    BUILD_TYPE = release
}

# ---------- 2. 判断平台与编译器 ----------
win32 {
    msvc {
        # MSVC：通过 QMAKE_TARGET.arch 区分 32/64 位
        contains(QMAKE_TARGET.arch, x86_64) {
            PLATFORM_TAG = windows/msvc/x64
        } else {
            PLATFORM_TAG = windows/msvc/x86
        }
    } else: gcc | clang {
        # MinGW：通过 QT_ARCH 区分 32/64 位
        contains(QT_ARCH, x86_64) {
            PLATFORM_TAG = windows/mingw/x64
        } else {
            PLATFORM_TAG = windows/mingw/x86
        }
    } else {
        PLATFORM_TAG = windows/unknown
    }
}

unix:!macx {
    # Linux：通过 QT_ARCH 区分 x86_64 / arm
    contains(QT_ARCH, x86_64) {
        PLATFORM_TAG = linux/x86_64
    } else: contains(QT_ARCH, arm64) | contains(QT_ARCH, aarch64) {
        PLATFORM_TAG = linux/arm64
    } else: contains(QT_ARCH, arm) {
        PLATFORM_TAG = linux/arm
    } else {
        PLATFORM_TAG = linux/$$QT_ARCH
    }
}

# ---------- 3. 拼接最终目录 ----------
BUILD_BASE   = $$PWD/output/$$PLATFORM_TAG/$$BUILD_TYPE

DESTDIR      = $$BUILD_BASE/bin
OBJECTS_DIR  = $$BUILD_BASE/build/obj
MOC_DIR      = $$BUILD_BASE/build/moc
RCC_DIR      = $$BUILD_BASE/build/rcc
UI_DIR       = $$BUILD_BASE/build/ui

# ---------- 4. 打印（调试用，确认无误后可注释掉）----------
message("=== Build Config ===")
message("Platform : $$PLATFORM_TAG")
message("Type     : $$BUILD_TYPE")
message("DESTDIR  : $$DESTDIR")
message("OBJ DIR  : $$OBJECTS_DIR")