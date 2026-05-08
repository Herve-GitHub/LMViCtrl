/**
 * @file lvgl_offscreen_render.c
 * @brief Real LVGL offscreen renderer for designer previews.
 */

/* This file defines exported functions inside LvglLuaBinding.dll. Some editor
 * diagnostics do not inherit the qmake/MSBuild project macro, so keep the DLL
 * export intent explicit here as well. */
#ifndef LVGLLUABINDING_EXPORTS
#define LVGLLUABINDING_EXPORTS
#endif

#include "lvgl_lua_bindings_internal.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct offscreen_render_ctx_t {
    /* Target QImage::Format_ARGB32 buffer owned by the designer process. */
    unsigned char * out;
    int width;
    int height;
    int stride;
} offscreen_render_ctx_t;

typedef struct offscreen_font_t {
    lv_font_t * font;
    void * data;
} offscreen_font_t;

static uint32_t offscreen_tick_get_cb(void)
{
    /* LVGL needs a monotonically increasing tick source even when no simulator
     * window or SDL timer is running. */
    return (uint32_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

static void set_error(char * error_buf, int error_buf_size, const char * msg)
{
    if(error_buf == NULL || error_buf_size <= 0) return;
    if(msg == NULL) msg = "unknown error";
    snprintf(error_buf, (size_t)error_buf_size, "%s", msg);
}

static char * dirname_dup(const char * path)
{
    /* Keep path parsing local to this file so the exported API stays plain C
     * and does not depend on Qt or platform-specific helpers. */
    if(path == NULL) return NULL;
    size_t len = strlen(path);
    const char * last = NULL;
    for(size_t i = 0; i < len; ++i) {
        if(path[i] == '/' || path[i] == '\\') last = path + i;
    }
    if(last == NULL) {
        char * d = (char *)malloc(2);
        if(d) { d[0] = '.'; d[1] = '\0'; }
        return d;
    }
    size_t dlen = (size_t)(last - path);
    if(dlen == 0) dlen = 1;
    char * d = (char *)malloc(dlen + 1);
    if(d == NULL) return NULL;
    memcpy(d, path, dlen);
    d[dlen] = '\0';
    return d;
}

static int is_sep(char c)
{
    return c == '/' || c == '\\';
}

static char * lua_root_from_widget_path(const char * path)
{
    /* A widget path normally looks like .../lua/widgets/custom/valve.lua.
     * Lua modules inside widgets often require common modules by package path,
     * so we recover the .../lua root and add both widgets and common below. */
    if(path == NULL) return NULL;
    const char * p = path;
    const char * widgets = NULL;
    while(*p) {
        if((p == path || is_sep(*(p - 1))) && strncmp(p, "widgets", 7) == 0 &&
            (p[7] == '\0' || is_sep(p[7]))) {
            widgets = p;
            break;
        }
        ++p;
    }
    if(widgets == NULL || widgets == path) return dirname_dup(path);
    const char * end = widgets;
    if(end > path && is_sep(*(end - 1))) --end;
    size_t len = (size_t)(end - path);
    char * root = (char *)malloc(len + 1);
    if(root == NULL) return NULL;
    memcpy(root, path, len);
    root[len] = '\0';
    return root;
}

static void setup_lua_path(lua_State * L, const char * script_dir, const char * lua_root)
{
    /* Let the widget module resolve local files, widget modules, and common
     * helpers exactly like the simulator/runtime layout does. */
    lua_getglobal(L, "package");
    if(!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "path");
    const char * old_path = lua_tostring(L, -1);

    lua_pushfstring(L,
        "%s/?.lua;%s/?/init.lua;"
        "%s/?.lua;%s/?/init.lua;"
        "%s/widgets/?.lua;%s/widgets/?/init.lua;"
        "%s/common/?.lua;%s/common/?/init.lua;%s",
        script_dir ? script_dir : ".", script_dir ? script_dir : ".",
        lua_root ? lua_root : ".", lua_root ? lua_root : ".",
        lua_root ? lua_root : ".", lua_root ? lua_root : ".",
        lua_root ? lua_root : ".", lua_root ? lua_root : ".",
        old_path ? old_path : "");

    lua_setfield(L, -3, "path");
    lua_pop(L, 2);
}

static void push_json_value(lua_State * L, const cJSON * item)
{
    /* The designer only sends simple property values for now. Complex JSON
     * values are ignored as nil instead of inventing a partial mapping. */
    if(cJSON_IsBool(item)) lua_pushboolean(L, cJSON_IsTrue(item));
    else if(cJSON_IsNumber(item)) lua_pushnumber(L, item->valuedouble);
    else if(cJSON_IsString(item)) lua_pushstring(L, item->valuestring ? item->valuestring : "");
    else lua_pushnil(L);
}

static void push_state_table(lua_State * L, const char * state_json, int width, int height)
{
    /* Build the state table passed to widget.new(parent, state). Geometry is
     * forced to the offscreen canvas so the preview starts at (0, 0). */
    lua_newtable(L);

    cJSON * root = NULL;
    if(state_json != NULL && state_json[0] != '\0') root = cJSON_Parse(state_json);
    if(root != NULL && cJSON_IsObject(root)) {
        const cJSON * item = NULL;
        cJSON_ArrayForEach(item, root) {
            if(item->string == NULL) continue;
            push_json_value(L, item);
            lua_setfield(L, -2, item->string);
        }
    }
    if(root) cJSON_Delete(root);

    lua_pushinteger(L, 0); lua_setfield(L, -2, "x");
    lua_pushinteger(L, 0); lua_setfield(L, -2, "y");
    lua_pushinteger(L, width); lua_setfield(L, -2, "width");
    lua_pushinteger(L, height); lua_setfield(L, -2, "height");
    lua_pushboolean(L, 1); lua_setfield(L, -2, "design_mode");
}

static offscreen_font_t load_offscreen_font(const char * font_path, int font_size)
{
    offscreen_font_t result;
    result.font = NULL;
    result.data = NULL;

#if LV_USE_TINY_TTF
    if(font_path == NULL || font_path[0] == '\0') return result;
    if(font_size <= 0) font_size = 16;

    FILE * fp = fopen(font_path, "rb");
    if(fp == NULL) return result;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if(file_size <= 0) {
        fclose(fp);
        return result;
    }

    void * data = malloc((size_t)file_size);
    if(data == NULL) {
        fclose(fp);
        return result;
    }
    if(fread(data, 1, (size_t)file_size, fp) != (size_t)file_size) {
        fclose(fp);
        free(data);
        return result;
    }
    fclose(fp);

    lv_font_t * font = lv_tiny_ttf_create_data_ex(
        data, (size_t)file_size, font_size,
        LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
    if(font == NULL) {
        free(data);
        return result;
    }

    result.font = font;
    result.data = data;
#else
    LV_UNUSED(font_path);
    LV_UNUSED(font_size);
#endif

    return result;
}

static void destroy_offscreen_font(offscreen_font_t * font)
{
    if(font == NULL) return;
#if LV_USE_TINY_TTF
    if(font->font) lv_tiny_ttf_destroy(font->font);
#endif
    if(font->data) free(font->data);
    font->font = NULL;
    font->data = NULL;
}

static void offscreen_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    /* LVGL calls this when a rendered area is ready. Copy only the flushed
     * rectangle into the QImage-compatible ARGB32 destination buffer. */
    offscreen_render_ctx_t * ctx = (offscreen_render_ctx_t *)lv_display_get_user_data(disp);
    if(ctx == NULL || ctx->out == NULL || area == NULL || px_map == NULL) {
        lv_display_flush_ready(disp);
        return;
    }

    const int area_w = (int)(area->x2 - area->x1 + 1);
    const int area_h = (int)(area->y2 - area->y1 + 1);
    const lv_color32_t * src = (const lv_color32_t *)px_map;

    for(int row = 0; row < area_h; ++row) {
        const int y = (int)area->y1 + row;
        if(y < 0 || y >= ctx->height) continue;
        unsigned char * dst = ctx->out + y * ctx->stride;
        for(int col = 0; col < area_w; ++col) {
            const int x = (int)area->x1 + col;
            if(x < 0 || x >= ctx->width) continue;
            const lv_color32_t c = src[row * area_w + col];
            unsigned char * d = dst + x * 4;
            /* Qt stores Format_ARGB32 as BGRA bytes on little-endian Windows. */
            d[0] = c.blue;
            d[1] = c.green;
            d[2] = c.red;
            d[3] = c.alpha ? c.alpha : 255;
        }
    }

    lv_display_flush_ready(disp);
}

LVGLLUABINDING_API int lvgl_lua_render_widget_to_argb32(const char * lua_widget_path,
                                                        const char * state_json,
                                                        const char * font_path,
                                                        int font_size,
                                                        int width,
                                                        int height,
                                                        unsigned char * out_argb32,
                                                        int out_stride,
                                                        char * error_buf,
                                                        int error_buf_size)
{
    static int lvgl_ready = 0;

    /* Return small numeric error codes to keep the DLL boundary simple; the
     * human-readable message is written to error_buf when available. */
    if(lua_widget_path == NULL || lua_widget_path[0] == '\0') {
        set_error(error_buf, error_buf_size, "lua_widget_path is empty");
        return 1;
    }
    if(width <= 0 || height <= 0 || out_argb32 == NULL || out_stride < width * 4) {
        set_error(error_buf, error_buf_size, "invalid output buffer or size");
        return 2;
    }

    /* Start with a transparent image. If Lua or LVGL fails later, the caller can
     * discard this buffer and fall back to draw_hints/placeholder rendering. */
    memset(out_argb32, 0, (size_t)out_stride * (size_t)height);

    if(!lvgl_ready) {
        /* LVGL is process-global, so initialize it once and create/delete only
         * the temporary display for each preview render. */
        lv_init();
        lv_tick_set_cb(offscreen_tick_get_cb);
        lvgl_ready = 1;
    }

    offscreen_render_ctx_t ctx = { out_argb32, width, height, out_stride };

    /* Preserve the previous display because designer previews may be rendered
     * from a process that already initialized LVGL for another purpose. */
    lv_display_t * previous_disp = lv_display_get_default();
    lv_display_t * disp = lv_display_create(width, height);
    if(disp == NULL) {
        set_error(error_buf, error_buf_size, "failed to create LVGL display");
        return 3;
    }

    lv_display_set_default(disp);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_user_data(disp, &ctx);
    lv_display_set_flush_cb(disp, offscreen_flush_cb);

    /* FULL render mode is straightforward for previews: the whole widget area
     * is available, and LVGL can flush it through one predictable buffer. */
    const uint32_t buf_size = (uint32_t)(width * height * 4);
    void * draw_buf = malloc(buf_size);
    if(draw_buf == NULL) {
        lv_display_delete(disp);
        lv_display_set_default(previous_disp);
        set_error(error_buf, error_buf_size, "failed to allocate LVGL draw buffer");
        return 4;
    }
    lv_display_set_buffers(disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_FULL);

    lv_font_t * previous_font = get_current_ttf_font();
    offscreen_font_t preview_font = load_offscreen_font(font_path, font_size);
    if(preview_font.font != NULL) {
        set_current_ttf_font(preview_font.font);
        lv_obj_set_style_text_font(lv_screen_active(), preview_font.font, 0);
    }

    lua_State * L = luaL_newstate();
    if(L == NULL) {
        set_current_ttf_font(previous_font);
        destroy_offscreen_font(&preview_font);
        free(draw_buf);
        lv_display_delete(disp);
        lv_display_set_default(previous_disp);
        set_error(error_buf, error_buf_size, "failed to create Lua state");
        return 5;
    }

    luaL_openlibs(L);
    lvgl_lua_register(L);

    /* Each preview render gets a fresh Lua state so widget scripts cannot leak
     * globals or mutable state into the next canvas item preview. */
    char * script_dir = dirname_dup(lua_widget_path);
    char * lua_root = lua_root_from_widget_path(lua_widget_path);
    setup_lua_path(L, script_dir, lua_root);

    int rc = 0;
    if(luaL_dofile(L, lua_widget_path) != LUA_OK) {
        set_error(error_buf, error_buf_size, lua_tostring(L, -1));
        rc = 6;
        goto cleanup;
    }
    if(!lua_istable(L, -1)) {
        set_error(error_buf, error_buf_size, "Lua widget module did not return a table");
        rc = 7;
        goto cleanup;
    }

    lua_getfield(L, -1, "new");
    if(!lua_isfunction(L, -1)) {
        set_error(error_buf, error_buf_size, "Lua widget module has no new(parent, state) function");
        rc = 8;
        goto cleanup;
    }

    /* The Lua widget contract is module.new(parent, state). The parent is the
     * active LVGL screen of this temporary offscreen display. */
    push_lv_obj(L, lv_screen_active());
    push_state_table(L, state_json, width, height);
    if(lua_pcall(L, 2, 1, 0) != LUA_OK) {
        set_error(error_buf, error_buf_size, lua_tostring(L, -1));
        rc = 9;
        goto cleanup;
    }

    /* Force layout and refresh immediately because there is no normal LVGL task
     * loop running in the designer paint path. */
    lv_obj_update_layout(lv_screen_active());
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(disp);

    if(lua_istable(L, -1)) {
        /* Prefer the widget's own cleanup hook when it provides one. The Lua
         * state and temporary LVGL display are still destroyed below either way. */
        lua_getfield(L, -1, "destroy");
        if(lua_isfunction(L, -1)) {
            lua_pushvalue(L, -2);
            if(lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
        }
        else {
            lua_pop(L, 1);
        }
    }

cleanup:
    /* Clean up in reverse order and restore LVGL's previous default display so
     * the caller does not observe this temporary preview display after return. */
    if(script_dir) free(script_dir);
    if(lua_root) free(lua_root);
    lua_close(L);
    free(draw_buf);
    lv_display_delete(disp);
    lv_display_set_default(previous_disp);
    set_current_ttf_font(previous_font);
    destroy_offscreen_font(&preview_font);
    return rc;
}
