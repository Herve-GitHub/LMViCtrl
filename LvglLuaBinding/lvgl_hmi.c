/**
 * @file lvgl_hmi.c
 * @brief Linux ARM HMI entry point using DRM/KMS display and evdev input.
 */

#include "lvgl_lua_bindings.h"
#include "lvgl_lua_runtime.h"

#include "lvgl/src/drivers/display/drm/lv_linux_drm.h"
#include "lvgl/src/drivers/evdev/lv_evdev.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

static uint32_t hmi_tick_get_cb(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint32_t)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

LVGLLUABINDING_API int lvgl_hmi_run(const char * lua_script_path,
                                    const char * drm_path,
                                    int64_t connector_id,
                                    const char * pointer_path,
                                    const char * keypad_path)
{
    if (lua_script_path == NULL || lua_script_path[0] == '\0') {
        fprintf(stderr, "[hmi] lua_script_path is null or empty\n");
        return 1;
    }

    lv_init();
    lv_tick_set_cb(hmi_tick_get_cb);

    lv_display_t * disp = lv_linux_drm_create();
    if (disp == NULL) {
        fprintf(stderr, "[hmi] failed to create DRM display\n");
        return 2;
    }

    lv_linux_drm_set_file(disp,
                          (drm_path && drm_path[0]) ? drm_path : "/dev/dri/card0",
                          connector_id);

    if (pointer_path != NULL && pointer_path[0] != '\0') {
        lv_indev_t * pointer = lv_evdev_create(LV_INDEV_TYPE_POINTER, pointer_path);
        if (pointer == NULL) {
            fprintf(stderr, "[hmi] warning: failed to open pointer input: %s\n", pointer_path);
        }
    }

    if (keypad_path != NULL && keypad_path[0] != '\0') {
        lv_indev_t * keypad = lv_evdev_create(LV_INDEV_TYPE_KEYPAD, keypad_path);
        if (keypad == NULL) {
            fprintf(stderr, "[hmi] warning: failed to open keypad input: %s\n", keypad_path);
        }
    }

    lua_State * L = lvgl_lua_runtime_create_state();
    if (L == NULL) {
        fprintf(stderr, "[hmi] failed to create Lua state\n");
        return 3;
    }

    int rc = lvgl_lua_runtime_run_script(L, lua_script_path, "hmi");
    if (rc != 0) {
        lua_close(L);
        return rc;
    }

    while (1) {
        uint32_t delay = lv_timer_handler();
        if (delay > 100) delay = 100;
        if (delay < 1) delay = 1;
        usleep(delay * 1000u);
    }

    /* Unreachable in normal operation. */
    /* lua_close(L); */
    /* return 0; */
}

