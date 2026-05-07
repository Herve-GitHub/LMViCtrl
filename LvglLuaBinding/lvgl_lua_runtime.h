/**
 * @file lvgl_lua_runtime.h
 * @brief Shared Lua project loader for LVGL runtime entry points.
 */

#ifndef LVGL_LUA_RUNTIME_H
#define LVGL_LUA_RUNTIME_H
#include "lvgl_lua_bindings.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "lua/lua.h"
 LVGLLUABINDING_API lua_State * lvgl_lua_runtime_create_state(void);
 LVGLLUABINDING_API int lvgl_lua_runtime_run_script(lua_State * L,
                                const char * lua_script_path,
                                const char * log_prefix);
#ifdef __cplusplus
}
#endif

#endif /* LVGL_LUA_RUNTIME_H */
