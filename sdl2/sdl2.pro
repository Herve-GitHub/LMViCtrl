# Qt project file for SDL2
# Supports: Windows (MinGW) and Linux (GCC/MinGW)
# Build type: Shared Library

TEMPLATE = lib
TARGET   = SDL2
CONFIG  += shared
CONFIG  -= qt
# 引入目录配置
include($$PWD/../build_dirs.pri)

DESTDIR = $$TARDIR
DEPENDPATH += $$TARDIR
OBJECTS_DIR = $$BUILD_PATH/$$TARGET/obj
MOC_DIR = $$BUILD_PATH/$$TARGET/moc
RCC_DIR = $$BUILD_PATH/$$TARGET/rcc
UI_DIR =  $$BUILD_PATH/$$TARGET/ui

# Avoid object name collisions between source files sharing the same
# basename across different directories (e.g.
# src/thread/windows/SDL_sysmutex.c vs src/thread/generic/SDL_sysmutex.c).
# Without this, qmake's flat OBJECTS_DIR + nmake batch inference rules can
# cause the wrong source to be compiled, producing linker errors like
# "unresolved external symbol SDL_mutex_impl_active".
#  - object_parallel_to_source: place .obj files under a directory tree
#    that mirrors the source layout so basenames never clash.
#  - no_batch: emit explicit per-file compile commands instead of nmake
#    batch inference rules ({src}.c{obj}.obj::), so the matching source
#    cannot be ambiguous.
CONFIG  += object_parallel_to_source no_batch

# Suppress Qt-specific additions
QT -= core gui

# Include paths (common)
INCLUDEPATH += include \
               src \
               src/video/khronos

# Compiler-specific flags
*-g++*|*-gcc*|*mingw*|*-clang* {
    QMAKE_CFLAGS += -std=c99 -msse2 -fno-strict-aliasing
}
*msvc* {
    # MSVC: enable C11, SSE2 is implicit on x64, suppress noisy warnings,
    # and define _CRT_* macros so MS CRT doesn't complain about "unsafe" funcs.
    QMAKE_CFLAGS += /std:c11 /wd4244 /wd4267 /wd4018 /wd4146 /wd4090 /wd4133 /wd4101 /wd4102 /wd4146 /wd4456 /wd4457 /wd4459
    DEFINES += _CRT_SECURE_NO_WARNINGS \
               _CRT_NONSTDC_NO_DEPRECATE \
               HAVE_LIBC=1
}

# -----------------------------------------------------------------------
# Windows (MinGW) — platform-specific sources, defines and libs
# -----------------------------------------------------------------------
win32 {
    DEFINES += DLL_EXPORT _WINDOWS __WIN32__

    LIBS += -lsetupapi \
            -lwinmm \
            -limm32 \
            -lversion \
            -luser32 \
            -lgdi32 \
            -lole32 \
            -loleaut32 \
            -lshell32 \
            -ladvapi32 \
            -loleaut32

    SOURCES += \
        src/audio/directsound/SDL_directsound.c \
        src/audio/disk/SDL_diskaudio.c \
        src/audio/dummy/SDL_dummyaudio.c \
        src/audio/wasapi/SDL_wasapi.c \
        src/audio/wasapi/SDL_wasapi_win32.c \
        src/audio/winmm/SDL_winmm.c \
        src/core/windows/SDL_hid.c \
        src/core/windows/SDL_immdevice.c \
        src/core/windows/SDL_windows.c \
        src/core/windows/SDL_xinput.c \
        src/filesystem/windows/SDL_sysfilesystem.c \
        src/haptic/windows/SDL_dinputhaptic.c \
        src/haptic/windows/SDL_windowshaptic.c \
        src/haptic/windows/SDL_xinputhaptic.c \
        src/joystick/windows/SDL_dinputjoystick.c \
        src/joystick/windows/SDL_rawinputjoystick.c \
        src/joystick/windows/SDL_windowsjoystick.c \
        src/joystick/windows/SDL_windows_gaming_input.c \
        src/joystick/windows/SDL_xinputjoystick.c \
        src/loadso/windows/SDL_sysloadso.c \
        src/locale/windows/SDL_syslocale.c \
        src/misc/windows/SDL_sysurl.c \
        src/power/windows/SDL_syspower.c \
        src/render/direct3d/SDL_render_d3d.c \
        src/render/direct3d/SDL_shaders_d3d.c \
        src/render/direct3d11/SDL_render_d3d11.c \
        src/render/direct3d11/SDL_shaders_d3d11.c \
        src/render/direct3d12/SDL_render_d3d12.c \
        src/render/direct3d12/SDL_shaders_d3d12.c \
        src/sensor/windows/SDL_windowssensor.c \
        src/thread/windows/SDL_syscond_cv.c \
        src/thread/windows/SDL_sysmutex.c \
        src/thread/windows/SDL_syssem.c \
        src/thread/windows/SDL_systhread.c \
        src/thread/windows/SDL_systls.c \
        src/timer/windows/SDL_systimer.c \
        src/video/windows/SDL_windowsclipboard.c \
        src/video/windows/SDL_windowsevents.c \
        src/video/windows/SDL_windowsframebuffer.c \
        src/video/windows/SDL_windowskeyboard.c \
        src/video/windows/SDL_windowsmessagebox.c \
        src/video/windows/SDL_windowsmodes.c \
        src/video/windows/SDL_windowsmouse.c \
        src/video/windows/SDL_windowsopengl.c \
        src/video/windows/SDL_windowsopengles.c \
        src/video/windows/SDL_windowsshape.c \
        src/video/windows/SDL_windowsvideo.c \
        src/video/windows/SDL_windowsvulkan.c \
        src/video/windows/SDL_windowswindow.c
    RC_FILE = src/main/windows/version.rc
}

# -----------------------------------------------------------------------
# Linux — platform-specific sources, defines and libs
# -----------------------------------------------------------------------
unix:!macx {
    DEFINES += __LINUX__

    LIBS += -ldl \
            -lpthread \
            -lm \
            -lX11 \
            -lXext \
            -lXrandr \
            -lXi \
            -lXfixes \
            -lXcursor \
            -lasound

    SOURCES += \
        src/audio/alsa/SDL_alsa_audio.c \
        src/audio/disk/SDL_diskaudio.c \
        src/audio/dummy/SDL_dummyaudio.c \
        src/filesystem/unix/SDL_sysfilesystem.c \
        src/haptic/linux/SDL_syshaptic.c \
        src/joystick/linux/SDL_sysjoystick.c \
        src/loadso/dlopen/SDL_sysloadso.c \
        src/locale/unix/SDL_syslocale.c \
        src/misc/unix/SDL_sysurl.c \
        src/power/linux/SDL_syspower.c \
        src/sensor/dummy/SDL_dummysensor.c \
        src/thread/pthread/SDL_syscond.c \
        src/thread/pthread/SDL_sysmutex.c \
        src/thread/pthread/SDL_syssem.c \
        src/thread/pthread/SDL_systhread.c \
        src/thread/pthread/SDL_systls.c \
        src/timer/unix/SDL_systimer.c \
        src/video/x11/edid-parse.c \
        src/video/x11/SDL_x11clipboard.c \
        src/video/x11/SDL_x11dyn.c \
        src/video/x11/SDL_x11events.c \
        src/video/x11/SDL_x11framebuffer.c \
        src/video/x11/SDL_x11keyboard.c \
        src/video/x11/SDL_x11messagebox.c \
        src/video/x11/SDL_x11modes.c \
        src/video/x11/SDL_x11mouse.c \
        src/video/x11/SDL_x11opengl.c \
        src/video/x11/SDL_x11opengles.c \
        src/video/x11/SDL_x11shape.c \
        src/video/x11/SDL_x11touch.c \
        src/video/x11/SDL_x11video.c \
        src/video/x11/SDL_x11vulkan.c \
        src/video/x11/SDL_x11window.c \
        src/video/x11/SDL_x11xfixes.c \
        src/video/x11/SDL_x11xinput2.c
}

# -----------------------------------------------------------------------
# Common source files (shared by all platforms)
# -----------------------------------------------------------------------
SOURCES += \
    src/SDL.c \
    src/SDL_assert.c \
    src/SDL_dataqueue.c \
    src/SDL_error.c \
    src/SDL_guid.c \
    src/SDL_hints.c \
    src/SDL_list.c \
    src/SDL_log.c \
    src/SDL_utils.c \
    src/atomic/SDL_atomic.c \
    src/atomic/SDL_spinlock.c \
    src/audio/SDL_audio.c \
    src/audio/SDL_audiocvt.c \
    src/audio/SDL_audiodev.c \
    src/audio/SDL_audiotypecvt.c \
    src/audio/SDL_mixer.c \
    src/audio/SDL_wave.c \
    src/cpuinfo/SDL_cpuinfo.c \
    src/dynapi/SDL_dynapi.c \
    src/events/SDL_clipboardevents.c \
    src/events/SDL_displayevents.c \
    src/events/SDL_dropevents.c \
    src/events/SDL_events.c \
    src/events/SDL_gesture.c \
    src/events/SDL_keyboard.c \
    src/events/SDL_mouse.c \
    src/events/SDL_quit.c \
    src/events/SDL_touch.c \
    src/events/SDL_windowevents.c \
    src/file/SDL_rwops.c \
    src/haptic/dummy/SDL_syshaptic.c \
    src/haptic/SDL_haptic.c \
    src/hidapi/SDL_hidapi.c \
    src/joystick/controller_type.c \
    src/joystick/dummy/SDL_sysjoystick.c \
    src/joystick/hidapi/SDL_hidapijoystick.c \
    src/joystick/hidapi/SDL_hidapi_8bitdo.c \
    src/joystick/hidapi/SDL_hidapi_combined.c \
    src/joystick/hidapi/SDL_hidapi_gamecube.c \
    src/joystick/hidapi/SDL_hidapi_luna.c \
    src/joystick/hidapi/SDL_hidapi_ps3.c \
    src/joystick/hidapi/SDL_hidapi_ps4.c \
    src/joystick/hidapi/SDL_hidapi_ps5.c \
    src/joystick/hidapi/SDL_hidapi_rumble.c \
    src/joystick/hidapi/SDL_hidapi_shield.c \
    src/joystick/hidapi/SDL_hidapi_stadia.c \
    src/joystick/hidapi/SDL_hidapi_steam.c \
    src/joystick/hidapi/SDL_hidapi_steamdeck.c \
    src/joystick/hidapi/SDL_hidapi_switch.c \
    src/joystick/hidapi/SDL_hidapi_wii.c \
    src/joystick/hidapi/SDL_hidapi_xbox360.c \
    src/joystick/hidapi/SDL_hidapi_xbox360w.c \
    src/joystick/hidapi/SDL_hidapi_xboxone.c \
    src/joystick/SDL_gamecontroller.c \
    src/joystick/SDL_joystick.c \
    src/joystick/SDL_steam_virtual_gamepad.c \
    src/joystick/virtual/SDL_virtualjoystick.c \
    src/libm/e_atan2.c \
    src/libm/e_exp.c \
    src/libm/e_fmod.c \
    src/libm/e_log.c \
    src/libm/e_log10.c \
    src/libm/e_pow.c \
    src/libm/e_rem_pio2.c \
    src/libm/e_sqrt.c \
    src/libm/k_cos.c \
    src/libm/k_rem_pio2.c \
    src/libm/k_sin.c \
    src/libm/k_tan.c \
    src/libm/s_atan.c \
    src/libm/s_copysign.c \
    src/libm/s_cos.c \
    src/libm/s_fabs.c \
    src/libm/s_floor.c \
    src/libm/s_scalbn.c \
    src/libm/s_sin.c \
    src/libm/s_tan.c \
    src/locale/SDL_locale.c \
    src/misc/SDL_url.c \
    src/power/SDL_power.c \
    src/render/opengl/SDL_render_gl.c \
    src/render/opengl/SDL_shaders_gl.c \
    src/render/opengles2/SDL_render_gles2.c \
    src/render/opengles2/SDL_shaders_gles2.c \
    src/render/SDL_d3dmath.c \
    src/render/SDL_render.c \
    src/render/SDL_yuv_sw.c \
    src/render/software/SDL_blendfillrect.c \
    src/render/software/SDL_blendline.c \
    src/render/software/SDL_blendpoint.c \
    src/render/software/SDL_drawline.c \
    src/render/software/SDL_drawpoint.c \
    src/render/software/SDL_render_sw.c \
    src/render/software/SDL_rotate.c \
    src/render/software/SDL_triangle.c \
    src/sensor/dummy/SDL_dummysensor.c \
    src/sensor/SDL_sensor.c \
    src/stdlib/SDL_crc16.c \
    src/stdlib/SDL_crc32.c \
    src/stdlib/SDL_getenv.c \
    src/stdlib/SDL_iconv.c \
    src/stdlib/SDL_malloc.c \
    src/stdlib/SDL_mslibc.c \
    src/stdlib/SDL_qsort.c \
    src/stdlib/SDL_stdlib.c \
    src/stdlib/SDL_string.c \
    src/stdlib/SDL_strtokr.c \
    src/thread/generic/SDL_syscond.c \
    src/thread/SDL_thread.c \
    src/timer/SDL_timer.c \
    src/video/dummy/SDL_nullevents.c \
    src/video/dummy/SDL_nullframebuffer.c \
    src/video/dummy/SDL_nullvideo.c \
    src/video/SDL_blit.c \
    src/video/SDL_blit_0.c \
    src/video/SDL_blit_1.c \
    src/video/SDL_blit_A.c \
    src/video/SDL_blit_auto.c \
    src/video/SDL_blit_copy.c \
    src/video/SDL_blit_N.c \
    src/video/SDL_blit_slow.c \
    src/video/SDL_bmp.c \
    src/video/SDL_clipboard.c \
    src/video/SDL_egl.c \
    src/video/SDL_fillrect.c \
    src/video/SDL_pixels.c \
    src/video/SDL_rect.c \
    src/video/SDL_RLEaccel.c \
    src/video/SDL_shape.c \
    src/video/SDL_stretch.c \
    src/video/SDL_surface.c \
    src/video/SDL_video.c \
    src/video/SDL_vulkan_utils.c \
    src/video/SDL_yuv.c \
    src/video/yuv2rgb/yuv_rgb_lsx.c \
    src/video/yuv2rgb/yuv_rgb_sse.c \
    src/video/yuv2rgb/yuv_rgb_std.c

# -----------------------------------------------------------------------
# Public header files
# -----------------------------------------------------------------------
HEADERS += \
    include/begin_code.h \
    include/close_code.h \
    include/SDL.h \
    include/SDL_assert.h \
    include/SDL_atomic.h \
    include/SDL_audio.h \
    include/SDL_bits.h \
    include/SDL_blendmode.h \
    include/SDL_clipboard.h \
    include/SDL_config.h \
    include/SDL_config_windows.h \
    include/SDL_copying.h \
    include/SDL_cpuinfo.h \
    include/SDL_egl.h \
    include/SDL_endian.h \
    include/SDL_error.h \
    include/SDL_events.h \
    include/SDL_filesystem.h \
    include/SDL_gamecontroller.h \
    include/SDL_gesture.h \
    include/SDL_guid.h \
    include/SDL_haptic.h \
    include/SDL_hidapi.h \
    include/SDL_hints.h \
    include/SDL_joystick.h \
    include/SDL_keyboard.h \
    include/SDL_keycode.h \
    include/SDL_loadso.h \
    include/SDL_locale.h \
    include/SDL_log.h \
    include/SDL_main.h \
    include/SDL_messagebox.h \
    include/SDL_metal.h \
    include/SDL_misc.h \
    include/SDL_mouse.h \
    include/SDL_mutex.h \
    include/SDL_name.h \
    include/SDL_opengl.h \
    include/SDL_opengl_glext.h \
    include/SDL_opengles.h \
    include/SDL_opengles2.h \
    include/SDL_opengles2_gl2.h \
    include/SDL_opengles2_gl2ext.h \
    include/SDL_opengles2_gl2platform.h \
    include/SDL_opengles2_khrplatform.h \
    include/SDL_pixels.h \
    include/SDL_platform.h \
    include/SDL_power.h \
    include/SDL_quit.h \
    include/SDL_rect.h \
    include/SDL_render.h \
    include/SDL_revision.h \
    include/SDL_rwops.h \
    include/SDL_scancode.h \
    include/SDL_sensor.h \
    include/SDL_shape.h \
    include/SDL_stdinc.h \
    include/SDL_surface.h \
    include/SDL_system.h \
    include/SDL_syswm.h \
    include/SDL_thread.h \
    include/SDL_timer.h \
    include/SDL_touch.h \
    include/SDL_types.h \
    include/SDL_version.h \
    include/SDL_video.h \
    include/SDL_vulkan.h
