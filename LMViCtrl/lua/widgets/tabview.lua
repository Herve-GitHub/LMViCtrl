-- 带元数据的标签页示例
local lv     = require("lvgl")
local common = require("common.common")

local TabView = {}

TabView.__widget_meta = {
  id             = "lv_tabview_basic",
  name           = "TabView",
  description    = "示例标签页容器：标签栏 + 切换内容区",
  category       = "容器与页面组织",
  icon           = "img/widgets/tabview.png",
  preview_image  = "img/widgets/tabview.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "tabview", "标签", "容器" },

  type        = "lv_tabview",
  render_mode = "custom",

  default_size = { w = 320, h = 220 },
  min_size     = { w = 100, h = 80  },
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
    { group = "布局", name = "width",  type = "number", default = 320, label = "宽度", unit = "px", min = 80, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 220, label = "高度", unit = "px", min = 60, max = 4096 },

    { group = "结构", name = "tabs", type = "string",
      default = "",
      label = "标签(每行一项)", multiline = true, lines = 4 },
    { group = "结构", name = "current_tab", type = "number", default = 0,
      label = "当前标签", min = 0, max = 64, bindable = true },
    { group = "结构", name = "tab_position", type = "enum", default = "top", label = "标签位置",
      options = {
        { value = "top",    label = "上" },
        { value = "bottom", label = "下" },
        { value = "left",   label = "左" },
        { value = "right",  label = "右" },
      } },
    { group = "结构", name = "tab_size", type = "number", default = 36,
      label = "标签栏厚度", unit = "px", min = 16, max = 200 },

    { group = "外观", name = "bg_color",          type = "color", default = "#ffffff", label = "内容区背景" },
    { group = "外观", name = "tabbar_bg",         type = "color", default = "#f4f6f9", label = "标签栏背景" },
    { group = "外观", name = "tab_text_color",    type = "color", default = "#5b6470", label = "标签文字" },
    { group = "外观", name = "active_text_color", type = "color", default = "#1976d2", label = "当前标签文字" },
    { group = "外观", name = "indicator_color",   type = "color", default = "#1976d2", label = "指示条颜色" },
    { group = "外观", name = "border_color",      type = "color", default = "#cfd6dd", label = "边框", advanced = true },
    { group = "外观", name = "font_size",         type = "number", default = 13, label = "字号", unit = "px", min = 6, max = 64 },

    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用", bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见", bindable = true },
  },

  bindings = {
    { name = "current_tab", target = "value",   direction = "inout" },
    { name = "enabled",     target = "enabled", direction = "in" },
    { name = "visible",     target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "切换标签",
      params = { { name = "tab", type = "number", description = "新的当前标签索引" } } },
    { name = "clicked", label = "点击", params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "切换处理代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape                  = "tabview",
    tabs_from              = "tabs",
    current_tab_from       = "current_tab",
    tab_position_from      = "tab_position",
    tab_size_from          = "tab_size",
    bg_color_from          = "bg_color",
    tabbar_bg_from         = "tabbar_bg",
    tab_text_color_from    = "tab_text_color",
    active_text_color_from = "active_text_color",
    indicator_color_from   = "indicator_color",
    border_color_from      = "border_color",
    font_size_from         = "font_size",
    enabled_from           = "enabled",
  },
}

local function parse_lines(text)
  local out = {}
  if type(text) ~= "string" then return out end
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do table.insert(out, line) end
  while #out > 0 and out[#out] == "" do table.remove(out) end
  return out
end

function TabView.new(parent, state)
  state = state or {}
  local self = {}
  self.props = {}
  for _, p in ipairs(TabView.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}
  self._suppress_event = false

  local function pos_to_lv(p)
    if p == "bottom" then return lv.DIR_BOTTOM or 2 end
    if p == "left"   then return lv.DIR_LEFT   or 4 end
    if p == "right"  then return lv.DIR_RIGHT  or 8 end
    return lv.DIR_TOP or 1
  end

  local function rebuild()
    if not self.tab then return end
    local names = parse_lines(self.props.tabs)
    if self.tab.add_tab then
      for _, n in ipairs(names) do self.tab:add_tab(n) end
    end
    if self.tab.set_act then
      self.tab:set_act(self.props.current_tab or 0, lv.ANIM_OFF or 0)
    end
  end

  local function apply_visible()
    if not self.tab then return end
    if self.props.visible then
      if self.tab.clear_flag and lv.OBJ_FLAG_HIDDEN then self.tab:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.tab.add_flag   and lv.OBJ_FLAG_HIDDEN then self.tab:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.tab then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then if self.tab.clear_state then self.tab:clear_state(sd) end
    else if self.tab.add_state then self.tab:add_state(sd) end end
  end

  function self.draw()
    if lv.tabview_create then
      self.tab = lv.tabview_create(parent, pos_to_lv(self.props.tab_position),
                                    self.props.tab_size or 36)
    else
      self.tab = lv.obj_create(parent)
    end
    self.tab:set_pos(self.props.x, self.props.y)
    self.tab:set_size(self.props.width, self.props.height)
    rebuild()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        if event_name == "value_changed" and self.tab and self.tab.get_tab_act then
          self.props.current_tab = self.tab:get_tab_act()
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[tabview] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[tabview] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.tab.add_event_cb then self.tab:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.tab, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "current_tab" then
      if self.tab and self.tab.set_act then
        self._suppress_event = true
        self.tab:set_act(value, lv.ANIM_OFF or 0)
        self._suppress_event = false
      end
    elseif name == "x" or name == "y" then
      if self.tab then self.tab:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.tab then self.tab:set_size(self.props.width, self.props.height) end
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
    if self.tab and self.tab.del then self.tab:del()
    elseif self.tab and lv.obj_del then lv.obj_del(self.tab) end
    self.tab = nil
  end
  return self
end

return TabView
