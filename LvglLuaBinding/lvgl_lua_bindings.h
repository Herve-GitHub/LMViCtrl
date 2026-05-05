/**
 * @file lvgl_lua_bindings.h
 * @brief LVGL Lua bindings for VduEditor
 */
//公共头文件
#ifndef LVGL_LUA_BINDINGS_H
#define LVGL_LUA_BINDINGS_H

// Platform-specific DLL export/import macros
#if defined(_MSC_VER)
    #ifndef LVGLLUABINDING_EXPORTS
        #define LVGLLUABINDING_API __declspec(dllimport)
    #else
        #define LVGLLUABINDING_API __declspec(dllexport)
    #endif
#else
    // For GCC/non-Windows platforms, no DLL export needed
    #define LVGLLUABINDING_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
#include "lvgl/lvgl.h"

/**
 * @brief Register all LVGL functions to Lua state
 * @param L Lua state
 */
LVGLLUABINDING_API void lvgl_lua_register(lua_State* L);
LVGLLUABINDING_API void set_current_ttf_font(lv_font_t* font);
LVGLLUABINDING_API lv_font_t* get_current_ttf_font(void);

/**
 * @brief 启动 LVGL 仿真：创建 SDL 窗口/输入，注册 Lua 绑定并运行指定脚本
 *
 * @param lua_script_path  设计器生成的 Lua 入口脚本路径（必填）
 * @param hor_res          仿真窗口宽度（<=0 时使用默认 800）
 * @param ver_res          仿真窗口高度（<=0 时使用默认 480）
 * @param window_title     仿真窗口标题（NULL 时使用 SDL 默认）
 * @return 0 成功，其它值为错误码（在 LV_SDL_DIRECT_EXIT==1 时通常不会返回）
 */
LVGLLUABINDING_API int lvgl_simulator_run(const char * lua_script_path,
                                          int hor_res, int ver_res,
                                          const char * window_title);
#ifdef __cplusplus
}
#endif

#endif /* LVGL_LUA_BINDINGS_H */
