-- 带元数据的文本输入框示例
local lv     = require("lvgl")
local common = require("common.common")

local TextArea = {}

TextArea.__widget_meta = {
  id             = "lv_textarea_basic",
  name           = "TextArea",
  description    = "示例文本输入框，支持单行/多行、占位符、密码模式等",
  category       = "交互输入",
  icon           = "img/widgets/textarea.png",
  preview_image  = "img/widgets/textarea.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "textarea", "输入", "文本", "交互" },

  type        = "lv_textarea",
  render_mode = "custom",

  default_size = { w = 200, h = 40 },
  min_size     = { w = 32,  h = 20 },
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
    -- 布局
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 200, label = "宽度", unit = "px",
      min = 16, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 40,  label = "高度", unit = "px",
      min = 16, max = 4096 },

    -- 内容
    { group = "内容", name = "text",        type = "string", default = "", label = "文本",
      bindable = true, multiline = true, lines = 4,
      description = "文本框当前文本，可绑定到变量" },
    { group = "内容", name = "placeholder", type = "string", default = "请输入...", label = "占位符",
      description = "文本为空时显示的提示文字" },
    { group = "内容", name = "max_length",  type = "number", default = 0, label = "最大长度",
      min = 0, max = 65535, advanced = true,
      description = "0 表示不限制" },
    { group = "内容", name = "accepted_chars", type = "string", default = "", label = "允许字符",
      advanced = true,
      description = "只允许输入此字符串中出现的字符；为空表示不限制" },

    -- 模式
    { group = "模式", name = "one_line",    type = "boolean", default = false, label = "单行模式",
      description = "启用后回车不换行" },
    { group = "模式", name = "password",    type = "boolean", default = false, label = "密码模式",
      description = "启用后输入字符显示为 *" },
    { group = "模式", name = "read_only",   type = "boolean", default = false, label = "只读" },

    -- 外观
    { group = "外观", name = "color",        type = "color",  default = "#000000", label = "文本颜色" },
    { group = "外观", name = "bg_color",     type = "color",  default = "#ffffff", label = "背景色" },
    { group = "外观", name = "border_color", type = "color",  default = "#888888", label = "边框色" },
    { group = "外观", name = "border_width", type = "number", default = 1,         label = "边框宽度",
      unit = "px", min = 0, max = 8 },
    { group = "外观", name = "radius",       type = "number", default = 4,         label = "圆角",
      unit = "px", min = 0, max = 64 },
    { group = "外观", name = "font_size",    type = "number", default = 14,        label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family",  type = "string", default = "default", label = "字体",
      advanced = true },
    { group = "外观", name = "placeholder_color", type = "color", default = "#aaaaaa", label = "占位符颜色",
      advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "text",    target = "text",    direction = "inout" },
    { name = "enabled", target = "enabled", direction = "in"    },
    { name = "visible", target = "visible", direction = "in"    },
    { name = "changed", target = "trigger", direction = "out"   },
  },

  events = {
    { name = "clicked",       label = "点击",
      description = "文本框被点击时触发", params = {} },
    { name = "focused",       label = "获得焦点",
      description = "文本框获得焦点时触发", params = {} },
    { name = "defocused",     label = "失去焦点",
      description = "文本框失去焦点时触发", params = {} },
    { name = "value_changed", label = "文本改变",
      description = "文本内容发生变化时触发",
      params = { { name = "text", type = "string", description = "当前文本" } } },
    { name = "ready",         label = "提交(回车)",
      description = "用户按下回车提交输入时触发",
      params = { { name = "text", type = "string", description = "当前文本" } } },
  },

  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_focused_handler",       type = "code", language = "lua",
      event = "focused",       label = "获得焦点处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_defocused_handler",     type = "code", language = "lua",
      event = "defocused",     label = "失去焦点处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "文本改变处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('text =', self.props.text)" },
    { name = "on_ready_handler",         type = "code", language = "lua",
      event = "ready",         label = "提交处理代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape              = "textarea",
    text_from          = "text",
    placeholder_from   = "placeholder",
    text_color_from    = "color",
    placeholder_color_from = "placeholder_color",
    fill_from          = "bg_color",
    border_color_from  = "border_color",
    border_width_from  = "border_width",
    radius_from        = "radius",
    font_size_from     = "font_size",
    font_family_from   = "font_family",
    one_line_from      = "one_line",
    password_from      = "password",
    enabled_from       = "enabled",
  },
}

function TextArea.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(TextArea.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()        return lv.PART_MAIN        or 0x00000 end
  local function PART_TEXTAREA_PLACEHOLDER()
    return lv.PART_TEXTAREA_PLACEHOLDER or 0x08
  end

  local function apply_text_color()
    if self.ta and self.ta.set_style_text_color then
      self.ta:set_style_text_color(common.colorToNumber(self.props.color, 0x000000), PART_MAIN())
    end
  end
  local function apply_bg()
    if self.ta and self.ta.set_style_bg_color then
      self.ta:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0xffffff), PART_MAIN())
    end
  end
  local function apply_border()
    if not self.ta then return end
    if self.ta.set_style_border_width then
      self.ta:set_style_border_width(self.props.border_width or 0, PART_MAIN())
    end
    if self.ta.set_style_border_color then
      self.ta:set_style_border_color(common.colorToNumber(self.props.border_color, 0x888888), PART_MAIN())
    end
  end
  local function apply_radius()
    if self.ta and self.ta.set_style_radius then
      self.ta:set_style_radius(self.props.radius or 0, PART_MAIN())
    end
  end
  local function apply_font_size()
    if self.ta and self.ta.set_style_text_font and lv.font_get_by_size then
      local f = lv.font_get_by_size(self.props.font_size)
      if f then self.ta:set_style_text_font(f, PART_MAIN()) end
    end
  end
  local function apply_placeholder()
    if self.ta and self.ta.set_placeholder_text then
      self.ta:set_placeholder_text(self.props.placeholder or "")
    end
    if self.ta and self.ta.set_style_text_color then
      self.ta:set_style_text_color(common.colorToNumber(self.props.placeholder_color, 0xaaaaaa),
                                    PART_TEXTAREA_PLACEHOLDER())
    end
  end
  local function apply_max_length()
    if self.ta and self.ta.set_max_length then
      self.ta:set_max_length(self.props.max_length or 0)
    end
  end
  local function apply_accepted_chars()
    if self.ta and self.ta.set_accepted_chars then
      local s = self.props.accepted_chars or ""
      self.ta:set_accepted_chars(s ~= "" and s or nil)
    end
  end
  local function apply_one_line()
    if self.ta and self.ta.set_one_line then
      self.ta:set_one_line(self.props.one_line and true or false)
    end
  end
  local function apply_password()
    if self.ta and self.ta.set_password_mode then
      self.ta:set_password_mode(self.props.password and true or false)
    end
  end
  local function apply_text()
    if self.ta and self.ta.set_text then
      self.ta:set_text(self.props.text or "")
    end
  end
  local function apply_enabled_state()
    if not self.ta then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.ta.clear_state then self.ta:clear_state(sd) end
    else
      if self.ta.add_state   then self.ta:add_state(sd) end
    end
    -- 只读：通过 add_state(STATE_DISABLED) 之外，也可清理 EDITABLE 标志
    if self.ta.add_flag and self.ta.clear_flag and lv.OBJ_FLAG_CLICKABLE then
      if self.props.read_only then
        -- 不阻止点击，但 LVGL textarea 没有原生 read_only；通常用 disabled 组合实现
        if self.ta.add_state then self.ta:add_state(sd) end
      end
    end
  end
  local function apply_visible()
    if not self.ta then return end
    if self.props.visible then
      if self.ta.clear_flag and lv.OBJ_FLAG_HIDDEN then self.ta:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.ta.add_flag   and lv.OBJ_FLAG_HIDDEN then self.ta:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.ta = lv.textarea_create(parent)
    self.ta:set_pos(self.props.x, self.props.y)
    self.ta:set_size(self.props.width, self.props.height)
    apply_one_line()
    apply_password()
    apply_max_length()
    apply_accepted_chars()
    apply_placeholder()
    apply_text()
    apply_text_color()
    apply_bg()
    apply_border()
    apply_radius()
    apply_font_size()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        if event_name == "value_changed" or event_name == "ready" then
          if self.ta and self.ta.get_text then
            self.props.text = self.ta:get_text() or ""
          end
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[textarea] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "focused"       then ev_code = lv.EVENT_FOCUSED
    elseif event_name == "defocused"     then ev_code = lv.EVENT_DEFOCUSED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    elseif event_name == "ready"         then ev_code = lv.EVENT_READY
    else
      print("[textarea] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.ta.add_event_cb then
      self.ta:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.ta, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "text" then
      self._suppress_event = true; apply_text(); self._suppress_event = false
    elseif name == "placeholder" or name == "placeholder_color" then apply_placeholder()
    elseif name == "color"             then apply_text_color()
    elseif name == "bg_color"          then apply_bg()
    elseif name == "border_color" or name == "border_width" then apply_border()
    elseif name == "radius"            then apply_radius()
    elseif name == "font_size"         then apply_font_size()
    elseif name == "max_length"        then apply_max_length()
    elseif name == "accepted_chars"    then apply_accepted_chars()
    elseif name == "one_line"          then apply_one_line()
    elseif name == "password"          then apply_password()
    elseif name == "x" or name == "y"  then
      if self.ta then self.ta:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.ta then self.ta:set_size(self.props.width, self.props.height) end
    elseif name == "enabled" or name == "read_only" then apply_enabled_state()
    elseif name == "visible"           then apply_visible()
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
    if self.ta and self.ta.del then self.ta:del()
    elseif self.ta and lv.obj_del then lv.obj_del(self.ta) end
    self.ta = nil
  end

  return self
end

return TextArea
