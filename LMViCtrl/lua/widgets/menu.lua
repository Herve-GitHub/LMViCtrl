-- 带元数据的菜单示例
local lv     = require("lvgl")
local common = require("common.common")

local Menu = {}

Menu.__widget_meta = {
  id             = "lv_menu_basic",
  name           = "Menu",
  description    = "示例菜单容器：左侧主菜单 + 右侧详情面板",
  category       = "容器与页面组织",
  icon           = "img/widgets/menu.png",
  preview_image  = "img/widgets/menu.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "menu", "菜单", "容器" },

  type        = "lv_menu",
  render_mode = "custom",

  default_size = { w = 360, h = 240 },
  min_size     = { w = 120, h = 80  },
  max_size     = { w = 4096, h = 4096 },
  resizable    = true,

  api = {
    has_draw             = true,
    has_on               = true,
    has_get_property     = true,
    has_set_property     = true,
    has_get_properties   = true,
    has_apply_properties = true,
    has_to_state         = true,
    has_destroy          = true,
  },

  properties = {
    { group = "布局", name = "x", type = "number", default = 0,   label = "X", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "y", type = "number", default = 0,   label = "Y", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 360, label = "宽度", unit = "px", min = 80, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 240, label = "高度", unit = "px", min = 60, max = 4096 },

    { group = "结构", name = "items", type = "string",
      default = "",
      label = "主菜单项(每行一项)", multiline = true, lines = 6 },
    { group = "结构", name = "selected_index", type = "number", default = 0,
      label = "选中项", min = 0, max = 9999, bindable = true },
    { group = "结构", name = "sidebar_ratio",  type = "number", default = 35,
      label = "侧栏宽占比(%)", unit = "%", min = 10, max = 80 },
    { group = "结构", name = "header_text",    type = "string", default = "设置",
      label = "详情区标题" },
    { group = "结构", name = "detail_text",    type = "string",
      default = "在这里展示当前菜单项的详细内容",
      label = "详情区内容", multiline = true, lines = 4 },

    { group = "外观", name = "bg_color",        type = "color", default = "#ffffff", label = "背景色" },
    { group = "外观", name = "sidebar_bg",      type = "color", default = "#f4f6f9", label = "侧栏背景" },
    { group = "外观", name = "item_text_color", type = "color", default = "#1f2933", label = "条目文字" },
    { group = "外观", name = "selected_color",  type = "color", default = "#1976d2", label = "选中背景" },
    { group = "外观", name = "selected_text",   type = "color", default = "#ffffff", label = "选中文字" },
    { group = "外观", name = "header_bg",       type = "color", default = "#e8eef7", label = "标题背景" },
    { group = "外观", name = "header_text_color", type = "color", default = "#1f2933", label = "标题文字" },
    { group = "外观", name = "border_color",    type = "color", default = "#cfd6dd", label = "边框", advanced = true },
    { group = "外观", name = "row_height",      type = "number", default = 32, label = "条目行高", unit = "px", min = 16, max = 96 },
    { group = "外观", name = "font_size",       type = "number", default = 13, label = "字号", unit = "px", min = 6, max = 64 },

    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用", bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见", bindable = true },
  },

  bindings = {
    { name = "items",          target = "value",   direction = "in" },
    { name = "selected_index", target = "value",   direction = "inout" },
    { name = "enabled",        target = "enabled", direction = "in" },
    { name = "visible",        target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "切换菜单项", params = {} },
    { name = "clicked",       label = "点击",       params = {} },
    { name = "pressed",       label = "按下",       params = {} },
    { name = "released",      label = "释放",       params = {} },
    { name = "long_pressed",  label = "长按",       params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "切换处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_pressed_handler", type = "code", language = "lua",
      event = "pressed", label = "按下代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_released_handler", type = "code", language = "lua",
      event = "released", label = "释放代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_long_pressed_handler", type = "code", language = "lua",
      event = "long_pressed", label = "长按代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape                = "menu",
    items_from           = "items",
    selected_index_from  = "selected_index",
    sidebar_ratio_from   = "sidebar_ratio",
    header_text_from     = "header_text",
    detail_text_from     = "detail_text",
    bg_color_from        = "bg_color",
    sidebar_bg_from      = "sidebar_bg",
    item_text_color_from = "item_text_color",
    selected_color_from  = "selected_color",
    selected_text_from   = "selected_text",
    header_bg_from       = "header_bg",
    header_text_color_from = "header_text_color",
    border_color_from    = "border_color",
    row_height_from      = "row_height",
    font_size_from       = "font_size",
    enabled_from         = "enabled",
  },
}

local function parse_items(text)
  local out = {}
  if type(text) ~= "string" then return out end
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do table.insert(out, line) end
  while #out > 0 and out[#out] == "" do table.remove(out) end
  return out
end

function Menu.new(parent, state)
  state = state or {}
  local self = {}
  self.props = {}
  for _, p in ipairs(Menu.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}

  local function apply_visible()
    if not self.menu then return end
    if self.props.visible then
      if self.menu.clear_flag and lv.OBJ_FLAG_HIDDEN then self.menu:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.menu.add_flag   and lv.OBJ_FLAG_HIDDEN then self.menu:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.menu then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then if self.menu.clear_state then self.menu:clear_state(sd) end
    else if self.menu.add_state then self.menu:add_state(sd) end end
  end
  local function build()
    if not self.menu then return end
    local items = parse_items(self.props.items)

    -- Main page (sidebar entry list)
    if lv.menu_page_create then
      self._main_page = lv.menu_page_create(self.menu, nil)
      self._sub_page  = lv.menu_page_create(self.menu, self.props.header_text or "")

      if self._sub_page and lv.label_create then
        local lbl = lv.label_create(self._sub_page)
        if lbl.set_text then lbl:set_text(self.props.detail_text or "") end
      end

      for _, name in ipairs(items) do
        if lv.menu_cont_create then
          local cont = lv.menu_cont_create(self._main_page)
          if lv.label_create then
            local lbl = lv.label_create(cont)
            if lbl.set_text then lbl:set_text(name) end
          end
          if self.menu.set_load_page_event then
            self.menu:set_load_page_event(cont, self._sub_page)
          end
        end
      end
      if self.menu.set_page then self.menu:set_page(self._main_page) end
    end
  end

  function self.draw()
    self.menu = lv.menu_create(parent)
    self.menu:set_pos(self.props.x, self.props.y)
    self.menu:set_size(self.props.width, self.props.height)
    build()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[menu] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "pressed"       then ev_code = lv.EVENT_PRESSED
    elseif event_name == "released"      then ev_code = lv.EVENT_RELEASED
    elseif event_name == "long_pressed"  then ev_code = lv.EVENT_LONG_PRESSED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[menu] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.menu.add_event_cb then self.menu:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.menu, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "x" or name == "y" then
      if self.menu then self.menu:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.menu then self.menu:set_size(self.props.width, self.props.height) end
    elseif name == "enabled" then apply_enabled()
    elseif name == "visible" then apply_visible()
    end
    return true
  end
  function self:get_properties()
    local out = {}; for k, v in pairs(self.props) do out[k] = v end; return out
  end
  function self:apply_properties(t) for k, v in pairs(t) do self:set_property(k, v) end; return true end
  function self:to_state() return self:get_properties() end
  function self:destroy()
    self._cb_handles = {}
    if self.menu and self.menu.del then self.menu:del()
    elseif self.menu and lv.obj_del then lv.obj_del(self.menu) end
    self.menu = nil
  end
  return self
end

return Menu
