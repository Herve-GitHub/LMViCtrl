/**
 * @file lvgl_simulator.c
 * @brief 仿真入口：初始化 LVGL + SDL，注册 Lua 绑定，运行设计器生成的 Lua 脚本
 */

#include "lvgl_lua_bindings.h"
#include "lvgl_lua_runtime.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_keyboard.h"

#include LV_SDL_INCLUDE_PATH
#include <stdio.h>

/* 把 SDL_GetTicks 桥接给 LVGL，作为 LV_USE_OS == LV_OS_NONE 时的 tick 来源 */
static uint32_t simu_tick_get_cb(void)
{
    return (uint32_t)SDL_GetTicks();
}

/**
 * @brief 运行一次仿真：初始化显示/输入设备，加载并执行 Lua 脚本，进入主循环
 *
 * @param lua_script_path  设计器生成的 Lua 入口脚本路径（必填）
 * @param hor_res          仿真窗口宽度（<=0 时使用 800）
 * @param ver_res          仿真窗口高度（<=0 时使用 480）
 * @param window_title     仿真窗口标题（NULL 时使用默认值）
 * @return 0 表示成功，其它值为错误码
 *
 * 注意：LV_SDL_DIRECT_EXIT == 1，关闭 SDL 窗口时由 LVGL 内部 exit(0)，
 * 因此本函数正常情况下不会返回（除非脚本加载失败等）。
 */
LVGLLUABINDING_API int lvgl_simulator_run(const char * lua_script_path,
                                          int hor_res, int ver_res,
                                          const char * window_title)
{
    if (lua_script_path == NULL || lua_script_path[0] == '\0') {
        fprintf(stderr, "[simu] lua_script_path is null or empty\n");
        return 1;
    }
    if (hor_res <= 0) hor_res = 800;
    if (ver_res <= 0) ver_res = 480;

    /* 1. LVGL 初始化 */
    lv_init();
    lv_tick_set_cb(simu_tick_get_cb);

    /* 2. SDL 显示与输入设备 */
    lv_display_t * disp = lv_sdl_window_create((int32_t)hor_res, (int32_t)ver_res);
    if (disp == NULL) {
        fprintf(stderr, "[simu] failed to create SDL window\n");
        return 2;
    }
    if (window_title != NULL && window_title[0] != '\0') {
        lv_sdl_window_set_title(disp, window_title);
    }
    lv_sdl_mouse_create();
    lv_sdl_keyboard_create();

    /* 3. Lua 状态机与绑定 */
    lua_State * L = lvgl_lua_runtime_create_state();
    if (L == NULL) {
        fprintf(stderr, "[simu] failed to create Lua state\n");
        return 3;
    }

    /* 4. 加载并执行设计器生成的 Lua 脚本 */
    int rc = lvgl_lua_runtime_run_script(L, lua_script_path, "simu");
    if (rc != 0) {
        lua_close(L);
        return rc;
    }

    /* 5. 主循环：让 LVGL 内部的 SDL 事件 timer 处理输入与刷新 */
    while (1) {
        uint32_t delay = lv_timer_handler();
        if (delay > 100) delay = 100;
        if (delay < 1)   delay = 1;
        SDL_Delay(delay);
    }

    /* 不会到达这里（SDL_QUIT 时由 LVGL 直接 exit(0)） */
    /* lua_close(L); */
    /* return 0; */
}
