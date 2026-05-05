/**
 * @file lvgl_simulator.c
 * @brief 仿真入口：初始化 LVGL + SDL，注册 Lua 绑定，运行设计器生成的 Lua 脚本
 */

#include "lvgl_lua_bindings.h"
#include "lvgl/src/drivers/sdl/lv_sdl_window.h"
#include "lvgl/src/drivers/sdl/lv_sdl_mouse.h"
#include "lvgl/src/drivers/sdl/lv_sdl_keyboard.h"

#include LV_SDL_INCLUDE_PATH
#include <stdio.h>
#include <string.h>

/* 把 SDL_GetTicks 桥接给 LVGL，作为 LV_USE_OS == LV_OS_NONE 时的 tick 来源 */
static uint32_t simu_tick_get_cb(void)
{
    return (uint32_t)SDL_GetTicks();
}

/* 取脚本所在目录（不含末尾斜杠）。返回值需调用方 free。 */
static char * simu_dirname_dup(const char * path)
{
    if (path == NULL) return NULL;
    size_t len = strlen(path);
    /* 找最后一个 '/' 或 '\\' */
    const char * last = NULL;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/' || path[i] == '\\') last = path + i;
    }
    if (last == NULL) {
        /* 无目录分隔符，认为在当前目录 */
        char * d = (char *)malloc(2);
        if (!d) return NULL;
        d[0] = '.'; d[1] = '\0';
        return d;
    }
    size_t dlen = (size_t)(last - path);
    if (dlen == 0) dlen = 1; /* 根 */
    char * d = (char *)malloc(dlen + 1);
    if (!d) return NULL;
    memcpy(d, path, dlen);
    d[dlen] = '\0';
    return d;
}

/* 把工程目录及常用子目录加入 Lua package.path，使 require("runtime") /
 * require("widgets.xxx") / require("common.xxx") 都能在工程目录中找到。 */
static void simu_setup_lua_path(lua_State * L, const char * script_dir)
{
    if (L == NULL || script_dir == NULL) return;

    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    /* 取原有 package.path 以兼容 Lua 内置标准库路径 */
    lua_getfield(L, -1, "path");
    const char * old_path = lua_tostring(L, -1);

    lua_pushfstring(L,
        "%s/?.lua;%s/?/init.lua;"
        "%s/widgets/?.lua;%s/widgets/?/init.lua;"
        "%s/common/?.lua;%s/common/?/init.lua;%s",
        script_dir, script_dir,
        script_dir, script_dir,
        script_dir, script_dir,
        old_path ? old_path : "");

    /* stack: package, oldpath, newpath */
    lua_setfield(L, -3, "path");
    lua_pop(L, 2); /* pop oldpath + package */
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
    lua_State * L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "[simu] failed to create Lua state\n");
        return 3;
    }
    luaL_openlibs(L);
    lvgl_lua_register(L);

    /* 让 require 能从工程目录解析 runtime / widgets.* / common.* */
    char * script_dir = simu_dirname_dup(lua_script_path);
    if (script_dir != NULL) {
        simu_setup_lua_path(L, script_dir);
    }

    /* 4. 加载并执行设计器生成的 Lua 脚本 */
    if (luaL_dofile(L, lua_script_path) != LUA_OK) {
        const char * msg = lua_tostring(L, -1);
        fprintf(stderr, "[simu] lua error: %s\n", msg ? msg : "(unknown)");
        if (script_dir) free(script_dir);
        lua_close(L);
        return 4;
    }
    if (script_dir) { free(script_dir); script_dir = NULL; }

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
