-- 带元数据的滑块示例
local lv     = require("lvgl")
local common = require("common.common")

local Slider = {}

Slider.__widget_meta = {
  id             = "lv_slider_basic",
  name           = "Slider",
  description    = "示例滑块，可在最小值和最大值之间拖动选择数值",
  category       = "交互输入",
  icon           = "img/widgets/slider.png",
  preview_image  = "img/widgets/slider.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "slider", "滑块", "交互", "数值" },

  type        = "lv_slider",
  render_mode = "custom",

  default_size = { w = 200, h = 16 },
  min_size     = { w = 40,  h = 8  },
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
    { group = "布局", name = "height", type = "number", default = 16,  label = "高度", unit = "px",
      min = 4, max = 4096 },

    -- 数值
    { group = "数值", name = "value", type = "number", default = 50, label = "当前值",
      bindable = true, description = "滑块当前值" },
    { group = "数值", name = "min",   type = "number", default = 0,   label = "最小值" },
    { group = "数值", name = "max",   type = "number", default = 100, label = "最大值" },
    { group = "数值", name = "mode",  type = "enum",   default = "normal", label = "模式",
      description = "normal: 单值；symmetrical: 对称；range: 区间(双滑块)",
      options = {
        { value = "normal",       label = "普通"     },
        { value = "symmetrical",  label = "对称"     },
        { value = "range",        label = "区间"     },
      } },
    { group = "数值", name = "value_left", type = "number", default = 0, label = "区间左值",
      advanced = true, description = "仅 mode=range 时有效" },

    -- 外观
    { group = "外观", name = "track_bg_color", type = "color",  default = "#cccccc", label = "轨道背景色" },
    { group = "外观", name = "indicator_color", type = "color", default = "#007acc", label = "指示色",
      bindable = true },
    { group = "外观", name = "knob_color",      type = "color", default = "#ffffff", label = "滑块颜色" },
    { group = "外观", name = "knob_border_color", type = "color", default = "#007acc", label = "滑块边框色",
      advanced = true },
    { group = "外观", name = "knob_border_width", type = "number", default = 2, label = "滑块边框宽度",
      unit = "px", min = 0, max = 8, advanced = true },
    { group = "外观", name = "track_radius_ratio", type = "number", default = 100, label = "轨道圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true },
    { group = "外观", name = "knob_radius_ratio",  type = "number", default = 100, label = "滑块圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "value",           target = "value",   direction = "inout" },
    { name = "indicator_color", target = "color",   direction = "in"    },
    { name = "enabled",         target = "enabled", direction = "in"    },
    { name = "visible",         target = "visible", direction = "in"    },
    { name = "changed",         target = "trigger", direction = "out"   },
  },

  events = {
    { name = "clicked",       label = "点击",
      description = "滑块被点击时触发", params = {} },
    { name = "value_changed", label = "值改变",
      description = "拖动或点击导致值变化时触发",
      params = { { name = "value", type = "number", description = "新的当前值" } } },
    { name = "released",      label = "释放",
      description = "拖动结束、手指/鼠标抬起时触发",
      params = { { name = "value", type = "number", description = "释放时的值" } } },
  },

  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      description = "点击滑块时执行的 Lua 代码" },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "值改变处理代码",
      default = "", multiline = true, lines = 6,
      description = "值变化时执行（self.props.value 已更新）",
      snippet = "print('value =', self.props.value)" },
    { name = "on_released_handler",      type = "code", language = "lua",
      event = "released",      label = "释放处理代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape                   = "slider",
    value_from              = "value",
    min_from                = "min",
    max_from                = "max",
    mode_from               = "mode",
    value_left_from         = "value_left",
    enabled_from            = "enabled",
    track_fill_from         = "track_bg_color",
    indicator_fill_from     = "indicator_color",
    knob_fill_from          = "knob_color",
    border_color_from       = "knob_border_color",
    border_width_from       = "knob_border_width",
    track_radius_ratio_from = "track_radius_ratio",
    knob_radius_ratio_from  = "knob_radius_ratio",
  },
}

function Slider.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Slider.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()      return lv.PART_MAIN      or 0x00000 end
  local function PART_INDICATOR() return lv.PART_INDICATOR or 0x20000 end
  local function PART_KNOB()      return lv.PART_KNOB      or 0x30000 end

  local function mode_to_lv(m)
    if m == "symmetrical" then return lv.SLIDER_MODE_SYMMETRICAL or 1 end
    if m == "range"       then return lv.SLIDER_MODE_RANGE       or 2 end
    return lv.SLIDER_MODE_NORMAL or 0
  end

  local function apply_track_bg()
    if self.sl and self.sl.set_style_bg_color then
      self.sl:set_style_bg_color(common.colorToNumber(self.props.track_bg_color, 0xcccccc), PART_MAIN())
    end
  end
  local function apply_indicator()
    if self.sl and self.sl.set_style_bg_color then
      self.sl:set_style_bg_color(common.colorToNumber(self.props.indicator_color, 0x007acc), PART_INDICATOR())
    end
  end
  local function apply_knob_color()
    if self.sl and self.sl.set_style_bg_color then
      self.sl:set_style_bg_color(common.colorToNumber(self.props.knob_color, 0xffffff), PART_KNOB())
    end
  end
  local function apply_knob_border()
    if not self.sl then return end
    if self.sl.set_style_border_width then
      self.sl:set_style_border_width(self.props.knob_border_width or 0, PART_KNOB())
    end
    if self.sl.set_style_border_color then
      self.sl:set_style_border_color(common.colorToNumber(self.props.knob_border_color, 0x007acc), PART_KNOB())
    end
  end
  local function pct_to_radius(pct, dim)
    pct = pct or 100
    if pct >= 100 then return math.floor((dim or 16) / 2) end
    if pct <= 0   then return 0 end
    return math.floor((dim or 16) * pct / 200)
  end
  local function apply_radius()
    if self.sl and self.sl.set_style_radius then
      self.sl:set_style_radius(pct_to_radius(self.props.track_radius_ratio, self.props.height), PART_MAIN())
      self.sl:set_style_radius(pct_to_radius(self.props.track_radius_ratio, self.props.height), PART_INDICATOR())
      self.sl:set_style_radius(pct_to_radius(self.props.knob_radius_ratio,  self.props.height), PART_KNOB())
    end
  end
  local function apply_range()
    if self.sl and self.sl.set_range then
      self.sl:set_range(self.props.min, self.props.max)
    end
  end
  local function apply_mode()
    if self.sl and self.sl.set_mode then
      self.sl:set_mode(mode_to_lv(self.props.mode))
    end
  end
  local function apply_value()
    if self.sl and self.sl.set_value then
      self.sl:set_value(self.props.value, lv.ANIM_OFF or 0)
    end
    if self.props.mode == "range" and self.sl and self.sl.set_left_value then
      self.sl:set_left_value(self.props.value_left, lv.ANIM_OFF or 0)
    end
  end
  local function apply_enabled_state()
    if not self.sl then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.sl.clear_state then self.sl:clear_state(sd) end
    else
      if self.sl.add_state   then self.sl:add_state(sd) end
    end
  end
  local function apply_visible()
    if not self.sl then return end
    if self.props.visible then
      if self.sl.clear_flag and lv.OBJ_FLAG_HIDDEN then self.sl:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.sl.add_flag   and lv.OBJ_FLAG_HIDDEN then self.sl:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.sl = lv.slider_create(parent)
    self.sl:set_pos(self.props.x, self.props.y)
    self.sl:set_size(self.props.width, self.props.height)
    apply_range()
    apply_mode()
    apply_value()
    apply_track_bg()
    apply_indicator()
    apply_knob_color()
    apply_knob_border()
    apply_radius()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        if self.sl and self.sl.get_value then
          self.props.value = self.sl:get_value()
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[slider] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    elseif event_name == "released"      then ev_code = lv.EVENT_RELEASED
    else
      print("[slider] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.sl.add_event_cb then
      self.sl:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.sl, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "track_bg_color"     then apply_track_bg()
    elseif name == "indicator_color" then apply_indicator()
    elseif name == "knob_color"      then apply_knob_color()
    elseif name == "knob_border_color" or name == "knob_border_width" then apply_knob_border()
    elseif name == "track_radius_ratio" or name == "knob_radius_ratio" then apply_radius()
    elseif name == "min" or name == "max" then apply_range(); apply_value()
    elseif name == "mode"  then apply_mode(); apply_value()
    elseif name == "value" or name == "value_left" then
      self._suppress_event = true; apply_value(); self._suppress_event = false
    elseif name == "x" or name == "y" then
      if self.sl then self.sl:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.sl then self.sl:set_size(self.props.width, self.props.height); apply_radius() end
    elseif name == "enabled" then apply_enabled_state()
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
    if self.sl and self.sl.del then self.sl:del()
    elseif self.sl and lv.obj_del then lv.obj_del(self.sl) end
    self.sl = nil
  end

  return self
end

return Slider
