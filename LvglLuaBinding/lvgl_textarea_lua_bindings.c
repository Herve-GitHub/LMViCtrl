/**
 * @file lvgl_textarea_lua_bindings.c
 * @brief LVGL Textarea Lua bindings
 */

#include "lvgl_lua_bindings_internal.h"

// ========== Internal helper to delete selection ==========
static bool internal_delete_selection(lv_obj_t* obj) {
    if (!obj || !lv_obj_check_type(obj, &lv_textarea_class)) {
        return false;
    }
    
    if (!lv_textarea_text_is_selected(obj)) {
        return false;
    }
    
    lv_obj_t* label = lv_textarea_get_label(obj);
    if (!label) {
        return false;
    }
    
    uint32_t sel_start = lv_label_get_text_selection_start(label);
    uint32_t sel_end = lv_label_get_text_selection_end(label);
    
    // Make sure start < end
    if (sel_start > sel_end) {
        uint32_t tmp = sel_start;
        sel_start = sel_end;
        sel_end = tmp;
    }
    
    // Clear selection first
    lv_textarea_clear_selection(obj);
    
    // Move cursor to end of selection and delete backwards
    lv_textarea_set_cursor_pos(obj, sel_end);
    uint32_t chars_to_delete = sel_end - sel_start;
    for (uint32_t i = 0; i < chars_to_delete; i++) {
        lv_textarea_delete_char(obj);
    }
    
    return true;
}

// ========== Textarea specific methods ==========

// textarea:set_text(text)
static int l_textarea_set_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj) {
        if (lv_obj_check_type(obj, &lv_textarea_class)) {
            lv_textarea_set_text(obj, text);
        } else if (lv_obj_check_type(obj, &lv_label_class)) {
            lv_label_set_text(obj, text);
        }
    }
    return 0;
}

// textarea:get_text()
static int l_textarea_get_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj) {
        if (lv_obj_check_type(obj, &lv_textarea_class)) {
            const char* text = lv_textarea_get_text(obj);
            lua_pushstring(L, text ? text : "");
        } else if (lv_obj_check_type(obj, &lv_label_class)) {
            const char* text = lv_label_get_text(obj);
            lua_pushstring(L, text ? text : "");
        } else {
            lua_pushstring(L, "");
        }
    } else {
        lua_pushstring(L, "");
    }
    return 1;
}

// textarea:set_placeholder_text(text)
static int l_textarea_set_placeholder_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj) lv_textarea_set_placeholder_text(obj, text);
    return 0;
}

// textarea:set_one_line(en)
static int l_textarea_set_one_line(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    bool en = lua_toboolean(L, 2);
    if (obj) lv_textarea_set_one_line(obj, en);
    return 0;
}

// textarea:set_password_mode(en)
static int l_textarea_set_password_mode(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    bool en = lua_toboolean(L, 2);
    if (obj) lv_textarea_set_password_mode(obj, en);
    return 0;
}

// textarea:set_accepted_chars(list)
static int l_textarea_set_accepted_chars(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    const char* list = luaL_checkstring(L, 2);
    if (obj) lv_textarea_set_accepted_chars(obj, list);
    return 0;
}

// textarea:set_max_length(num)
static int l_textarea_set_max_length(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    uint32_t num = (uint32_t)luaL_checkinteger(L, 2);
    if (obj) lv_textarea_set_max_length(obj, num);
    return 0;
}

// textarea:add_char(c)
static int l_textarea_add_char(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    uint32_t c = (uint32_t)luaL_checkinteger(L, 2);
    if (obj) lv_textarea_add_char(obj, c);
    return 0;
}

// textarea:add_text(text)
static int l_textarea_add_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    const char* text = luaL_checkstring(L, 2);
    if (obj) lv_textarea_add_text(obj, text);
    return 0;
}

// textarea:delete_char()
static int l_textarea_delete_char(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj) lv_textarea_delete_char(obj);
    return 0;
}

// textarea:set_cursor_pos(pos)
static int l_textarea_set_cursor_pos(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    int32_t pos = (int32_t)luaL_checkinteger(L, 2);
    if (obj) lv_textarea_set_cursor_pos(obj, pos);
    return 0;
}

// textarea:get_cursor_pos()
static int l_textarea_get_cursor_pos(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    lua_pushinteger(L, obj ? lv_textarea_get_cursor_pos(obj) : 0);
    return 1;
}

// textarea:set_cursor_click_pos(en) - Enable/disable cursor positioning by clicking
static int l_textarea_set_cursor_click_pos(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    bool en = lua_toboolean(L, 2);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_textarea_set_cursor_click_pos(obj, en);
    }
    return 0;
}

// textarea:set_text_selection(en) - Enable/disable text selection
static int l_textarea_set_text_selection(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    bool en = lua_toboolean(L, 2);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_textarea_set_text_selection(obj, en);
    }
    return 0;
}

// textarea:get_text_selection() - Check if text selection is enabled
static int l_textarea_get_text_selection(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lua_pushboolean(L, lv_textarea_get_text_selection(obj));
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// textarea:text_is_selected() - Check if any text is currently selected
static int l_textarea_text_is_selected(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lua_pushboolean(L, lv_textarea_text_is_selected(obj));
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// textarea:get_label() - Get the label object of the textarea
static int l_textarea_get_label(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_t* label = lv_textarea_get_label(obj);
        if (label) {
            push_lv_obj(L, label);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

// textarea:get_selection_start() - Get selection start position
static int l_textarea_get_selection_start(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_t* label = lv_textarea_get_label(obj);
        if (label) {
            uint32_t start = lv_label_get_text_selection_start(label);
            lua_pushinteger(L, start);
            return 1;
        }
    }
    lua_pushinteger(L, -1);
    return 1;
}

// textarea:get_selection_end() - Get selection end position
static int l_textarea_get_selection_end(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_t* label = lv_textarea_get_label(obj);
        if (label) {
            uint32_t end = lv_label_get_text_selection_end(label);
            lua_pushinteger(L, end);
            return 1;
        }
    }
    lua_pushinteger(L, -1);
    return 1;
}

// Helper function to get byte position from character index
static uint32_t get_byte_pos(const char* text, uint32_t char_pos) {
    uint32_t byte_pos = 0;
    uint32_t char_count = 0;
    while (text[byte_pos] != '\0' && char_count < char_pos) {
        uint8_t c = (uint8_t)text[byte_pos];
        if (c < 0x80) {
            byte_pos += 1;
        } else if ((c & 0xE0) == 0xC0) {
            byte_pos += 2;
        } else if ((c & 0xF0) == 0xE0) {
            byte_pos += 3;
        } else if ((c & 0xF8) == 0xF0) {
            byte_pos += 4;
        } else {
            byte_pos += 1;
        }
        char_count++;
    }
    return byte_pos;
}

// textarea:get_selected_text() - Get the currently selected text
static int l_textarea_get_selected_text(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        if (!lv_textarea_text_is_selected(obj)) {
            lua_pushnil(L);
            return 1;
        }
        
        lv_obj_t* label = lv_textarea_get_label(obj);
        if (label) {
            uint32_t sel_start = lv_label_get_text_selection_start(label);
            uint32_t sel_end = lv_label_get_text_selection_end(label);
            
            // Make sure start < end
            if (sel_start > sel_end) {
                uint32_t tmp = sel_start;
                sel_start = sel_end;
                sel_end = tmp;
            }
            
            const char* text = lv_textarea_get_text(obj);
            if (text) {
                // Convert character positions to byte positions
                uint32_t byte_start = get_byte_pos(text, sel_start);
                uint32_t byte_end = get_byte_pos(text, sel_end);
                
                size_t sel_len = byte_end - byte_start;
                char* selected = (char*)malloc(sel_len + 1);
                if (selected) {
                    memcpy(selected, text + byte_start, sel_len);
                    selected[sel_len] = '\0';
                    lua_pushstring(L, selected);
                    free(selected);
                    return 1;
                }
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

// textarea:delete_selection() - Delete the currently selected text
static int l_textarea_delete_selection(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    lua_pushboolean(L, internal_delete_selection(obj));
    return 1;
}

// textarea:select_all() - Select all text
static int l_textarea_select_all(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        lv_obj_t* label = lv_textarea_get_label(obj);
        if (label) {
            const char* text = lv_textarea_get_text(obj);
            if (text) {
                // Count characters (not bytes) for UTF-8
                uint32_t char_count = 0;
                const char* p = text;
                while (*p) {
                    uint8_t c = (uint8_t)*p;
                    if (c < 0x80) {
                        p += 1;
                    } else if ((c & 0xE0) == 0xC0) {
                        p += 2;
                    } else if ((c & 0xF0) == 0xE0) {
                        p += 3;
                    } else if ((c & 0xF8) == 0xF0) {
                        p += 4;
                    } else {
                        p += 1;
                    }
                    char_count++;
                }
                
                lv_label_set_text_selection_start(label, 0);
                lv_label_set_text_selection_end(label, char_count);
                lv_obj_invalidate(obj);
                lua_pushboolean(L, 1);
                return 1;
            }
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

// textarea:smart_delete_char() - Delete selection if any, otherwise delete one char (backspace)
static int l_textarea_smart_delete_char(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        // If text is selected, delete the selection
        if (lv_textarea_text_is_selected(obj)) {
            lua_pushboolean(L, internal_delete_selection(obj));
        } else {
            // Otherwise delete one character (backspace behavior)
            lv_textarea_delete_char(obj);
            lua_pushboolean(L, 1);
        }
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// textarea:smart_delete_char_forward() - Delete selection if any, otherwise delete one char forward (delete key)
static int l_textarea_smart_delete_char_forward(lua_State* L) {
    lv_obj_t* obj = check_lv_obj(L, 1);
    if (obj && lv_obj_check_type(obj, &lv_textarea_class)) {
        // If text is selected, delete the selection
        if (lv_textarea_text_is_selected(obj)) {
            lua_pushboolean(L, internal_delete_selection(obj));
        } else {
            // Otherwise delete one character forward (delete key behavior)
            lv_textarea_delete_char_forward(obj);
            lua_pushboolean(L, 1);
        }
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// lv.textarea_get_text(obj) - Module level function
int l_lv_textarea_get_text(lua_State* L) {
    return l_textarea_get_text(L);
}

// ========== Textarea Methods Table ==========
static const luaL_Reg lv_textarea_methods[] = {
    {"set_placeholder_text", l_textarea_set_placeholder_text},
    {"set_one_line", l_textarea_set_one_line},
    {"set_password_mode", l_textarea_set_password_mode},
    {"set_accepted_chars", l_textarea_set_accepted_chars},
    {"set_max_length", l_textarea_set_max_length},
    {"add_char", l_textarea_add_char},
    {"add_text", l_textarea_add_text},
    {"delete_char", l_textarea_delete_char},
    {"set_cursor_pos", l_textarea_set_cursor_pos},
    {"get_cursor_pos", l_textarea_get_cursor_pos},
    {"set_cursor_click_pos", l_textarea_set_cursor_click_pos},
    {"set_text_selection", l_textarea_set_text_selection},
    {"get_text_selection", l_textarea_get_text_selection},
    {"text_is_selected", l_textarea_text_is_selected},
    {"get_label", l_textarea_get_label},
    {"get_selection_start", l_textarea_get_selection_start},
    {"get_selection_end", l_textarea_get_selection_end},
    {"get_selected_text", l_textarea_get_selected_text},
    {"delete_selection", l_textarea_delete_selection},
    {"smart_delete_char", l_textarea_smart_delete_char},
    {"smart_delete_char_forward", l_textarea_smart_delete_char_forward},
    {NULL, NULL}
};

const luaL_Reg* lvgl_get_textarea_methods(void) {
    return lv_textarea_methods;
}