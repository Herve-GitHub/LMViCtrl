local lv     = require("lvgl")
local common = require("common.common")

local Line = {}

Line.__widget_meta = {
  id             = "line",
  name           = "Line",
  description    = "直线",
  category       = "基础对象",
  icon           = "img/widgets/line.png",
  preview_image  = "img/widgets/line.png",
  schema_version = "1.1",
  version        = "1.0",
  tags           = { "line" },

  type        = "lv_button",
  render_mode = "builtin",

  default_size = { w = 100, h = 1 },  -- 高度1=直线
  min_size     = { w = 1,  h = 1 },
  max_size     = { w = 4096, h = 4096 },
  resizable    = true,
  rotatable    = false,

  api = {
    has_draw             = true,
    has_on               = false,  -- 无点击
    has_get_property     = true,
    has_set_property     = true,
    has_get_properties   = true,
    has_apply_properties = true,
    has_to_state         = true,
    has_destroy          = true,
  },

  properties = {
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X", min=-4096, max=4096 },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y", min=-4096, max=4096 },
    { group = "布局", name = "width",  type = "number", default = 100, label = "宽度", min=1, max=4096 },
    { group = "布局", name = "height", type = "number", default = 1,   label = "高度", min=1, max=4096 },

    { group = "外观", name = "color",    type = "color", default = "#cccccc", label = "线条颜色" },
    { group = "外观", name = "radius",    type = "number", default = 0, label = "圆角", min=0, max=64 },

    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见" },
  },

  bindings = {},
  events = {},
  event_properties = {},

  draw_hints = {
    shape              = "rounded_rect",
    fill_from          = "color",
    radius_from        = "radius",
  },
}

function Line.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Line.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles = {}

  local function apply_color()
    if not self.btn then return end
    local c = common.colorToNumber(self.props.color, 0xcccccc)
    self.btn:set_style_bg_color(c, 0)
    self.btn:set_style_border_width(0, 0)
  end

  local function apply_radius()
    if not self.btn then return end
    self.btn:set_style_radius(self.props.radius, 0)
  end

  local function apply_visible()
    if not self.btn then return end
    if self.props.visible then
      self.btn:remove_flag(lv.OBJ_FLAG_HIDDEN)
    else
      self.btn:add_flag(lv.OBJ_FLAG_HIDDEN)
    end
  end

  -- 去掉点击、去掉按下效果
  local function remove_all_effects()
    self.btn:clear_state(lv.STATE_PRESSED)
    self.btn:clear_state(lv.STATE_FOCUSED)
    self.btn:set_click(false)
    self.btn:set_style_bg_opa(255, 0)
  end

  function self.draw()
    self.btn = lv.btn_create(parent)
    self.btn:set_size(self.props.width, self.props.height)
    self.btn:set_pos(self.props.x, self.props.y)

    self.btn:remove_flag(lv.EVENT_CLICKED)

    apply_color()
    apply_radius()
    apply_visible()
    remove_all_effects()
  end
  self.draw()

  -- 空实现，禁止点击
  function self:on(event_name, callback)
  end

  function self:get_property(name)
    return self.props[name]
  end

  function self:set_property(name, value)
    self.props[name] = value

    if name == "x" or name == "y" then
      self.btn:set_pos(self.props.x, self.props.y)
    elseif name == "width" or name == "height" then
      self.btn:set_size(self.props.width, self.props.height)
    elseif name == "color" then
      apply_color()
    elseif name == "radius" then
      apply_radius()
    elseif name == "visible" then
      apply_visible()
    end
    return true
  end

  function self:get_properties()
    local out = {}
    for k, v in pairs(self.props) do out[k] = v end
    return out
  end

  function self:apply_properties(props_table)
    for k, v in pairs(props_table) do
      self:set_property(k, v)
    end
    return true
  end

  function self:to_state()
    return self:get_properties()
  end

  function self:destroy()
    self._cb_handles = {}
    if self.btn then self.btn:del() end
    self.btn = nil
  end

  return self
end

return Line