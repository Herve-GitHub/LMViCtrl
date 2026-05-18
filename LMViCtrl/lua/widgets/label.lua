local lv     = require("lvgl")
local common = require("common.common")

local Label = {}

Label.__widget_meta = {
  id             = "lv_label_basic",
  name           = "Label",
  description    = "基础文本标签，支持样式调整与数据绑定",
  category       = "基础对象",
  icon           = "img/widgets/label.png",
  preview_image  = "img/widgets/label.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "label", "文本", "基础" },

  type        = "lv_label",
  render_mode = "builtin",
  default_size = { w = 120, h = 28 },
  min_size     = { w = 8,   h = 8  },
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
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 X 坐标" },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 Y 坐标" },
    { group = "布局", name = "width",  type = "number", default = 120, label = "宽度", unit = "px",
      min = 1, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 28,  label = "高度", unit = "px",
      min = 1, max = 4096 },

    { group = "外观", name = "text",        type = "string", default = "Label",   label = "文本",
      bindable = true, description = "标签显示的文本，可绑定到变量",
      multiline = true, lines = 3 },
    { group = "外观", name = "color",       type = "color",  default = "#0xffffff", label = "文本颜色" },
    { group = "外观", name = "bg_color",    type = "color",  default = "#00000000", label = "背景色", advanced = true },
    { group = "外观", name = "font_size",   type = "number", default = 14,        label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "alignment",   type = "enum",   default = "left",    label = "对齐方式",
      options = {
        { value = "left",   label = "左对齐" },
        { value = "center", label = "居中"   },
        { value = "right",  label = "右对齐" },
      } },
    { group = "外观", name = "long_mode",   type = "enum",   default = "wrap",    label = "长文本模式",
      options = {
        { value = "wrap",       label = "自动换行" },
        { value = "dot",        label = "省略号"   },
        { value = "scroll",     label = "滚动"     },
        { value = "clip",       label = "裁剪"     },
      } },

    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "text",    target = "text",    direction = "in" },
    { name = "color",   target = "color",   direction = "in" },
    { name = "visible", target = "visible", direction = "in" },
  },

  events = {
    { name = "clicked", label = "点击", description = "点击时触发" },
  },

  event_properties = {
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击代码" },
  },

  draw_hints = {
    shape             = "rect",
    fill_from         = "bg_color",
    text_from         = "text",
    text_color_from   = "color",
    font_size_from    = "font_size",
    align_from        = "alignment",
  },
}

local function alignment_to_lv(align)
  if align == "left"  then return lv.TEXT_ALIGN_LEFT  end
  if align == "right" then return lv.TEXT_ALIGN_RIGHT end
  return lv.TEXT_ALIGN_CENTER
end

local function longmode_to_lv(mode)
  if mode == "dot"   then return lv.LABEL_LONG_DOT end
  if mode == "scroll"then return lv.LABEL_LONG_SCROLL end
  if mode == "clip"  then return lv.LABEL_LONG_CLIP end
  return lv.LABEL_LONG_WRAP
end

function Label.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Label.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles = {}
  self.label = nil

  -- ==============================================
  -- ✅ 完全对标 Button 的写法！
  -- ==============================================
  local function apply_text_color(col)
    if self.label and self.label.set_style_text_color then
      self.label:set_style_text_color(col, 0)
    end
  end

  local function apply_bg_color(col)
    if self.label and self.label.set_style_bg_color then
      self.label:set_style_bg_color(col, 0)
    end
  end

  local function apply_font()
    if not self.label or not lv.font_get_by_size then return end
    local f = lv.font_get_by_size(self.props.font_size)
    if f then self.label:set_style_text_font(f, 0) end
  end

  local function apply_align()
    if not self.label then return end
    self.label:set_style_text_align(alignment_to_lv(self.props.alignment), 0)
  end

  local function apply_visible()
    if not self.label or not lv.OBJ_FLAG_HIDDEN then return end
    if self.props.visible then
      self.label:clear_flag(lv.OBJ_FLAG_HIDDEN)
    else
      self.label:add_flag(lv.OBJ_FLAG_HIDDEN)
    end
  end

  local function apply_all()
    apply_text_color(common.colorToNumber(self.props.color, 0xffffff))
    apply_bg_color(common.colorToNumber(self.props.bg_color, 0x00000000))
    apply_font()
    apply_align()
    apply_visible()
    if self.label.set_long_mode then
      self.label:set_long_mode(longmode_to_lv(self.props.long_mode))
    end
  end

  function self.draw()
    self.label = lv.label_create(parent)
    if not self.label then return end

    self.label:set_pos(self.props.x, self.props.y)
    self.label:set_size(self.props.width, self.props.height)
    self.label:set_text(self.props.text)
    self.label:add_flag(lv.OBJ_FLAG_CLICKABLE)

    apply_all()
  end
  self.draw()

  function self:on(event_name, callback)
    if event_name ~= "clicked" then return end
    local function cb(e)
      pcall(callback, self, e)
    end
    self.label:add_event_cb(cb, lv.EVENT_CLICKED, nil)
    table.insert(self._cb_handles, cb)
  end

  function self:get_property(name)
    return self.props[name]
  end

  function self:set_property(name, value)
    self.props[name] = value
    if not self.label then return true end

    if name == "text" then
      self.label:set_text(tostring(value))
    elseif name == "color" then
      apply_text_color(common.colorToNumber(value, 0xffffff))
    elseif name == "bg_color" then
      apply_bg_color(common.colorToNumber(value, 0x00000000))
    elseif name == "font_size" then
      apply_font()
    elseif name == "alignment" then
      apply_align()
    elseif name == "long_mode" then
      self.label:set_long_mode(longmode_to_lv(value))
    elseif name == "visible" then
      apply_visible()
    elseif name == "x" or name == "y" then
      self.label:set_pos(self.props.x, self.props.y)
    elseif name == "width" or name == "height" then
      self.label:set_size(self.props.width, self.props.height)
    end
    return true
  end

  function self:get_properties()
    local out = {}
    for k,v in pairs(self.props) do out[k] = v end
    return out
  end

  function self:apply_properties(props)
    for k,v in pairs(props or {}) do
      self:set_property(k, v)
    end
  end

  function self:to_state()
    return self:get_properties()
  end

  function self:destroy()
    self._cb_handles = {}
    if self.label then self.label:del() end
    self.label = nil
  end

  return self
end

return Label