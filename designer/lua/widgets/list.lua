-- 带元数据的列表示例
local lv     = require("lvgl")
local common = require("common.common")

local List = {}

List.__widget_meta = {
  id             = "lv_list_basic",
  name           = "List",
  description    = "示例列表，支持多个文本/图标条目",
  category       = "容器与页面组织",
  icon           = "img/widgets/list.png",
  preview_image  = "img/widgets/list.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "list", "列表", "容器" },

  type        = "lv_list",
  render_mode = "custom",

  default_size = { w = 200, h = 220 },
  min_size     = { w = 60,  h = 60  },
  max_size     = { w = 4096, h = 4096 },
  resizable    = true,
  rotatable    = false,

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
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X",    unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y",    unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 200, label = "宽度", unit = "px", min = 40, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 220, label = "高度", unit = "px", min = 40, max = 4096 },

    { group = "数据", name = "items", type = "string",
      default = "首页\n设置\n关于\n退出",
      label = "条目(每行一项)", multiline = true, lines = 6,
      description = "每行一个条目；若以 [#] 开头将作为分组标题" },
    { group = "数据", name = "selected_index", type = "number", default = 0,
      label = "选中索引", min = -1, max = 9999, bindable = true,
      description = "0 起；-1 表示无选中" },

    { group = "外观", name = "bg_color",       type = "color",  default = "#ffffff", label = "背景色" },
    { group = "外观", name = "item_text_color", type = "color", default = "#1f2933", label = "条目文字" },
    { group = "外观", name = "item_bg_color",  type = "color",  default = "#f4f6f9", label = "条目背景" },
    { group = "外观", name = "selected_color", type = "color",  default = "#1976d2", label = "选中背景" },
    { group = "外观", name = "selected_text",  type = "color",  default = "#ffffff", label = "选中文字" },
    { group = "外观", name = "section_color",  type = "color",  default = "#7a8290", label = "分组标题色", advanced = true },
    { group = "外观", name = "row_height",     type = "number", default = 36,        label = "行高",
      unit = "px", min = 12, max = 200 },
    { group = "外观", name = "font_size",      type = "number", default = 14,        label = "字号",
      unit = "px", min = 6, max = 64 },
    { group = "外观", name = "border_color",   type = "color",  default = "#cfd6dd", label = "边框", advanced = true },
    { group = "外观", name = "border_width",   type = "number", default = 1,
      label = "边框宽度", unit = "px", min = 0, max = 16, advanced = true },

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
    { name = "value_changed", label = "选中变化",
      description = "选中条目变化时触发",
      params = { { name = "index", type = "number", description = "选中条目索引" } } },
    { name = "clicked", label = "点击", params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "选中变化代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape               = "list",
    items_from          = "items",
    selected_index_from = "selected_index",
    bg_color_from       = "bg_color",
    item_text_color_from = "item_text_color",
    item_bg_color_from  = "item_bg_color",
    selected_color_from = "selected_color",
    selected_text_from  = "selected_text",
    section_color_from  = "section_color",
    row_height_from     = "row_height",
    font_size_from      = "font_size",
    border_color_from   = "border_color",
    border_width_from   = "border_width",
    enabled_from        = "enabled",
  },
}

local function parse_items(text)
  local out = {}
  if type(text) ~= "string" then return out end
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do
    table.insert(out, line)
  end
  -- 去尾部空行
  while #out > 0 and out[#out] == "" do table.remove(out) end
  return out
end

function List.new(parent, state)
  state = state or {}
  local self = {}
  self.props = {}
  for _, p in ipairs(List.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}
  self._buttons = {}

  local function rebuild()
    if not self.list then return end
    if self.list.clean then self.list:clean() end
    self._buttons = {}
    local items = parse_items(self.props.items)
    for _, txt in ipairs(items) do
      if txt:sub(1, 3) == "[#]" then
        if self.list.add_text then self.list:add_text(txt:sub(4)) end
      else
        if self.list.add_btn then
          local b = self.list:add_btn(nil, txt)
          table.insert(self._buttons, b)
        end
      end
    end
  end

  local function apply_visible()
    if not self.list then return end
    if self.props.visible then
      if self.list.clear_flag and lv.OBJ_FLAG_HIDDEN then self.list:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.list.add_flag   and lv.OBJ_FLAG_HIDDEN then self.list:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.list then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then if self.list.clear_state then self.list:clear_state(sd) end
    else if self.list.add_state then self.list:add_state(sd) end end
  end

  function self.draw()
    self.list = lv.list_create(parent)
    self.list:set_pos(self.props.x, self.props.y)
    self.list:set_size(self.props.width, self.props.height)
    rebuild()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[list] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[list] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.list.add_event_cb then self.list:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.list, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "items" then rebuild()
    elseif name == "x" or name == "y" then
      if self.list then self.list:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.list then self.list:set_size(self.props.width, self.props.height) end
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
    self._cb_handles = {}; self._buttons = {}
    if self.list and self.list.del then self.list:del()
    elseif self.list and lv.obj_del then lv.obj_del(self.list) end
    self.list = nil
  end
  return self
end

return List
