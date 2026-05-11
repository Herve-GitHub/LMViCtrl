-- 带元数据的 LED 指示灯示例
local lv     = require("lvgl")
local common = require("common.common")

local Led = {}

Led.__widget_meta = {
  id             = "lv_led_basic",
  name           = "LED",
  description    = "示例 LED 指示灯，可控制颜色与亮度",
  category       = "数值与状态显示",
  icon           = "img/widgets/led.png",
  preview_image  = "img/widgets/led.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "led", "指示灯", "状态" },

  type        = "lv_led",
  render_mode = "custom",

  default_size = { w = 32, h = 32 },
  min_size     = { w = 8,  h = 8  },
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
    { group = "布局", name = "x",      type = "number", default = 0,  label = "X",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "y",      type = "number", default = 0,  label = "Y",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 32, label = "宽度", unit = "px",
      min = 4, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 32, label = "高度", unit = "px",
      min = 4, max = 4096 },

    -- 状态
    { group = "状态", name = "on",         type = "boolean", default = true, label = "点亮",
      bindable = true, description = "LED 当前是否点亮" },
    { group = "状态", name = "brightness", type = "number",  default = 255,  label = "亮度",
      min = 0, max = 255, bindable = true,
      description = "0=完全熄灭，255=最亮（点亮态生效）" },

    -- 外观
    { group = "外观", name = "color", type = "color", default = "#22cc44", label = "LED 颜色",
      bindable = true, description = "点亮时的发光颜色" },
    { group = "外观", name = "shape", type = "enum",  default = "circle",  label = "形状",
      options = {
        { value = "circle", label = "圆形"  },
        { value = "square", label = "方形"  },
      } },
    { group = "外观", name = "glow", type = "boolean", default = true, label = "发光效果",
      description = "点亮时叠加柔光" },

    -- 行为
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "on",         target = "value",   direction = "in" },
    { name = "brightness", target = "value",   direction = "in" },
    { name = "color",      target = "color",   direction = "in" },
    { name = "visible",    target = "visible", direction = "in" },
  },

  events = {
    -- LED 通常作为只读指示，不暴露默认事件
  },

  event_properties = {
  },

  draw_hints = {
    shape_kind        = "led", -- 用 shape_kind 避免与 shape 属性冲突
    shape             = "led",
    on_from           = "on",
    brightness_from   = "brightness",
    color_from        = "color",
    led_shape_from    = "shape",
    glow_from         = "glow",
  },
}

function Led.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Led.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles = {}

  local function clamp_brightness(value)
    value = tonumber(value) or 0
    if value < 0 then return 0 end
    if value > 255 then return 255 end
    return math.floor(value)
  end

  local function apply_color()
    if self.led and self.led.set_color then
      self.led:set_color(common.colorToNumber(self.props.color, 0x22cc44))
    end
  end
  local function apply_brightness()
    if not self.led then return end
    -- 熄灭态等价于亮度为 LV_LED_BRIGHT_MIN(80)；点亮态用 brightness
    local b
    if self.props.on then
      b = clamp_brightness(self.props.brightness or 255)
    else
      b = lv.LED_BRIGHT_MIN or 80
    end
    if self.led.set_brightness then self.led:set_brightness(b) end
  end
  local function apply_glow()
    if not self.led then return end

    local part = 0
    local brightness = clamp_brightness(self.props.brightness or 255)
    local strength = brightness / 255
    local size = math.max(self.props.width or 32, self.props.height or 32)
    local enabled = self.props.on and self.props.glow and brightness > 0

    if enabled then
      if self.led.set_style_shadow_width then
        self.led:set_style_shadow_width(math.max(4, math.floor(size * 0.45)), part)
      end
      if self.led.set_style_shadow_color then
        self.led:set_style_shadow_color(common.colorToNumber(self.props.color, 0x22cc44), part)
      end
      if self.led.set_style_shadow_opa then
        self.led:set_style_shadow_opa(math.floor(180 * strength), part)
      end
      if self.led.set_style_shadow_spread then
        self.led:set_style_shadow_spread(math.max(1, math.floor(size * 0.08)), part)
      end
      if self.led.set_style_shadow_offset_x then self.led:set_style_shadow_offset_x(0, part) end
      if self.led.set_style_shadow_offset_y then self.led:set_style_shadow_offset_y(0, part) end
    else
      if self.led.set_style_shadow_width then self.led:set_style_shadow_width(0, part) end
      if self.led.set_style_shadow_opa then self.led:set_style_shadow_opa(0, part) end
      if self.led.set_style_shadow_spread then self.led:set_style_shadow_spread(0, part) end
    end
  end
  local function apply_shape()
    -- LVGL 没有原生方/圆切换；通过 radius 模拟：圆形=圆角、方形=0
    if self.led and self.led.set_style_radius then
      if self.props.shape == "circle" then
        self.led:set_style_radius(math.min(self.props.width or 32, self.props.height or 32) // 2, 0)
      else
        self.led:set_style_radius(0, 0)
      end
    end
  end
  local function apply_visible()
    if not self.led then return end
    if self.props.visible then
      if self.led.clear_flag and lv.OBJ_FLAG_HIDDEN then self.led:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.led.add_flag   and lv.OBJ_FLAG_HIDDEN then self.led:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.led = lv.led_create(parent)
    self.led:set_pos(self.props.x, self.props.y)
    self.led:set_size(self.props.width, self.props.height)
    apply_color()
    apply_brightness()
    apply_shape()
    apply_glow()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[led] callback error:", err) end
      end
    end
    local ev_code
    if event_name == "clicked" then ev_code = lv.EVENT_CLICKED
    else
      print("[led] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.led.add_event_cb then
      self.led:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.led, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if     name == "color"       then apply_color(); apply_glow()
    elseif name == "on" or name == "brightness" then apply_brightness(); apply_glow()
    elseif name == "shape"       then apply_shape()
    elseif name == "x" or name == "y" then
      if self.led then self.led:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.led then self.led:set_size(self.props.width, self.props.height); apply_shape(); apply_glow() end
    elseif name == "glow"        then apply_glow()
    elseif name == "visible"     then apply_visible()
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
    if self.led and self.led.del then self.led:del()
    elseif self.led and lv.obj_del then lv.obj_del(self.led) end
    self.led = nil
  end

  return self
end

return Led
