/* Minimal Linux configuration for the qmake SDL2 build. */

#ifndef SDL_config_linux_h_
#define SDL_config_linux_h_
#define SDL_config_h_

#include "SDL_platform.h"

#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DLOPEN 1
#define HAVE_GETENV 1
#define HAVE_NANOSLEEP 1
#define HAVE_POLL 1
#define HAVE_PUTENV 1
#define HAVE_SEM_TIMEDWAIT 1
#define HAVE_SETENV 1
#define HAVE_SYSCONF 1
#define HAVE_UNSETENV 1

#ifdef __GNUC__
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1
#endif

#define SDL_AUDIO_DRIVER_DUMMY 1
#define SDL_FILESYSTEM_UNIX 1
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_JOYSTICK_DISABLED 1
#define SDL_LOADSO_DLOPEN 1
#define SDL_SENSOR_DISABLED 1
#define SDL_THREAD_PTHREAD 1
#define SDL_THREAD_PTHREAD_RECURSIVE_MUTEX 1
#define SDL_TIMER_UNIX 1
#define SDL_VIDEO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_X11 1
#define SDL_VIDEO_DRIVER_X11_DYNAMIC "libX11.so.6"
#define SDL_VIDEO_DRIVER_X11_HAS_XKBKEYCODETOKEYSYM 1
#define SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS 1
#define NO_SHARED_MEMORY 1

#endif /* SDL_config_linux_h_ */