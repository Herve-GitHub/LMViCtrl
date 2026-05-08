/**
 * @file lvgl_lua_runtime.c
 * @brief Shared Lua project loader for simulator and HMI runtime entry points.
 */

#include "lvgl_lua_runtime.h"

#include "lvgl_lua_bindings.h"

#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char * runtime_dirname_dup(const char * path)
{
    if (path == NULL) return NULL;

    size_t len = strlen(path);
    const char * last = NULL;
    for (size_t i = 0; i < len; ++i) {
        if (path[i] == '/' || path[i] == '\\') last = path + i;
    }

    if (last == NULL) {
        char * d = (char *)malloc(2);
        if (!d) return NULL;
        d[0] = '.';
        d[1] = '\0';
        return d;
    }

    size_t dlen = (size_t)(last - path);
    if (dlen == 0) dlen = 1;

    char * d = (char *)malloc(dlen + 1);
    if (!d) return NULL;
    memcpy(d, path, dlen);
    d[dlen] = '\0';
    return d;
}

static void runtime_setup_lua_path(lua_State * L, const char * script_dir)
{
    if (L == NULL || script_dir == NULL) return;

    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

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

    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
}

lua_State * lvgl_lua_runtime_create_state(void)
{
    lua_State * L = luaL_newstate();
    if (L == NULL) return NULL;

    luaL_openlibs(L);
    lvgl_lua_register(L);
    return L;
}

int lvgl_lua_runtime_run_script(lua_State * L,
                                const char * lua_script_path,
                                const char * log_prefix)
{
    const char * prefix = (log_prefix && log_prefix[0]) ? log_prefix : "runtime";

    if (L == NULL) {
        fprintf(stderr, "[%s] lua state is null\n", prefix);
        return 3;
    }
    if (lua_script_path == NULL || lua_script_path[0] == '\0') {
        fprintf(stderr, "[%s] lua_script_path is null or empty\n", prefix);
        return 1;
    }

    char * script_dir = runtime_dirname_dup(lua_script_path);
    if (script_dir != NULL) {
        runtime_setup_lua_path(L, script_dir);
    }

    if (luaL_dofile(L, lua_script_path) != LUA_OK) {
        const char * msg = lua_tostring(L, -1);
        fprintf(stderr, "[%s] lua error: %s\n", prefix, msg ? msg : "(unknown)");
        if (script_dir) free(script_dir);
        return 4;
    }

    if (script_dir) free(script_dir);
    return 0;
}

