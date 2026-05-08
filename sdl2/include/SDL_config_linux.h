/* Minimal Linux configuration for the qmake SDL2 build. */

#ifndef SDL_config_linux_h_
#define SDL_config_linux_h_
#define SDL_config_h_

#include "SDL_platform.h"

#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DLOPEN 1
#define HAVE_NANOSLEEP 1
#define HAVE_SEM_TIMEDWAIT 1
#define HAVE_SYSCONF 1

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

#endif /* SDL_config_linux_h_ */