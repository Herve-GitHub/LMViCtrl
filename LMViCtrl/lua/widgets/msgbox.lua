-- 带元数据的消息框示例
local lv     = require("lvgl")
local common = require("common.common")

local MsgBox = {}

MsgBox.__widget_meta = {
  id             = "lv_msgbox_basic",
  name           = "MessageBox",
  description    = "示例消息框：标题 + 内容 + 按钮组",
  category       = "容器与页面组织",
  icon           = "img/widgets/msgbox.png",
  preview_image  = "img/widgets/msgbox.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "msgbox", "消息框", "对话框" },

  type        = "lv_msgbox",
  render_mode = "custom",

  default_size = { w = 280, h = 160 },
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
    { group = "布局", name = "width",  type = "number", default = 280, label = "宽度", unit = "px", min = 80, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 160, label = "高度", unit = "px", min = 60, max = 4096 },

    { group = "内容", name = "title",   type = "string", default = "提示", label = "标题" },
    { group = "内容", name = "message", type = "string", default = "这是消息框的内容文字。",
      label = "消息内容", multiline = true, lines = 4 },
    { group = "内容", name = "buttons", type = "string", default = "确定,取消",
      label = "按钮文本(逗号分隔)" },
    { group = "内容", name = "selected_button", type = "number", default = -1,
      label = "选中按钮", min = -1, max = 16, bindable = true,
      description = "用户最近点击的按钮索引；-1 表示未点击" },

    { group = "外观", name = "bg_color",      type = "color", default = "#ffffff", label = "背景色" },
    { group = "外观", name = "title_color",   type = "color", default = "#1f2933", label = "标题颜色" },
    { group = "外观", name = "title_bg",      type = "color", default = "#e8eef7", label = "标题栏背景" },
    { group = "外观", name = "message_color", type = "color", default = "#3c4451", label = "正文颜色" },
    { group = "外观", name = "btn_bg",        type = "color", default = "#1976d2", label = "按钮背景" },
    { group = "外观", name = "btn_text",      type = "color", default = "#ffffff", label = "按钮文字" },
    { group = "外观", name = "border_color",  type = "color", default = "#cfd6dd", label = "边框", advanced = true },
    { group = "外观", name = "border_width",  type = "number", default = 1,
      label = "边框宽度", unit = "px", min = 0, max = 16 },
    { group = "外观", name = "radius",        type = "number", default = 8,
      label = "圆角半径", unit = "px", min = 0, max = 200 },
    { group = "外观", name = "title_size",    type = "number", default = 16, label = "标题字号", unit = "px", min = 6, max = 64 },
    { group = "外观", name = "font_size",     type = "number", default = 13, label = "正文字号", unit = "px", min = 6, max = 64 },
    { group = "外观", name = "show_close_btn", type = "boolean", default = true, label = "显示关闭按钮" },

    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用", bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见", bindable = true },
  },

  bindings = {
    { name = "title",           target = "value",   direction = "in" },
    { name = "message",         target = "value",   direction = "in" },
    { name = "selected_button", target = "value",   direction = "out" },
    { name = "enabled",         target = "enabled", direction = "in" },
    { name = "visible",         target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "按钮点击",
      params = { { name = "index", type = "number", description = "被点击按钮索引" } } },
    { name = "clicked", label = "点击", params = {} },
    { name = "pressed", label = "按下", params = {} },
    { name = "released", label = "释放", params = {} },
    { name = "long_pressed", label = "长按", params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "按钮点击代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('button =', self.props.selected_button)" },
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
    shape               = "msgbox",
    title_from          = "title",
    message_from        = "message",
    buttons_from        = "buttons",
    bg_color_from       = "bg_color",
    title_color_from    = "title_color",
    title_bg_from       = "title_bg",
    message_color_from  = "message_color",
    btn_bg_from         = "btn_bg",
    btn_text_from       = "btn_text",
    border_color_from   = "border_color",
    border_width_from   = "border_width",
    radius_from         = "radius",
    title_size_from     = "title_size",
    font_size_from      = "font_size",
    show_close_btn_from = "show_close_btn",
    enabled_from        = "enabled",
  },
}

local function parse_btns(text)
  local out = {}
  if type(text) ~= "string" then return out end
  for s in (text .. ","):gmatch("([^,]*),") do
    local v = s:gsub("^%s+", ""):gsub("%s+$", "")
    if v ~= "" then table.insert(out, v) end
  end
  return out
end

function MsgBox.new(parent, state)
  state = state or {}
  local self = {}
  self.props = {}
  for _, p in ipairs(MsgBox.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}

  local function build()
    if not self.box then return end
    -- LVGL v9 风格：lv.msgbox_create(parent, title, text, btns, add_close_btn)
    -- 这里更新已存在 box 的可访问字段
    if self.box.set_title and self.props.title then self.box:set_title(self.props.title) end
    if self.box.set_text  and self.props.message then self.box:set_text(self.props.message) end
  end

  local function apply_visible()
    if not self.box then return end
    if self.props.visible then
      if self.box.clear_flag and lv.OBJ_FLAG_HIDDEN then self.box:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.box.add_flag   and lv.OBJ_FLAG_HIDDEN then self.box:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.box then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then if self.box.clear_state then self.box:clear_state(sd) end
    else if self.box.add_state then self.box:add_state(sd) end end
  end

  function self.draw()
    local btns = parse_btns(self.props.buttons)
    if lv.msgbox_create then
      self.box = lv.msgbox_create(parent, self.props.title or "",
                                   self.props.message or "",
                                   btns, self.props.show_close_btn and true or false)
    else
      self.box = lv.obj_create(parent)
    end
    if self.box and self.box.set_pos  then self.box:set_pos(self.props.x, self.props.y) end
    if self.box and self.box.set_size then self.box:set_size(self.props.width, self.props.height) end
    build()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        if event_name == "value_changed" and self.box and self.box.get_active_btn then
          self.props.selected_button = self.box:get_active_btn()
        end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[msgbox] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "pressed"       then ev_code = lv.EVENT_PRESSED
    elseif event_name == "released"      then ev_code = lv.EVENT_RELEASED
    elseif event_name == "long_pressed"  then ev_code = lv.EVENT_LONG_PRESSED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[msgbox] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.box and self.box.add_event_cb then self.box:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.box, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "title" or name == "message" then build()
    elseif name == "x" or name == "y" then
      if self.box then self.box:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.box then self.box:set_size(self.props.width, self.props.height) end
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
    if self.box and self.box.del then self.box:del()
    elseif self.box and lv.obj_del then lv.obj_del(self.box) end
    self.box = nil
  end
  return self
end

return MsgBox
