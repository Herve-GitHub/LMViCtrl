/**
 * @file lvgl_lua_bindings.c
 * @brief LVGL Lua bindings implementation for VduEditor - Main module
 * 主模块：公共函数、helper函数、模块注册、常量定义、timer方法
 */

#include "lvgl_lua_bindings_internal.h"

static int luaopen_lvgl(lua_State* L);

// Global TTF font storage (for applying to objects)
static lv_font_t* g_current_ttf_font = NULL;

void set_current_ttf_font(lv_font_t* font)
{
    g_current_ttf_font = font;
}

lv_font_t* get_current_ttf_font(void)
{
    return g_current_ttf_font;
}

// ========== Helper functions ==========

// Helper: push lv_obj_t* as userdata with metatable
void push_lv_obj(lua_State* L, lv_obj_t* obj) {
    if (obj == NULL) {
        lua_pushnil(L);
        return;
    }
    lv_obj_t** ud = (lv_obj_t**)lua_newuserdata(L, sizeof(lv_obj_t*));
    *ud = obj;
    luaL_setmetatable(L, "lv_obj");
}

// Helper: get lv_obj_t* from userdata
lv_obj_t* check_lv_obj(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        lv_obj_t* obj = NULL;
        if (lua_islightuserdata(L, idx)) {
            obj = (lv_obj_t*)lua_touserdata(L, idx);
        } else {
            lv_obj_t** ud = (lv_obj_t**)lua_touserdata(L, idx);
            obj = ud ? *ud : NULL;
        }
        // Validate object is still valid before returning
        if (obj && !lv_obj_is_valid(obj)) {
            return NULL;
        }
        return obj;
    }
    return NULL;
}

// Helper: get lv_font_t* from userdata
lv_font_t* check_lv_font(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        lv_font_t** ud = (lv_font_t**)lua_touserdata(L, idx);
        return ud ? *ud : NULL;
    }
    return NULL;
}

// Helper: push lv_timer_t* as userdata with metatable
void push_lv_timer(lua_State* L, lv_timer_t* timer) {
    if (timer == NULL) {
        lua_pushnil(L);
        return;
    }
    lv_timer_t** ud = (lv_timer_t**)lua_newuserdata(L, sizeof(lv_timer_t*));
    *ud = timer;
    luaL_setmetatable(L, "lv_timer");
}

// Helper: get lv_timer_t* from userdata
lv_timer_t* check_lv_timer(lua_State* L, int idx) {
    if (lua_isuserdata(L, idx)) {
        if (lua_islightuserdata(L, idx)) {
            return (lv_timer_t*)lua_touserdata(L, idx);
        }
        lv_timer_t** ud = (lv_timer_t**)lua_touserdata(L, idx);
        return ud ? *ud : NULL;
    }
    return NULL;
}

// ========== Timer callback and methods ==========

// Timer callback function
static void lua_timer_cb(lv_timer_t* timer) {
    lua_timer_cb_data_t* cb_data = (lua_timer_cb_data_t*)lv_timer_get_user_data(timer);
    if (!cb_data || cb_data->func_ref == LUA_NOREF) return;
    
    lua_State* L = cb_data->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, cb_data->func_ref);
    
    // Push timer userdata as argument
    push_lv_timer(L, timer);
    
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        printf("Lua timer callback error: %s\n", err ? err : "unknown");
        lua_pop(L, 1);
    }
}

// timer:delete()
static int l_timer_delete(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    if (timer) {
        // Free the callback data
        lua_timer_cb_data_t* cb_data = (lua_timer_cb_data_t*)lv_timer_get_user_data(timer);
        if (cb_data) {
            if (cb_data->func_ref != LUA_NOREF) {
                luaL_unref(L, LUA_REGISTRYINDEX, cb_data->func_ref);
            }
            free(cb_data);
        }
        lv_timer_delete(timer);
    }
    lv_timer_t** ud = (lv_timer_t**)lua_touserdata(L, 1);
    if (ud) *ud = NULL;
    return 0;
}

// timer:pause()
static int l_timer_pause(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    if (timer) lv_timer_pause(timer);
    return 0;
}

// timer:resume()
static int l_timer_resume(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    if (timer) lv_timer_resume(timer);
    return 0;
}

// timer:set_period(period_ms)
static int l_timer_set_period(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    uint32_t period = (uint32_t)luaL_checkinteger(L, 2);
    if (timer) lv_timer_set_period(timer, period);
    return 0;
}

// timer:ready()
static int l_timer_ready(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    if (timer) lv_timer_ready(timer);
    return 0;
}

// timer:reset()
static int l_timer_reset(lua_State* L) {
    lv_timer_t* timer = check_lv_timer(L, 1);
    if (timer) lv_timer_reset(timer);
    return 0;
}

// ========== Module-level functions ==========

// lv.scr_act()
static int l_lv_scr_act(lua_State* L) {
    push_lv_obj(L, lv_screen_active());
    return 1;
}

// lv.screen_load(scr) / lv.scr_load(scr)
// 把指定对象切换为活动屏。runtime.lua 创建多屏后用此函数显示首屏。
static int l_lv_screen_load(lua_State* L) {
    lv_obj_t* scr = check_lv_obj(L, 1);
    if (scr) {
        lv_screen_load(scr);
    }
    return 0;
}

#if LV_USE_TINY_TTF
// 维护 font -> 文件缓冲映射，destroy 时一并释放。
// 直接 fopen 读取文件，避免 LVGL 内置 FS 驱动对 Windows 盘符 ("C:/...") 解析失败
// （lv_fs_open 把首字符当作 driver letter，遇 Windows 盘符报 unknown driver letter）。
typedef struct ttf_buf_entry_s {
    lv_font_t* font;
    void*      buf;
    struct ttf_buf_entry_s* next;
} ttf_buf_entry_t;
static ttf_buf_entry_t* g_ttf_buf_list = NULL;

static void ttf_buf_track(lv_font_t* font, void* buf) {
    ttf_buf_entry_t* e = (ttf_buf_entry_t*)malloc(sizeof(*e));
    if (!e) return;
    e->font = font; e->buf = buf;
    e->next = g_ttf_buf_list;
    g_ttf_buf_list = e;
}
static void ttf_buf_release(lv_font_t* font) {
    ttf_buf_entry_t** pp = &g_ttf_buf_list;
    while (*pp) {
        if ((*pp)->font == font) {
            ttf_buf_entry_t* dead = *pp;
            *pp = dead->next;
            free(dead->buf);
            free(dead);
            return;
        }
        pp = &(*pp)->next;
    }
}

// lv.tiny_ttf_create_file(path, font_size)
static int l_lv_tiny_ttf_create_file(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int32_t font_size = (int32_t)luaL_checkinteger(L, 2);

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        printf("Failed to open TTF font file: %s\n", path);
        lua_pushnil(L); return 1;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        printf("Empty TTF font file: %s\n", path);
        lua_pushnil(L); return 1;
    }
    void* buf = malloc((size_t)fsize);
    if (!buf) { fclose(fp); lua_pushnil(L); return 1; }
    if (fread(buf, 1, (size_t)fsize, fp) != (size_t)fsize) {
        fclose(fp); free(buf);
        printf("Failed to read TTF font file: %s\n", path);
        lua_pushnil(L); return 1;
    }
    fclose(fp);

    lv_font_t* font = lv_tiny_ttf_create_data_ex(
        buf, (size_t)fsize, font_size,
        LV_FONT_KERNING_NONE, LV_TINY_TTF_CACHE_GLYPH_CNT);
    if (!font) {
        free(buf);
        printf("lv_tiny_ttf_create_data_ex failed: %s\n", path);
        lua_pushnil(L); return 1;
    }
    ttf_buf_track(font, buf);

    lv_font_t** ud = (lv_font_t**)lua_newuserdata(L, sizeof(lv_font_t*));
    *ud = font;
    luaL_setmetatable(L, "lv_font");
    g_current_ttf_font = font;
    return 1;
}

// lv.tiny_ttf_destroy(font)
static int l_lv_tiny_ttf_destroy(lua_State* L) {
    lv_font_t* font = check_lv_font(L, 1);
    if (font) {
        lv_tiny_ttf_destroy(font);
        ttf_buf_release(font);
        if (g_current_ttf_font == font) {
            g_current_ttf_font = NULL;
        }
    }
    lv_font_t** ud = (lv_font_t**)lua_touserdata(L, 1);
    if (ud) *ud = NULL;
    return 0;
}

// lv.set_default_font(font)
static int l_lv_set_default_font(lua_State* L) {
    lv_font_t* font = check_lv_font(L, 1);
    if (font) {
        g_current_ttf_font = font;
    }
    return 0;
}
#endif

// lv.obj_create(parent)
static int l_lv_obj_create(lua_State* L) {
    push_lv_obj(L, lv_obj_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.label_create(parent)
static int l_lv_label_create(lua_State* L) {
    lv_obj_t* parent = check_lv_obj(L, 1);
    lv_obj_t* label = lv_label_create(parent);
    
    if (label) {
        lv_font_t* font_to_use = NULL;
#if LV_USE_TINY_TTF
        if (g_current_ttf_font) {
            font_to_use = g_current_ttf_font;
        } 
#endif
        if (font_to_use) {
            lv_obj_set_style_text_font(label, font_to_use, 0);
        }
    }
    
    push_lv_obj(L, label);
    return 1;
}

// lv.button_create(parent)
static int l_lv_button_create(lua_State* L) {
    push_lv_obj(L, lv_button_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.list_create(parent)
static int l_lv_list_create(lua_State* L) {
    push_lv_obj(L, lv_list_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.win_create(parent)
static int l_lv_win_create(lua_State* L) {
    push_lv_obj(L, lv_win_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.menu_create(parent)
static int l_lv_menu_create(lua_State* L) {
    push_lv_obj(L, lv_menu_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.tabview_create(parent)
static int l_lv_tabview_create(lua_State* L) {
    push_lv_obj(L, lv_tabview_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.textarea_create(parent)
static int l_lv_textarea_create(lua_State* L) {
    push_lv_obj(L, lv_textarea_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.checkbox_create(parent)
static int l_lv_checkbox_create(lua_State* L) {
    push_lv_obj(L, lv_checkbox_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.checkbox_set_text(obj, text)
static int l_lv_checkbox_set_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj && lv_obj_check_type(obj, &lv_checkbox_class)) {
        lv_checkbox_set_text(obj, text);
    }
    return 0;
}

// lv.dropdown_create(parent)
static int l_lv_dropdown_create(lua_State* L) {
    push_lv_obj(L, lv_dropdown_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.slider_create(parent)
static int l_lv_slider_create(lua_State* L) {
    push_lv_obj(L, lv_slider_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.chart_create(parent)
static int l_lv_chart_create(lua_State* L) {
    push_lv_obj(L, lv_chart_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.image_create(parent)
static int l_lv_image_create(lua_State* L) {
    push_lv_obj(L, lv_image_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.switch_create(parent)
static int l_lv_switch_create(lua_State* L) {
    push_lv_obj(L, lv_switch_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.bar_create(parent)
static int l_lv_bar_create(lua_State* L) {
    push_lv_obj(L, lv_bar_create(check_lv_obj(L, 1)));
    return 1;
}

// lv.arc_create(parent)
static int l_lv_arc_create(lua_State* L) {
    push_lv_obj(L, lv_arc_create(check_lv_obj(L, 1)));
    return 1;
}

// ========== LED ==========
// lv.led_create(parent)
static int l_lv_led_create(lua_State* L) {
    push_lv_obj(L, lv_led_create(check_lv_obj(L, 1)));
    return 1;
}

// led:set_color(hex)
static int l_led_set_color(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    uint32_t color_hex = (uint32_t)luaL_checkinteger(L, 2);
    if (obj && lv_obj_check_type(obj, &lv_led_class)) {
        lv_led_set_color(obj, lv_color_hex(color_hex));
    }
    return 0;
}

// ========== CHECKBOX methods ==========
static int l_checkbox_set_text(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o && lv_obj_check_type(o, &lv_checkbox_class)) {
        lv_checkbox_set_text(o, luaL_checkstring(L, 2));
    }
    return 0;
}

static int l_checkbox_get_text(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    const char* text = (o && lv_obj_check_type(o, &lv_checkbox_class)) ? lv_checkbox_get_text(o) : "";
    lua_pushstring(L, text ? text : "");
    return 1;
}

// led:set_brightness(b)  b: 0..255
static int l_led_set_brightness(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    int b = (int)luaL_checkinteger(L, 2);
    if (b < 0)   b = 0;
    if (b > 255) b = 255;
    if (obj && lv_obj_check_type(obj, &lv_led_class)) {
        lv_led_set_brightness(obj, (uint8_t)b);
    }
    return 0;
}

// led:led_on()
static int l_led_on(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_led_class)) {
        lv_led_on(obj);
    }
    return 0;
}

// led:led_off()
static int l_led_off(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_led_class)) {
        lv_led_off(obj);
    }
    return 0;
}

// led:led_toggle()
static int l_led_toggle(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_led_class)) {
        lv_led_toggle(obj);
    }
    return 0;
}

// LED methods table（在 luaopen_lvgl 中合并到 lv_obj 元表）
static const luaL_Reg lv_led_methods[] = {
    {"set_color",      l_led_set_color},
    {"set_brightness", l_led_set_brightness},
    {"led_on",         l_led_on},
    {"led_off",        l_led_off},
    {"led_toggle",     l_led_toggle},
    {NULL, NULL}
};

// ========== Roller / Table / Tileview create ==========
static int l_lv_roller_create(lua_State* L) {
    push_lv_obj(L, lv_roller_create(check_lv_obj(L, 1)));
    return 1;
}
static int l_lv_table_create(lua_State* L) {
    push_lv_obj(L, lv_table_create(check_lv_obj(L, 1)));
    return 1;
}
static int l_lv_tileview_create(lua_State* L) {
    push_lv_obj(L, lv_tileview_create(check_lv_obj(L, 1)));
    return 1;
}
// lv.tileview_add_tile(tv, col, row, dir)
static int l_lv_tileview_add_tile(lua_State* L) {
    lv_obj_t* tv = check_lv_obj(L, 1);
    int col = (int)luaL_checkinteger(L, 2);
    int row = (int)luaL_checkinteger(L, 3);
    int dir = (int)luaL_optinteger(L, 4, LV_DIR_ALL);
    push_lv_obj(L, tv ? lv_tileview_add_tile(tv, (uint8_t)col, (uint8_t)row, (lv_dir_t)dir) : NULL);
    return 1;
}

// ========== ARC methods ==========
static int l_arc_set_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_arc_set_value(o, (int32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_arc_set_range(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_arc_set_range(o, (int32_t)luaL_checkinteger(L, 2), (int32_t)luaL_checkinteger(L, 3));
    return 0;
}
static int l_arc_set_mode(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_arc_set_mode(o, (lv_arc_mode_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_arc_set_bg_angles(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_arc_set_bg_angles(o,
            (lv_value_precise_t)luaL_checkinteger(L, 2),
            (lv_value_precise_t)luaL_checkinteger(L, 3));
    return 0;
}
static int l_arc_set_rotation(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_arc_set_rotation(o, (int32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_arc_get_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    lua_pushinteger(L, o ? lv_arc_get_value(o) : 0);
    return 1;
}

// ========== BAR methods ==========
static int l_bar_set_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_bar_set_value(o,
            (int32_t)luaL_checkinteger(L, 2),
            (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    return 0;
}
static int l_bar_set_range(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_bar_set_range(o, (int32_t)luaL_checkinteger(L, 2), (int32_t)luaL_checkinteger(L, 3));
    return 0;
}
static int l_bar_set_mode(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_bar_set_mode(o, (lv_bar_mode_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_bar_set_start_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_bar_set_start_value(o,
            (int32_t)luaL_checkinteger(L, 2),
            (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    return 0;
}
static int l_bar_set_orientation(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_bar_set_orientation(o, (lv_bar_orientation_t)luaL_checkinteger(L, 2));
    return 0;
}

// ========== DROPDOWN methods ==========
static int l_dropdown_set_options(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_dropdown_set_options(o, luaL_checkstring(L, 2));
    return 0;
}
static int l_dropdown_set_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_dropdown_set_selected(o, (uint32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_dropdown_set_dir(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_dropdown_set_dir(o, (lv_dir_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_dropdown_get_list(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    push_lv_obj(L, o ? lv_dropdown_get_list(o) : NULL);
    return 1;
}
static int l_dropdown_get_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    lua_pushinteger(L, o ? lv_dropdown_get_selected(o) : 0);
    return 1;
}

// ========== LABEL methods ==========
static int l_label_set_long_mode(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_label_set_long_mode(o, (lv_label_long_mode_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_label_set_recolor(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_label_set_recolor(o, lua_toboolean(L, 2));
    return 0;
}

// ========== LIST methods ==========
static int l_list_clean(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_obj_clean(o);
    return 0;
}
// list:add_text(txt) -> obj
static int l_list_add_text(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    push_lv_obj(L, o ? lv_list_add_text(o, luaL_checkstring(L, 2)) : NULL);
    return 1;
}
// list:add_btn(icon, text) -> obj  (icon 接受 nil 或字符串)
static int l_list_add_btn(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    const char* icon = lua_isnoneornil(L, 2) ? NULL : luaL_checkstring(L, 2);
    const char* txt  = lua_isnoneornil(L, 3) ? NULL : luaL_checkstring(L, 3);
    push_lv_obj(L, o ? lv_list_add_button(o, icon, txt) : NULL);
    return 1;
}

// ========== MENU methods ==========
static int l_menu_set_page(lua_State* L) {
    lv_obj_t* menu = check_lv_obj(L, 1);
    lv_obj_t* page = lua_isnoneornil(L, 2) ? NULL : check_lv_obj(L, 2);
    if (menu) lv_menu_set_page(menu, page);
    return 0;
}
static int l_menu_set_load_page_event(lua_State* L) {
    lv_obj_t* menu = check_lv_obj(L, 1);
    lv_obj_t* obj  = check_lv_obj(L, 2);
    lv_obj_t* page = check_lv_obj(L, 3);
    if (menu && obj && page) lv_menu_set_load_page_event(menu, obj, page);
    return 0;
}
// 同时把 menu_page_create / menu_cont_create 暴露为模块函数
static int l_lv_menu_page_create(lua_State* L) {
    lv_obj_t* menu = check_lv_obj(L, 1);
    const char* title = lua_isnoneornil(L, 2) ? NULL : luaL_checkstring(L, 2);
    push_lv_obj(L, menu ? lv_menu_page_create(menu, title) : NULL);
    return 1;
}
static int l_lv_menu_cont_create(lua_State* L) {
    push_lv_obj(L, lv_menu_cont_create(check_lv_obj(L, 1)));
    return 1;
}

// ========== ROLLER methods ==========
static int l_roller_set_options(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_roller_set_options(o,
            luaL_checkstring(L, 2),
            (lv_roller_mode_t)luaL_optinteger(L, 3, LV_ROLLER_MODE_NORMAL));
    return 0;
}
static int l_roller_set_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_roller_set_selected(o,
            (uint32_t)luaL_checkinteger(L, 2),
            (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    return 0;
}
static int l_roller_set_visible_row_count(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_roller_set_visible_row_count(o, (uint32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_roller_get_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    lua_pushinteger(L, o ? lv_roller_get_selected(o) : 0);
    return 1;
}

// ========== SLIDER 补：set_left_value ==========
static int l_slider_set_left_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_slider_set_left_value(o,
            (int32_t)luaL_checkinteger(L, 2),
            (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    return 0;
}

// ========== TABLE methods (v9 名映射 v8 别名) ==========
static int l_table_set_col_cnt(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_table_set_column_count(o, (uint32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_table_set_row_cnt(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_table_set_row_count(o, (uint32_t)luaL_checkinteger(L, 2));
    return 0;
}
static int l_table_set_col_width(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_table_set_column_width(o,
            (uint32_t)luaL_checkinteger(L, 2),
            (int32_t)luaL_checkinteger(L, 3));
    return 0;
}
static int l_table_set_cell_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_table_set_cell_value(o,
            (uint32_t)luaL_checkinteger(L, 2),
            (uint32_t)luaL_checkinteger(L, 3),
            luaL_checkstring(L, 4));
    return 0;
}

// ========== TABVIEW methods (v9 名映射 v8 别名) ==========
static int l_tabview_add_tab(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    push_lv_obj(L, o ? lv_tabview_add_tab(o, luaL_checkstring(L, 2)) : NULL);
    return 1;
}
static int l_tabview_set_act(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_tabview_set_active(o,
            (uint32_t)luaL_checkinteger(L, 2),
            (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    return 0;
}
static int l_tabview_get_tab_act(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    lua_pushinteger(L, o ? lv_tabview_get_tab_active(o) : 0);
    return 1;
}

// ========== TILEVIEW method ==========
static int l_tileview_set_tile_id(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (o) lv_tileview_set_tile_by_index(o,
            (uint32_t)luaL_checkinteger(L, 2),
            (uint32_t)luaL_checkinteger(L, 3),
            (lv_anim_enable_t)luaL_optinteger(L, 4, LV_ANIM_OFF));
    return 0;
}

// 把上述 widget-specific 方法合并进 lv_obj 元表
// 共享同一个 lv_obj userdata 元表。无冲突的方法直接合入；
// 冲突的方法（set_value/set_range/set_mode/set_options/set_selected/get_value/get_selected）
// 通过 lv_widget_dispatch_methods 用对象类型分派。
static const luaL_Reg lv_widget_extra_methods[] = {
    // arc 独有
    {"set_bg_angles",          l_arc_set_bg_angles},
    {"set_rotation",           l_arc_set_rotation},
    // bar 独有
    {"set_start_value",        l_bar_set_start_value},
    {"set_orientation",        l_bar_set_orientation},
    // dropdown 独有
    {"set_dir",                l_dropdown_set_dir},
    {"get_list",               l_dropdown_get_list},
    // label
    {"set_long_mode",          l_label_set_long_mode},
    {"set_recolor",            l_label_set_recolor},
    // checkbox
    {"set_checkbox_text",      l_checkbox_set_text},
    {"get_checkbox_text",      l_checkbox_get_text},
    // list
    {"clean",                  l_list_clean},
    {"add_text",               l_list_add_text},
    {"add_btn",                l_list_add_btn},
    // menu
    {"set_page",               l_menu_set_page},
    {"set_load_page_event",    l_menu_set_load_page_event},
    // roller 独有
    {"set_visible_row_count",  l_roller_set_visible_row_count},
    // slider 补
    {"set_left_value",         l_slider_set_left_value},
    // table (v9 名字映射 v8 别名)
    {"set_col_cnt",            l_table_set_col_cnt},
    {"set_row_cnt",            l_table_set_row_cnt},
    {"set_col_width",          l_table_set_col_width},
    {"set_cell_value",         l_table_set_cell_value},
    // tabview (v9 名字映射 v8 别名)
    {"add_tab",                l_tabview_add_tab},
    {"set_act",                l_tabview_set_act},
    {"get_tab_act",            l_tabview_get_tab_act},
    // tileview
    {"set_tile_id",            l_tileview_set_tile_id},
    {NULL, NULL}
};

// arc/bar/roller 的 set_value/set_range/set_mode 等同名冲突，
// 单独再注册：把 arc 与 bar 的差异化方法用前缀化别名导出，widget Lua 仍然
// 用 :set_range 等无前缀名，所以按"先 arc 再 bar 再 roller"的顺序合并，
// 让最常被调用的覆盖前者；若同名按需进入元表。
// arc/bar/slider 共享 lv_obj 元表，set_value/set_range/set_mode 同名冲突。
// 用统一分派函数按 lv_obj_check_type 选择正确实现。
static int l_dispatch_set_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (!o) return 0;
    if (lv_obj_check_type(o, &lv_arc_class)) {
        lv_arc_set_value(o, (int32_t)luaL_checkinteger(L, 2));
    } else if (lv_obj_check_type(o, &lv_bar_class)) {
        lv_bar_set_value(o,
                (int32_t)luaL_checkinteger(L, 2),
                (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    } else if (lv_obj_check_type(o, &lv_slider_class)) {
        lv_slider_set_value(o,
                (int32_t)luaL_checkinteger(L, 2),
                (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    }
    return 0;
}
static int l_dispatch_set_range(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (!o) return 0;
    int32_t a = (int32_t)luaL_checkinteger(L, 2);
    int32_t b = (int32_t)luaL_checkinteger(L, 3);
    if (lv_obj_check_type(o, &lv_arc_class))         lv_arc_set_range(o, a, b);
    else if (lv_obj_check_type(o, &lv_bar_class))    lv_bar_set_range(o, a, b);
    else if (lv_obj_check_type(o, &lv_slider_class)) lv_slider_set_range(o, a, b);
    return 0;
}
static int l_dispatch_set_mode(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (!o) return 0;
    int m = (int)luaL_checkinteger(L, 2);
    if (lv_obj_check_type(o, &lv_arc_class))         lv_arc_set_mode(o, (lv_arc_mode_t)m);
    else if (lv_obj_check_type(o, &lv_bar_class))    lv_bar_set_mode(o, (lv_bar_mode_t)m);
    else if (lv_obj_check_type(o, &lv_slider_class)) lv_slider_set_mode(o, (lv_slider_mode_t)m);
    return 0;
}
static int l_dispatch_get_value(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    int32_t v = 0;
    if (o) {
        if (lv_obj_check_type(o, &lv_arc_class))         v = lv_arc_get_value(o);
        else if (lv_obj_check_type(o, &lv_bar_class))    v = lv_bar_get_value(o);
        else if (lv_obj_check_type(o, &lv_slider_class)) v = lv_slider_get_value(o);
    }
    lua_pushinteger(L, v);
    return 1;
}
// dropdown / roller 同名 set_options / set_selected：用 dispatch
static int l_dispatch_set_options(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (!o) return 0;
    if (lv_obj_check_type(o, &lv_dropdown_class)) {
        lv_dropdown_set_options(o, luaL_checkstring(L, 2));
    } else if (lv_obj_check_type(o, &lv_roller_class)) {
        lv_roller_set_options(o,
                luaL_checkstring(L, 2),
                (lv_roller_mode_t)luaL_optinteger(L, 3, LV_ROLLER_MODE_NORMAL));
    }
    return 0;
}
static int l_dispatch_set_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    if (!o) return 0;
    if (lv_obj_check_type(o, &lv_dropdown_class)) {
        lv_dropdown_set_selected(o, (uint32_t)luaL_checkinteger(L, 2));
    } else if (lv_obj_check_type(o, &lv_roller_class)) {
        lv_roller_set_selected(o,
                (uint32_t)luaL_checkinteger(L, 2),
                (lv_anim_enable_t)luaL_optinteger(L, 3, LV_ANIM_OFF));
    }
    return 0;
}
static int l_dispatch_get_selected(lua_State* L) {
    lv_obj_t* o = check_lv_obj(L, 1);
    uint32_t v = 0;
    if (o) {
        if (lv_obj_check_type(o, &lv_dropdown_class)) v = lv_dropdown_get_selected(o);
        else if (lv_obj_check_type(o, &lv_roller_class)) v = lv_roller_get_selected(o);
    }
    lua_pushinteger(L, v);
    return 1;
}

static const luaL_Reg lv_widget_dispatch_methods[] = {
    {"set_value",    l_dispatch_set_value},
    {"set_range",    l_dispatch_set_range},
    {"set_mode",     l_dispatch_set_mode},
    {"get_value",    l_dispatch_get_value},
    {"set_options",  l_dispatch_set_options},
    {"set_selected", l_dispatch_set_selected},
    {"get_selected", l_dispatch_get_selected},
    {NULL, NULL}
};

// lv.display_get_hor_res()
static int l_lv_display_get_hor_res(lua_State* L) {
    lua_pushinteger(L, lv_display_get_horizontal_resolution(lv_display_get_default()));
    return 1;
}

// lv.display_get_ver_res()
static int l_lv_display_get_ver_res(lua_State* L) {
    lua_pushinteger(L, lv_display_get_vertical_resolution(lv_display_get_default()));
    return 1;
}

// lv.get_mouse_x()
static int l_lv_get_mouse_x(lua_State* L) {
    lv_indev_t* indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            lua_pushinteger(L, point.x);
            return 1;
        }
        indev = lv_indev_get_next(indev);
    }
    lua_pushinteger(L, 0);
    return 1;
}

// lv.get_mouse_y()
static int l_lv_get_mouse_y(lua_State* L) {
    lv_indev_t* indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_point_t point;
            lv_indev_get_point(indev, &point);
            lua_pushinteger(L, point.y);
            return 1;
        }
        indev = lv_indev_get_next(indev);
    }
    lua_pushinteger(L, 0);
    return 1;
}

// lv.pct(value)
static int l_lv_pct(lua_State* L) {
    int32_t value = (int32_t)luaL_checkinteger(L, 1);
    lua_pushinteger(L, LV_PCT(value));
    return 1;
}

// lv.timer_create(callback, period_ms)
static int l_lv_timer_create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    uint32_t period = (uint32_t)luaL_checkinteger(L, 2);
    
    lua_timer_cb_data_t* cb_data = (lua_timer_cb_data_t*)malloc(sizeof(lua_timer_cb_data_t));
    if (!cb_data) {
        lua_pushnil(L);
        return 1;
    }
    
    cb_data->L = L;
    lua_pushvalue(L, 1);
    cb_data->func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    
    lv_timer_t* timer = lv_timer_create(lua_timer_cb, period, cb_data);
    if (!timer) {
        luaL_unref(L, LUA_REGISTRYINDEX, cb_data->func_ref);
        free(cb_data);
        lua_pushnil(L);
        return 1;
    }
    
    cb_data->timer = timer;
    push_lv_timer(L, timer);
    return 1;
}

// lv.timer_delete(timer)
static int l_lv_timer_delete(lua_State* L) {
    return l_timer_delete(L);
}

// External declaration for textarea module function
extern int l_lv_textarea_get_text(lua_State* L);

// ========== Module Functions Table ==========
static const luaL_Reg lvgl_funcs[] = {
    {"scr_act", l_lv_scr_act},
    {"screen_active", l_lv_scr_act},
    {"screen_load", l_lv_screen_load},
    {"scr_load", l_lv_screen_load},
    {"obj_create", l_lv_obj_create},
    {"label_create", l_lv_label_create},
    {"button_create", l_lv_button_create},
    {"btn_create", l_lv_button_create},
    {"list_create", l_lv_list_create},
    {"win_create", l_lv_win_create},
    {"menu_create", l_lv_menu_create},
    {"tabview_create", l_lv_tabview_create},
    {"textarea_create", l_lv_textarea_create},
    {"textarea_get_text", l_lv_textarea_get_text},
    {"checkbox_create", l_lv_checkbox_create},
    {"checkbox_set_text", l_lv_checkbox_set_text},
    {"dropdown_create", l_lv_dropdown_create},
    {"slider_create", l_lv_slider_create},
    {"chart_create", l_lv_chart_create},
    {"image_create", l_lv_image_create},
    {"switch_create", l_lv_switch_create},
    {"bar_create", l_lv_bar_create},
    {"arc_create", l_lv_arc_create},
    {"led_create", l_lv_led_create},
    {"roller_create", l_lv_roller_create},
    {"table_create", l_lv_table_create},
    {"tileview_create", l_lv_tileview_create},
    {"tileview_add_tile", l_lv_tileview_add_tile},
    {"menu_page_create", l_lv_menu_page_create},
    {"menu_cont_create", l_lv_menu_cont_create},
    {"display_get_hor_res", l_lv_display_get_hor_res},
    {"display_get_ver_res", l_lv_display_get_ver_res},
    {"get_mouse_x", l_lv_get_mouse_x},
    {"get_mouse_y", l_lv_get_mouse_y},
    {"pct", l_lv_pct},
#if LV_USE_TINY_TTF
    {"tiny_ttf_create_file", l_lv_tiny_ttf_create_file},
    {"tiny_ttf_destroy", l_lv_tiny_ttf_destroy},
    {"set_default_font", l_lv_set_default_font},
#endif
    {"timer_create", l_lv_timer_create},
    {"timer_delete", l_lv_timer_delete},
    {NULL, NULL}
};

// Timer methods table
static const luaL_Reg lv_timer_methods[] = {
    {"delete", l_timer_delete},
    {"pause", l_timer_pause},
    {"resume", l_timer_resume},
    {"set_period", l_timer_set_period},
    {"ready", l_timer_ready},
    {"reset", l_timer_reset},
    {NULL, NULL}
};

// Helper to merge method tables
static void merge_methods_to_table(lua_State* L, const luaL_Reg* methods) {
    if (!methods) return;
    for (const luaL_Reg* m = methods; m->name != NULL; m++) {
        lua_pushcfunction(L, m->func);
        lua_setfield(L, -2, m->name);
    }
}

// Module loader function
static int luaopen_lvgl(lua_State* L) {
    // Create lv_obj metatable with merged methods
    luaL_newmetatable(L, "lv_obj");
    lua_newtable(L);  // Create __index table
    
    // Add obj methods
    merge_methods_to_table(L, lvgl_get_obj_methods());
    // Add textarea methods
    merge_methods_to_table(L, lvgl_get_textarea_methods());
    // Add chart methods
    merge_methods_to_table(L, lvgl_get_chart_methods());
    // Add slider methods
    merge_methods_to_table(L, lvgl_get_slider_methods());
    // Add LED methods
    merge_methods_to_table(L, lv_led_methods);
    // Add widget extra methods（dropdown/label/list/menu/slider/table/tabview/tileview）
    merge_methods_to_table(L, lv_widget_extra_methods);
    // Add type-dispatched methods (set_value/set_range/set_mode/set_options/...)
    merge_methods_to_table(L, lv_widget_dispatch_methods);
    
   
    
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    
    // Create lv_font metatable
    luaL_newmetatable(L, "lv_font");
    lua_pop(L, 1);
    
    // Create lv_timer metatable
    luaL_newmetatable(L, "lv_timer");
    lua_newtable(L);
    merge_methods_to_table(L, lv_timer_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    
    // Create module table
    luaL_newlib(L, lvgl_funcs);
    
    // Add clipboard functions

    merge_methods_to_table(L, lvgl_get_mongoose_methods());
    
    // Add constants - Alignment
    lua_pushinteger(L, LV_ALIGN_DEFAULT); lua_setfield(L, -2, "ALIGN_DEFAULT");
    lua_pushinteger(L, LV_ALIGN_TOP_LEFT); lua_setfield(L, -2, "ALIGN_TOP_LEFT");
    lua_pushinteger(L, LV_ALIGN_TOP_MID); lua_setfield(L, -2, "ALIGN_TOP_MID");
    lua_pushinteger(L, LV_ALIGN_TOP_RIGHT); lua_setfield(L, -2, "ALIGN_TOP_RIGHT");
    lua_pushinteger(L, LV_ALIGN_BOTTOM_LEFT); lua_setfield(L, -2, "ALIGN_BOTTOM_LEFT");
    lua_pushinteger(L, LV_ALIGN_BOTTOM_MID); lua_setfield(L, -2, "ALIGN_BOTTOM_MID");
    lua_pushinteger(L, LV_ALIGN_BOTTOM_RIGHT); lua_setfield(L, -2, "ALIGN_BOTTOM_RIGHT");
    lua_pushinteger(L, LV_ALIGN_LEFT_MID); lua_setfield(L, -2, "ALIGN_LEFT_MID");
    lua_pushinteger(L, LV_ALIGN_RIGHT_MID); lua_setfield(L, -2, "ALIGN_RIGHT_MID");
    lua_pushinteger(L, LV_ALIGN_CENTER); lua_setfield(L, -2, "ALIGN_CENTER");
    
    // Align out constants
    lua_pushinteger(L, LV_ALIGN_OUT_TOP_LEFT); lua_setfield(L, -2, "ALIGN_OUT_TOP_LEFT");
    lua_pushinteger(L, LV_ALIGN_OUT_TOP_MID); lua_setfield(L, -2, "ALIGN_OUT_TOP_MID");
    lua_pushinteger(L, LV_ALIGN_OUT_TOP_RIGHT); lua_setfield(L, -2, "ALIGN_OUT_TOP_RIGHT");
    lua_pushinteger(L, LV_ALIGN_OUT_BOTTOM_LEFT); lua_setfield(L, -2, "ALIGN_OUT_BOTTOM_LEFT");
    lua_pushinteger(L, LV_ALIGN_OUT_BOTTOM_MID); lua_setfield(L, -2, "ALIGN_OUT_BOTTOM_MID");
    lua_pushinteger(L, LV_ALIGN_OUT_BOTTOM_RIGHT); lua_setfield(L, -2, "ALIGN_OUT_BOTTOM_RIGHT");
    lua_pushinteger(L, LV_ALIGN_OUT_LEFT_TOP); lua_setfield(L, -2, "ALIGN_OUT_LEFT_TOP");
    lua_pushinteger(L, LV_ALIGN_OUT_LEFT_MID); lua_setfield(L, -2, "ALIGN_OUT_LEFT_MID");
    lua_pushinteger(L, LV_ALIGN_OUT_LEFT_BOTTOM); lua_setfield(L, -2, "ALIGN_OUT_LEFT_BOTTOM");
    lua_pushinteger(L, LV_ALIGN_OUT_RIGHT_TOP); lua_setfield(L, -2, "ALIGN_OUT_RIGHT_TOP");
    lua_pushinteger(L, LV_ALIGN_OUT_RIGHT_MID); lua_setfield(L, -2, "ALIGN_OUT_RIGHT_MID");
    lua_pushinteger(L, LV_ALIGN_OUT_RIGHT_BOTTOM); lua_setfield(L, -2, "ALIGN_OUT_RIGHT_BOTTOM");
    
    // Flex constants
    lua_pushinteger(L, LV_FLEX_FLOW_ROW); lua_setfield(L, -2, "FLEX_FLOW_ROW");
    lua_pushinteger(L, LV_FLEX_FLOW_COLUMN); lua_setfield(L, -2, "FLEX_FLOW_COLUMN");
    lua_pushinteger(L, LV_FLEX_FLOW_ROW_WRAP); lua_setfield(L, -2, "FLEX_FLOW_ROW_WRAP");
    lua_pushinteger(L, LV_FLEX_FLOW_COLUMN_WRAP); lua_setfield(L, -2, "FLEX_FLOW_COLUMN_WRAP");
    lua_pushinteger(L, LV_FLEX_ALIGN_START); lua_setfield(L, -2, "FLEX_ALIGN_START");
    lua_pushinteger(L, LV_FLEX_ALIGN_END); lua_setfield(L, -2, "FLEX_ALIGN_END");
    lua_pushinteger(L, LV_FLEX_ALIGN_CENTER); lua_setfield(L, -2, "FLEX_ALIGN_CENTER");
    lua_pushinteger(L, LV_FLEX_ALIGN_SPACE_EVENLY); lua_setfield(L, -2, "FLEX_ALIGN_SPACE_EVENLY");
    lua_pushinteger(L, LV_FLEX_ALIGN_SPACE_AROUND); lua_setfield(L, -2, "FLEX_ALIGN_SPACE_AROUND");
    lua_pushinteger(L, LV_FLEX_ALIGN_SPACE_BETWEEN); lua_setfield(L, -2, "FLEX_ALIGN_SPACE_BETWEEN");
    
    // Flag constants
    lua_pushinteger(L, LV_OBJ_FLAG_HIDDEN); lua_setfield(L, -2, "OBJ_FLAG_HIDDEN");
    lua_pushinteger(L, LV_OBJ_FLAG_CLICKABLE); lua_setfield(L, -2, "OBJ_FLAG_CLICKABLE");
    lua_pushinteger(L, LV_OBJ_FLAG_SCROLLABLE); lua_setfield(L, -2, "OBJ_FLAG_SCROLLABLE");
    lua_pushinteger(L, LV_OBJ_FLAG_CHECKABLE); lua_setfield(L, -2, "OBJ_FLAG_CHECKABLE");
    lua_pushinteger(L, LV_OBJ_FLAG_SCROLL_ON_FOCUS); lua_setfield(L, -2, "OBJ_FLAG_SCROLL_ON_FOCUS");
    lua_pushinteger(L, LV_OBJ_FLAG_GESTURE_BUBBLE); lua_setfield(L, -2, "OBJ_FLAG_GESTURE_BUBBLE");
    lua_pushinteger(L, LV_OBJ_FLAG_PRESS_LOCK); lua_setfield(L, -2, "OBJ_FLAG_PRESS_LOCK");
    lua_pushinteger(L, LV_OBJ_FLAG_EVENT_BUBBLE); lua_setfield(L, -2, "OBJ_FLAG_EVENT_BUBBLE");
    
    // State constants
    lua_pushinteger(L, LV_STATE_DEFAULT); lua_setfield(L, -2, "STATE_DEFAULT");
    lua_pushinteger(L, LV_STATE_CHECKED); lua_setfield(L, -2, "STATE_CHECKED");
    lua_pushinteger(L, LV_STATE_FOCUSED); lua_setfield(L, -2, "STATE_FOCUSED");
    lua_pushinteger(L, LV_STATE_FOCUS_KEY); lua_setfield(L, -2, "STATE_FOCUS_KEY");
    lua_pushinteger(L, LV_STATE_EDITED); lua_setfield(L, -2, "STATE_EDITED");
    lua_pushinteger(L, LV_STATE_HOVERED); lua_setfield(L, -2, "STATE_HOVERED");
    lua_pushinteger(L, LV_STATE_PRESSED); lua_setfield(L, -2, "STATE_PRESSED");
    lua_pushinteger(L, LV_STATE_SCROLLED); lua_setfield(L, -2, "STATE_SCROLLED");
    lua_pushinteger(L, LV_STATE_DISABLED); lua_setfield(L, -2, "STATE_DISABLED");
    
    // Event constants
    lua_pushinteger(L, LV_EVENT_PRESSED); lua_setfield(L, -2, "EVENT_PRESSED");
    lua_pushinteger(L, LV_EVENT_PRESSING); lua_setfield(L, -2, "EVENT_PRESSING");
    lua_pushinteger(L, LV_EVENT_RELEASED); lua_setfield(L, -2, "EVENT_RELEASED");
    lua_pushinteger(L, LV_EVENT_CLICKED); lua_setfield(L, -2, "EVENT_CLICKED");
    lua_pushinteger(L, LV_EVENT_SHORT_CLICKED); lua_setfield(L, -2, "EVENT_SHORT_CLICKED");
    lua_pushinteger(L, LV_EVENT_LONG_PRESSED); lua_setfield(L, -2, "EVENT_LONG_PRESSED");
    lua_pushinteger(L, LV_EVENT_LONG_PRESSED_REPEAT); lua_setfield(L, -2, "EVENT_LONG_PRESSED_REPEAT");
    lua_pushinteger(L, LV_EVENT_SINGLE_CLICKED); lua_setfield(L, -2, "EVENT_SINGLE_CLICKED");
    lua_pushinteger(L, LV_EVENT_DOUBLE_CLICKED); lua_setfield(L, -2, "EVENT_DOUBLE_CLICKED");
    lua_pushinteger(L, LV_EVENT_VALUE_CHANGED); lua_setfield(L, -2, "EVENT_VALUE_CHANGED");
    lua_pushinteger(L, LV_EVENT_FOCUSED); lua_setfield(L, -2, "EVENT_FOCUSED");
    lua_pushinteger(L, LV_EVENT_DEFOCUSED); lua_setfield(L, -2, "EVENT_DEFOCUSED");
    lua_pushinteger(L, LV_EVENT_READY); lua_setfield(L, -2, "EVENT_READY");
    lua_pushinteger(L, LV_EVENT_CANCEL); lua_setfield(L, -2, "EVENT_CANCEL");
    lua_pushinteger(L, LV_EVENT_KEY); lua_setfield(L, -2, "EVENT_KEY");
    lua_pushinteger(L, LV_EVENT_INSERT); lua_setfield(L, -2, "EVENT_INSERT");
    lua_pushinteger(L, LV_EVENT_REFRESH); lua_setfield(L, -2, "EVENT_REFRESH");
    lua_pushinteger(L, LV_EVENT_DELETE); lua_setfield(L, -2, "EVENT_DELETE");
    
    // Opacity constants
    lua_pushinteger(L, LV_OPA_TRANSP); lua_setfield(L, -2, "OPA_TRANSP");
    lua_pushinteger(L, LV_OPA_COVER); lua_setfield(L, -2, "OPA_COVER");
    lua_pushinteger(L, LV_OPA_50); lua_setfield(L, -2, "OPA_50");
    
    // Size constants
    lua_pushinteger(L, LV_SIZE_CONTENT); lua_setfield(L, -2, "SIZE_CONTENT");
    lua_pushinteger(L, LV_PCT(100)); lua_setfield(L, -2, "PCT_100");
    lua_pushinteger(L, LV_PCT(50)); lua_setfield(L, -2, "PCT_50");
    
    // Chart constants
    lua_pushinteger(L, LV_CHART_TYPE_NONE); lua_setfield(L, -2, "CHART_TYPE_NONE");
    lua_pushinteger(L, LV_CHART_TYPE_LINE); lua_setfield(L, -2, "CHART_TYPE_LINE");
    lua_pushinteger(L, LV_CHART_TYPE_BAR); lua_setfield(L, -2, "CHART_TYPE_BAR");
    lua_pushinteger(L, LV_CHART_TYPE_SCATTER); lua_setfield(L, -2, "CHART_TYPE_SCATTER");
    lua_pushinteger(L, LV_CHART_UPDATE_MODE_SHIFT); lua_setfield(L, -2, "CHART_UPDATE_MODE_SHIFT");
    lua_pushinteger(L, LV_CHART_UPDATE_MODE_CIRCULAR); lua_setfield(L, -2, "CHART_UPDATE_MODE_CIRCULAR");
    lua_pushinteger(L, LV_CHART_AXIS_PRIMARY_Y); lua_setfield(L, -2, "CHART_AXIS_PRIMARY_Y");
    lua_pushinteger(L, LV_CHART_AXIS_SECONDARY_Y); lua_setfield(L, -2, "CHART_AXIS_SECONDARY_Y");
    lua_pushinteger(L, LV_CHART_AXIS_PRIMARY_X); lua_setfield(L, -2, "CHART_AXIS_PRIMARY_X");
    lua_pushinteger(L, LV_CHART_AXIS_SECONDARY_X); lua_setfield(L, -2, "CHART_AXIS_SECONDARY_X");
    
    // Animation constants
    lua_pushinteger(L, LV_ANIM_OFF); lua_setfield(L, -2, "ANIM_OFF");
    lua_pushinteger(L, LV_ANIM_ON); lua_setfield(L, -2, "ANIM_ON");
    
    // Slider mode constants
    lua_pushinteger(L, LV_SLIDER_MODE_NORMAL); lua_setfield(L, -2, "SLIDER_MODE_NORMAL");
    lua_pushinteger(L, LV_SLIDER_MODE_SYMMETRICAL); lua_setfield(L, -2, "SLIDER_MODE_SYMMETRICAL");
    lua_pushinteger(L, LV_SLIDER_MODE_RANGE); lua_setfield(L, -2, "SLIDER_MODE_RANGE");
    
    // Part constants (for styling)
    lua_pushinteger(L, LV_PART_MAIN); lua_setfield(L, -2, "PART_MAIN");
    lua_pushinteger(L, LV_PART_INDICATOR); lua_setfield(L, -2, "PART_INDICATOR");
    lua_pushinteger(L, LV_PART_KNOB); lua_setfield(L, -2, "PART_KNOB");
    
    // Border side constants
    lua_pushinteger(L, LV_BORDER_SIDE_NONE); lua_setfield(L, -2, "BORDER_SIDE_NONE");
    lua_pushinteger(L, LV_BORDER_SIDE_BOTTOM); lua_setfield(L, -2, "BORDER_SIDE_BOTTOM");
    lua_pushinteger(L, LV_BORDER_SIDE_TOP); lua_setfield(L, -2, "BORDER_SIDE_TOP");
    lua_pushinteger(L, LV_BORDER_SIDE_LEFT); lua_setfield(L, -2, "BORDER_SIDE_LEFT");
    lua_pushinteger(L, LV_BORDER_SIDE_RIGHT); lua_setfield(L, -2, "BORDER_SIDE_RIGHT");
    lua_pushinteger(L, LV_BORDER_SIDE_FULL); lua_setfield(L, -2, "BORDER_SIDE_FULL");
    lua_pushinteger(L, LV_BORDER_SIDE_INTERNAL); lua_setfield(L, -2, "BORDER_SIDE_INTERNAL");
    
    // Other constants
    lua_pushinteger(L, LV_RADIUS_CIRCLE); lua_setfield(L, -2, "RADIUS_CIRCLE");
    lua_pushinteger(L, LV_TEXT_ALIGN_LEFT); lua_setfield(L, -2, "TEXT_ALIGN_LEFT");
    lua_pushinteger(L, LV_TEXT_ALIGN_CENTER); lua_setfield(L, -2, "TEXT_ALIGN_CENTER");
    lua_pushinteger(L, LV_TEXT_ALIGN_RIGHT); lua_setfield(L, -2, "TEXT_ALIGN_RIGHT");
    lua_pushinteger(L, LV_TEXT_ALIGN_AUTO); lua_setfield(L, -2, "TEXT_ALIGN_AUTO");
    
    // Image align constants (for image scaling/fitting)
    lua_pushinteger(L, LV_IMAGE_ALIGN_DEFAULT); lua_setfield(L, -2, "IMAGE_ALIGN_DEFAULT");
    lua_pushinteger(L, LV_IMAGE_ALIGN_TOP_LEFT); lua_setfield(L, -2, "IMAGE_ALIGN_TOP_LEFT");
    lua_pushinteger(L, LV_IMAGE_ALIGN_TOP_MID); lua_setfield(L, -2, "IMAGE_ALIGN_TOP_MID");
    lua_pushinteger(L, LV_IMAGE_ALIGN_TOP_RIGHT); lua_setfield(L, -2, "IMAGE_ALIGN_TOP_RIGHT");
    lua_pushinteger(L, LV_IMAGE_ALIGN_BOTTOM_LEFT); lua_setfield(L, -2, "IMAGE_ALIGN_BOTTOM_LEFT");
    lua_pushinteger(L, LV_IMAGE_ALIGN_BOTTOM_MID); lua_setfield(L, -2, "IMAGE_ALIGN_BOTTOM_MID");
    lua_pushinteger(L, LV_IMAGE_ALIGN_BOTTOM_RIGHT); lua_setfield(L, -2, "IMAGE_ALIGN_BOTTOM_RIGHT");
    lua_pushinteger(L, LV_IMAGE_ALIGN_LEFT_MID); lua_setfield(L, -2, "IMAGE_ALIGN_LEFT_MID");
    lua_pushinteger(L, LV_IMAGE_ALIGN_RIGHT_MID); lua_setfield(L, -2, "IMAGE_ALIGN_RIGHT_MID");
    lua_pushinteger(L, LV_IMAGE_ALIGN_CENTER); lua_setfield(L, -2, "IMAGE_ALIGN_CENTER");
    lua_pushinteger(L, LV_IMAGE_ALIGN_STRETCH); lua_setfield(L, -2, "IMAGE_ALIGN_STRETCH");
    lua_pushinteger(L, LV_IMAGE_ALIGN_TILE); lua_setfield(L, -2, "IMAGE_ALIGN_TILE");
    lua_pushinteger(L, LV_IMAGE_ALIGN_CONTAIN); lua_setfield(L, -2, "IMAGE_ALIGN_CONTAIN");
    lua_pushinteger(L, LV_IMAGE_ALIGN_COVER); lua_setfield(L, -2, "IMAGE_ALIGN_COVER");
    
    // Image scale constant (256 = 100%, no scale)
    lua_pushinteger(L, LV_SCALE_NONE); lua_setfield(L, -2, "SCALE_NONE");
    
    // Direction constants
    lua_pushinteger(L, LV_DIR_NONE);   lua_setfield(L, -2, "DIR_NONE");
    lua_pushinteger(L, LV_DIR_LEFT);   lua_setfield(L, -2, "DIR_LEFT");
    lua_pushinteger(L, LV_DIR_RIGHT);  lua_setfield(L, -2, "DIR_RIGHT");
    lua_pushinteger(L, LV_DIR_TOP);    lua_setfield(L, -2, "DIR_TOP");
    lua_pushinteger(L, LV_DIR_BOTTOM); lua_setfield(L, -2, "DIR_BOTTOM");
    lua_pushinteger(L, LV_DIR_HOR);    lua_setfield(L, -2, "DIR_HOR");
    lua_pushinteger(L, LV_DIR_VER);    lua_setfield(L, -2, "DIR_VER");
    lua_pushinteger(L, LV_DIR_ALL);    lua_setfield(L, -2, "DIR_ALL");

    // Label long mode constants
    lua_pushinteger(L, LV_LABEL_LONG_MODE_WRAP);            lua_setfield(L, -2, "LABEL_LONG_WRAP");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_DOTS);            lua_setfield(L, -2, "LABEL_LONG_DOT");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_DOTS);            lua_setfield(L, -2, "LABEL_LONG_DOTS");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_SCROLL);          lua_setfield(L, -2, "LABEL_LONG_SCROLL");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR); lua_setfield(L, -2, "LABEL_LONG_SCROLL_CIRC");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR); lua_setfield(L, -2, "LABEL_LONG_SCROLL_CIRCULAR");
    lua_pushinteger(L, LV_LABEL_LONG_MODE_CLIP);            lua_setfield(L, -2, "LABEL_LONG_CLIP");

    // LED brightness constants
    lua_pushinteger(L, LV_LED_BRIGHT_MIN); lua_setfield(L, -2, "LED_BRIGHT_MIN");
    lua_pushinteger(L, LV_LED_BRIGHT_MAX); lua_setfield(L, -2, "LED_BRIGHT_MAX");

    // Roller mode constants
    lua_pushinteger(L, LV_ROLLER_MODE_NORMAL);   lua_setfield(L, -2, "ROLLER_MODE_NORMAL");
    lua_pushinteger(L, LV_ROLLER_MODE_INFINITE); lua_setfield(L, -2, "ROLLER_MODE_INFINITE");

    // Bar orientation / mode constants
    lua_pushinteger(L, LV_BAR_ORIENTATION_AUTO);       lua_setfield(L, -2, "BAR_ORIENTATION_AUTO");
    lua_pushinteger(L, LV_BAR_ORIENTATION_HORIZONTAL); lua_setfield(L, -2, "BAR_ORIENTATION_HORIZONTAL");
    lua_pushinteger(L, LV_BAR_ORIENTATION_VERTICAL);   lua_setfield(L, -2, "BAR_ORIENTATION_VERTICAL");
    lua_pushinteger(L, LV_BAR_MODE_NORMAL);            lua_setfield(L, -2, "BAR_MODE_NORMAL");
    lua_pushinteger(L, LV_BAR_MODE_SYMMETRICAL);       lua_setfield(L, -2, "BAR_MODE_SYMMETRICAL");
    lua_pushinteger(L, LV_BAR_MODE_RANGE);             lua_setfield(L, -2, "BAR_MODE_RANGE");

    // Arc mode constants
    lua_pushinteger(L, LV_ARC_MODE_NORMAL);      lua_setfield(L, -2, "ARC_MODE_NORMAL");
    lua_pushinteger(L, LV_ARC_MODE_REVERSE);     lua_setfield(L, -2, "ARC_MODE_REVERSE");
    lua_pushinteger(L, LV_ARC_MODE_SYMMETRICAL); lua_setfield(L, -2, "ARC_MODE_SYMMETRICAL");

    return 1;
}

void lvgl_lua_register(lua_State* L) {
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_pushcfunction(L, luaopen_lvgl);
    lua_setfield(L, -2, "lvgl");
    lua_pop(L, 2);
    
    luaopen_lvgl(L);
    lua_setglobal(L, "lvgl");
}
