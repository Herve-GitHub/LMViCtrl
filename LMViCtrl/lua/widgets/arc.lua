-- 带元数据的圆弧/仪表示例
local lv     = require("lvgl")
local common = require("common.common")

local Arc = {}

Arc.__widget_meta = {
  id             = "lv_arc_basic",
  name           = "Arc",
  description    = "示例圆弧/仪表，可显示数值或作为可拖动的环形输入",
  category       = "数值与状态显示",
  icon           = "img/widgets/arc.png",
  preview_image  = "img/widgets/arc.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "arc", "圆弧", "仪表", "数值" },

  type        = "lv_arc",
  render_mode = "custom",

  default_size = { w = 150, h = 150 },
  min_size     = { w = 32,  h = 32  },
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
    { group = "布局", name = "width",  type = "number", default = 150, label = "宽度", unit = "px",
      min = 16, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 150, label = "高度", unit = "px",
      min = 16, max = 4096 },

    -- 数值
    { group = "数值", name = "value", type = "number", default = 50,  label = "当前值",
      bindable = true },
    { group = "数值", name = "min",   type = "number", default = 0,   label = "最小值" },
    { group = "数值", name = "max",   type = "number", default = 100, label = "最大值" },

    -- 角度
    { group = "角度", name = "bg_start_angle",  type = "number", default = 135, label = "背景起始角度",
      unit = "°", min = 0, max = 360 },
    { group = "角度", name = "bg_end_angle",    type = "number", default = 45,  label = "背景结束角度",
      unit = "°", min = 0, max = 360,
      description = "LVGL 0°=右，顺时针递增；可跨过 360 形成环形" },
    { group = "角度", name = "rotation",        type = "number", default = 0,   label = "旋转偏移",
      unit = "°", min = 0, max = 360, advanced = true },

    -- 模式
    { group = "模式", name = "mode", type = "enum", default = "normal", label = "模式",
      options = {
        { value = "normal",      label = "普通(正向)" },
        { value = "reverse",     label = "反向"       },
        { value = "symmetrical", label = "对称"       },
      } },
    { group = "模式", name = "interactive", type = "boolean", default = false, label = "可交互拖动",
      description = "禁用时仅作为显示" },

    -- 外观
    { group = "外观", name = "bg_arc_color",  type = "color",  default = "#cccccc", label = "背景弧颜色" },
    { group = "外观", name = "ind_arc_color", type = "color",  default = "#007acc", label = "指示弧颜色",
      bindable = true },
    { group = "外观", name = "arc_width",     type = "number", default = 8,         label = "弧线宽度",
      unit = "px", min = 1, max = 64 },
    { group = "外观", name = "rounded_caps",  type = "boolean", default = true,     label = "圆角端帽" },
    { group = "外观", name = "show_knob",     type = "boolean", default = false,    label = "显示拖动球",
      description = "用于交互式 Arc 时在弧端显示一个圆形按钮",
      advanced = true },
    { group = "外观", name = "knob_color",    type = "color",   default = "#ffffff", label = "拖动球颜色",
      advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "value",         target = "value",   direction = "inout" },
    { name = "ind_arc_color", target = "color",   direction = "in"    },
    { name = "enabled",       target = "enabled", direction = "in"    },
    { name = "visible",       target = "visible", direction = "in"    },
    { name = "changed",       target = "trigger", direction = "out"   },
  },

  events = {
    { name = "clicked",       label = "点击",
      description = "弧被点击时触发", params = {} },
    { name = "value_changed", label = "值改变",
      description = "拖动或赋值导致值变化时触发",
      params = { { name = "value", type = "number", description = "新的当前值" } } },
  },

  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "值改变处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('value =', self.props.value)" },
  },

  draw_hints = {
    shape               = "arc",
    value_from          = "value",
    min_from            = "min",
    max_from            = "max",
    bg_start_angle_from = "bg_start_angle",
    bg_end_angle_from   = "bg_end_angle",
    rotation_from       = "rotation",
    mode_from           = "mode",
    bg_arc_color_from   = "bg_arc_color",
    ind_arc_color_from  = "ind_arc_color",
    arc_width_from      = "arc_width",
    rounded_caps_from   = "rounded_caps",
    show_knob_from      = "show_knob",
    knob_color_from     = "knob_color",
    enabled_from        = "enabled",
  },
}

function Arc.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Arc.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()      return lv.PART_MAIN      or 0x00000 end
  local function PART_INDICATOR() return lv.PART_INDICATOR or 0x20000 end
  local function PART_KNOB()      return lv.PART_KNOB      or 0x30000 end

  local function mode_to_lv(m)
    if m == "reverse"     then return lv.ARC_MODE_REVERSE     or 1 end
    if m == "symmetrical" then return lv.ARC_MODE_SYMMETRICAL or 2 end
    return lv.ARC_MODE_NORMAL or 0
  end

  local function apply_range()
    if self.arc and self.arc.set_range then self.arc:set_range(self.props.min, self.props.max) end
  end
  local function apply_value()
    if self.arc and self.arc.set_value then self.arc:set_value(self.props.value) end
  end
  local function apply_mode()
    if self.arc and self.arc.set_mode then self.arc:set_mode(mode_to_lv(self.props.mode)) end
  end
  local function apply_angles()
    if self.arc and self.arc.set_bg_angles then
      self.arc:set_bg_angles(self.props.bg_start_angle, self.props.bg_end_angle)
    end
    if self.arc and self.arc.set_rotation then
      self.arc:set_rotation(self.props.rotation or 0)
    end
  end
  local function apply_bg_color()
    if self.arc and self.arc.set_style_arc_color then
      self.arc:set_style_arc_color(common.colorToNumber(self.props.bg_arc_color, 0xcccccc), PART_MAIN())
    end
  end
  local function apply_ind_color()
    if self.arc and self.arc.set_style_arc_color then
      self.arc:set_style_arc_color(common.colorToNumber(self.props.ind_arc_color, 0x007acc), PART_INDICATOR())
    end
  end
  local function apply_arc_width()
    if self.arc and self.arc.set_style_arc_width then
      self.arc:set_style_arc_width(self.props.arc_width or 8, PART_MAIN())
      self.arc:set_style_arc_width(self.props.arc_width or 8, PART_INDICATOR())
    end
  end
  local function apply_caps()
    if self.arc and self.arc.set_style_arc_rounded then
      self.arc:set_style_arc_rounded(self.props.rounded_caps and true or false, PART_MAIN())
      self.arc:set_style_arc_rounded(self.props.rounded_caps and true or false, PART_INDICATOR())
    end
  end
  local function apply_knob()
    if not self.arc then return end
    if not self.props.show_knob then
      -- 透明 KNOB 背景：不渲染
      if self.arc.set_style_bg_opa then self.arc:set_style_bg_opa(0, PART_KNOB()) end
      return
    end
    if self.arc.set_style_bg_opa then self.arc:set_style_bg_opa(255, PART_KNOB()) end
    if self.arc.set_style_bg_color then
      self.arc:set_style_bg_color(common.colorToNumber(self.props.knob_color, 0xffffff), PART_KNOB())
    end
  end
  local function apply_interactive()
    if not self.arc then return end
    -- 非交互态：移除 CLICKABLE，保留显示
    local CLK = lv.OBJ_FLAG_CLICKABLE or 0x10
    if self.props.interactive then
      if self.arc.add_flag then self.arc:add_flag(CLK) end
    else
      if self.arc.clear_flag then self.arc:clear_flag(CLK) end
    end
  end
  local function apply_enabled_state()
    if not self.arc then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.arc.clear_state then self.arc:clear_state(sd) end
    else
      if self.arc.add_state   then self.arc:add_state(sd) end
    end
  end
  local function apply_visible()
    if not self.arc then return end
    if self.props.visible then
      if self.arc.clear_flag and lv.OBJ_FLAG_HIDDEN then self.arc:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.arc.add_flag   and lv.OBJ_FLAG_HIDDEN then self.arc:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.arc = lv.arc_create(parent)
    self.arc:set_pos(self.props.x, self.props.y)
    self.arc:set_size(self.props.width, self.props.height)
    apply_range()
    apply_value()
    apply_mode()
    apply_angles()
    apply_bg_color()
    apply_ind_color()
    apply_arc_width()
    apply_caps()
    apply_knob()
    apply_interactive()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if event_name == "value_changed" and self.arc and self.arc.get_value then
          self.props.value = self.arc:get_value()
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[arc] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[arc] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.arc.add_event_cb then
      self.arc:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.arc, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "value" then
      self._suppress_event = true; apply_value(); self._suppress_event = false
    elseif name == "min" or name == "max" then apply_range(); apply_value()
    elseif name == "mode" then apply_mode()
    elseif name == "bg_start_angle" or name == "bg_end_angle" or name == "rotation" then apply_angles()
    elseif name == "bg_arc_color"   then apply_bg_color()
    elseif name == "ind_arc_color"  then apply_ind_color()
    elseif name == "arc_width"      then apply_arc_width()
    elseif name == "rounded_caps"   then apply_caps()
    elseif name == "show_knob" or name == "knob_color" then apply_knob()
    elseif name == "interactive"    then apply_interactive()
    elseif name == "x" or name == "y" then
      if self.arc then self.arc:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.arc then self.arc:set_size(self.props.width, self.props.height) end
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
    if self.arc and self.arc.del then self.arc:del()
    elseif self.arc and lv.obj_del then lv.obj_del(self.arc) end
    self.arc = nil
  end

  return self
end

return Arc
