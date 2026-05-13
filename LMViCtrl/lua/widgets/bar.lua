-- 带元数据的进度条示例
local lv     = require("lvgl")
local common = require("common.common")

local Bar = {}

Bar.__widget_meta = {
  id             = "lv_bar_basic",
  name           = "Bar",
  description    = "示例进度条，可在最小值与最大值之间显示当前进度",
  category       = "数值与状态显示",
  icon           = "img/widgets/bar.png",
  preview_image  = "img/widgets/bar.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "bar", "进度条", "数值" },

  type        = "lv_bar",
  render_mode = "custom",

  default_size = { w = 200, h = 16 },
  min_size     = { w = 16,  h = 4  },
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
      min = 4, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 16,  label = "高度", unit = "px",
      min = 2, max = 4096 },

    -- 数值
    { group = "数值", name = "value", type = "number", default = 50, label = "当前值",
      bindable = true, description = "进度条当前值" },
    { group = "数值", name = "min",   type = "number", default = 0,   label = "最小值" },
    { group = "数值", name = "max",   type = "number", default = 100, label = "最大值" },
    { group = "数值", name = "mode",  type = "enum",   default = "normal", label = "模式",
      description = "normal: 单值；symmetrical: 对称；range: 区间(双端)",
      options = {
        { value = "normal",      label = "普通" },
        { value = "symmetrical", label = "对称" },
        { value = "range",       label = "区间" },
      } },
    { group = "数值", name = "value_start", type = "number", default = 0, label = "起始值",
      advanced = true, description = "仅 mode=range 时有效" },

    -- 方向
    { group = "数值", name = "orientation", type = "enum", default = "horizontal", label = "方向",
      options = {
        { value = "horizontal", label = "水平(左->右)" },
        { value = "vertical",   label = "垂直(下->上)" },
        { value = "horizontal_rtl", label = "水平(右->左)" },
        { value = "vertical_ttb",   label = "垂直(上->下)" },
      } },

    -- 外观
    { group = "外观", name = "track_bg_color",  type = "color", default = "#cccccc", label = "轨道背景色" },
    { group = "外观", name = "indicator_color", type = "color", default = "#007acc", label = "指示色",
      bindable = true },
    { group = "外观", name = "border_color",    type = "color", default = "#888888", label = "边框色",
      advanced = true },
    { group = "外观", name = "border_width",    type = "number", default = 0,        label = "边框宽度",
      unit = "px", min = 0, max = 8, advanced = true },
    { group = "外观", name = "track_radius_ratio", type = "number", default = 100, label = "轨道圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true },
    { group = "外观", name = "indicator_radius_ratio", type = "number", default = 100, label = "指示圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "value",           target = "value",   direction = "in" },
    { name = "indicator_color", target = "color",   direction = "in" },
    { name = "enabled",         target = "enabled", direction = "in" },
    { name = "visible",         target = "visible", direction = "in" },
  },

  events = {
    { name = "clicked", label = "点击",
      description = "进度条被点击时触发", params = {} },
    { name = "pressed", label = "按下",
      description = "手指或鼠标按下进度条时触发", params = {} },
    { name = "released", label = "释放",
      description = "手指或鼠标从进度条抬起时触发", params = {} },
    { name = "long_pressed", label = "长按",
      description = "进度条被持续按住达到长按阈值时触发", params = {} },
    { name = "value_changed", label = "值改变",
      description = "进度值发生变化时触发",
      params = { { name = "value", type = "number", description = "新的当前值" } } },
  },

  event_properties = {
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_pressed_handler", type = "code", language = "lua",
      event = "pressed", label = "按下处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_released_handler", type = "code", language = "lua",
      event = "released", label = "释放处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_long_pressed_handler", type = "code", language = "lua",
      event = "long_pressed", label = "长按处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "值改变处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('value =', self.props.value)" },
  },

  draw_hints = {
    shape                       = "bar",
    value_from                  = "value",
    min_from                    = "min",
    max_from                    = "max",
    mode_from                   = "mode",
    value_start_from            = "value_start",
    orientation_from            = "orientation",
    enabled_from                = "enabled",
    track_fill_from             = "track_bg_color",
    indicator_fill_from         = "indicator_color",
    border_color_from           = "border_color",
    border_width_from           = "border_width",
    track_radius_ratio_from     = "track_radius_ratio",
    indicator_radius_ratio_from = "indicator_radius_ratio",
  },
}

function Bar.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Bar.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()      return lv.PART_MAIN      or 0x00000 end
  local function PART_INDICATOR() return lv.PART_INDICATOR or 0x20000 end

  local function mode_to_lv(m)
    if m == "symmetrical" then return lv.BAR_MODE_SYMMETRICAL or 1 end
    if m == "range"       then return lv.BAR_MODE_RANGE       or 2 end
    return lv.BAR_MODE_NORMAL or 0
  end

  local function pct_to_radius(pct, dim)
    pct = pct or 100
    if pct >= 100 then return math.floor((dim or 16) / 2) end
    if pct <= 0   then return 0 end
    return math.floor((dim or 16) * pct / 200)
  end

  local function apply_mode()
    if self.bar and self.bar.set_mode then self.bar:set_mode(mode_to_lv(self.props.mode)) end
  end
  local function apply_range()
    if self.bar and self.bar.set_range then self.bar:set_range(self.props.min, self.props.max) end
  end
  local function apply_value()
    if self.bar and self.bar.set_value then
      self.bar:set_value(self.props.value, lv.ANIM_OFF or 0)
    end
    if self.props.mode == "range" and self.bar and self.bar.set_start_value then
      self.bar:set_start_value(self.props.value_start, lv.ANIM_OFF or 0)
    end
  end
  local function apply_orientation()
    -- LVGL 通过 base_dir 与 set_orientation 控制；这里用 set_size 方向 + style transform 简化处理
    -- LVGL v9: lv_bar_set_orientation(bar, LV_BAR_ORIENTATION_*)
    if self.bar and self.bar.set_orientation then
      local o = self.props.orientation
      local v = lv.BAR_ORIENTATION_HORIZONTAL or 0
      if     o == "vertical"        then v = lv.BAR_ORIENTATION_VERTICAL or 1
      elseif o == "horizontal_rtl"  then v = lv.BAR_ORIENTATION_HORIZONTAL_RTL or 2
      elseif o == "vertical_ttb"    then v = lv.BAR_ORIENTATION_VERTICAL_TTB   or 3
      end
      self.bar:set_orientation(v)
    end
  end
  local function apply_track_bg()
    if self.bar and self.bar.set_style_bg_color then
      self.bar:set_style_bg_color(common.colorToNumber(self.props.track_bg_color, 0xcccccc), PART_MAIN())
    end
  end
  local function apply_indicator()
    if self.bar and self.bar.set_style_bg_color then
      self.bar:set_style_bg_color(common.colorToNumber(self.props.indicator_color, 0x007acc), PART_INDICATOR())
    end
  end
  local function apply_border()
    if not self.bar then return end
    if self.bar.set_style_border_width then
      self.bar:set_style_border_width(self.props.border_width or 0, PART_MAIN())
    end
    if self.bar.set_style_border_color then
      self.bar:set_style_border_color(common.colorToNumber(self.props.border_color, 0x888888), PART_MAIN())
    end
  end
  local function apply_radius()
    if self.bar and self.bar.set_style_radius then
      local d = math.min(self.props.width or 16, self.props.height or 16)
      self.bar:set_style_radius(pct_to_radius(self.props.track_radius_ratio,     d), PART_MAIN())
      self.bar:set_style_radius(pct_to_radius(self.props.indicator_radius_ratio, d), PART_INDICATOR())
    end
  end
  local function apply_enabled_state()
    if not self.bar then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.bar.clear_state then self.bar:clear_state(sd) end
    else
      if self.bar.add_state   then self.bar:add_state(sd) end
    end
  end
  local function apply_visible()
    if not self.bar then return end
    if self.props.visible then
      if self.bar.clear_flag and lv.OBJ_FLAG_HIDDEN then self.bar:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.bar.add_flag   and lv.OBJ_FLAG_HIDDEN then self.bar:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.bar = lv.bar_create(parent)
    self.bar:set_pos(self.props.x, self.props.y)
    self.bar:set_size(self.props.width, self.props.height)
    apply_orientation()
    apply_mode()
    apply_range()
    apply_value()
    apply_track_bg()
    apply_indicator()
    apply_border()
    apply_radius()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[bar] callback error:", err) end
      end
    end
    local ev_code
    if event_name == "clicked" then ev_code = lv.EVENT_CLICKED
    elseif event_name == "pressed" then ev_code = lv.EVENT_PRESSED
    elseif event_name == "released" then ev_code = lv.EVENT_RELEASED
    elseif event_name == "long_pressed" then ev_code = lv.EVENT_LONG_PRESSED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[bar] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.bar.add_event_cb then
      self.bar:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.bar, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "value" or name == "value_start" then
      self._suppress_event = true; apply_value(); self._suppress_event = false
    elseif name == "min" or name == "max" then apply_range(); apply_value()
    elseif name == "mode" then apply_mode(); apply_value()
    elseif name == "orientation" then apply_orientation()
    elseif name == "track_bg_color"  then apply_track_bg()
    elseif name == "indicator_color" then apply_indicator()
    elseif name == "border_color" or name == "border_width" then apply_border()
    elseif name == "track_radius_ratio" or name == "indicator_radius_ratio" then apply_radius()
    elseif name == "x" or name == "y" then
      if self.bar then self.bar:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.bar then self.bar:set_size(self.props.width, self.props.height); apply_radius() end
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
    if self.bar and self.bar.del then self.bar:del()
    elseif self.bar and lv.obj_del then lv.obj_del(self.bar) end
    self.bar = nil
  end

  return self
end

return Bar
