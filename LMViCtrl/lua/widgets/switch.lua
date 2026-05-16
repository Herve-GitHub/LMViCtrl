-- 带元数据的开关示例，演示如何将属性暴露给编辑器
local lv     = require("lvgl")
local common = require("common.common")

local Switch = {}

Switch.__widget_meta = {
  -- ===== 控件标识 =====
  id             = "lv_switch_basic",
  name           = "Switch",
  description    = "示例开关，用于二态切换（开/关）",
  category       = "交互输入",
  icon           = "img/widgets/switch.png",
  preview_image  = "img/widgets/switch.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "switch", "开关", "交互" },

  -- ===== 渲染策略 =====
  -- type        : 真正的 LVGL 对象类型（运行时使用）
  -- render_mode : Qt 设计器端渲染策略
  --   builtin -> Qt 用内置渲染器按 type 绘制
  --   custom  -> Qt 解析 self.draw() 或使用 draw_hints 通用绘制
  --   image   -> Qt 直接用 preview_image 占位
  type        = "lv_switch",
  render_mode = "custom",

  -- ===== 尺寸约束 =====
  default_size = { w = 50, h = 25 },
  min_size     = { w = 24, h = 14 },
  max_size     = { w = 4096, h = 4096 },
  resizable    = true,
  rotatable    = false,

  -- ===== 生命周期 / API 契约（供 Qt 端反射调用） =====
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

  -- ===== 基础属性 =====
  properties = {
    -- 布局组
    { group = "布局", name = "x",      type = "number", default = 0,  label = "X",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 X 坐标" },
    { group = "布局", name = "y",      type = "number", default = 0,  label = "Y",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 Y 坐标" },
    { group = "布局", name = "width",  type = "number", default = 50, label = "宽度", unit = "px",
      min = 16, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 25, label = "高度", unit = "px",
      min = 12, max = 4096 },

    -- 外观组
    { group = "外观", name = "off_bg_color", type = "color", default = "#cccccc", label = "关闭背景色" },
    { group = "外观", name = "on_bg_color",  type = "color", default = "#007acc", label = "开启背景色",
      bindable = true },
    { group = "外观", name = "knob_color",   type = "color", default = "#ffffff", label = "滑块颜色" },
    { group = "外观", name = "border_color", type = "color", default = "#888888", label = "边框色",
      advanced = true },
    { group = "外观", name = "border_width", type = "number", default = 0,        label = "边框宽度",
      unit = "px", min = 0, max = 8, advanced = true },
    { group = "外观", name = "knob_radius_ratio", type = "number", default = 100, label = "滑块圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true,
      description = "0 为方形，100 为圆形" },
    { group = "外观", name = "track_radius_ratio", type = "number", default = 100, label = "轨道圆角(%)",
      unit = "%", min = 0, max = 100, advanced = true,
      description = "0 为方形，100 为胶囊形" },

    -- 状态组
    { group = "状态", name = "checked", type = "boolean", default = false, label = "开启",
      bindable = true, description = "开关当前是否处于开启状态" },

    -- 行为组
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true, description = "禁用时开关变灰且不响应事件" },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  -- ===== 数据绑定声明 =====
  bindings = {
    { name = "checked", target = "value",   direction = "inout" },
    { name = "enabled", target = "enabled", direction = "in"    },
    { name = "visible", target = "visible", direction = "in"    },
    { name = "changed", target = "trigger", direction = "out"   },
  },

  -- ===== 事件定义（结构化） =====
  events = {
    { name = "clicked",       label = "点击",
      description = "开关被点击时触发",
      params = {} },
    { name = "pressed",       label = "按下",
      description = "手指或鼠标按下开关时触发",
      params = {} },
    { name = "released",      label = "释放",
      description = "手指或鼠标从开关抬起时触发",
      params = {} },
    { name = "long_pressed",  label = "长按",
      description = "开关被持续按住达到长按阈值时触发",
      params = {} },
    { name = "value_changed", label = "状态改变",
      description = "开关状态在开/关之间切换时触发",
      params = { { name = "checked", type = "boolean", description = "新的开启状态" } } },
  },

  -- ===== 事件处理代码 =====
  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      description = "点击开关时执行的 Lua 代码",
      snippet = "-- self 为开关实例\nprint('clicked')" },
    { name = "on_pressed_handler",       type = "code", language = "lua",
      event = "pressed",       label = "按下处理代码",
      default = "", multiline = true, lines = 6,
      description = "按下开关时执行的 Lua 代码" },
    { name = "on_released_handler",      type = "code", language = "lua",
      event = "released",      label = "释放处理代码",
      default = "", multiline = true, lines = 6,
      description = "释放开关时执行的 Lua 代码" },
    { name = "on_long_pressed_handler",  type = "code", language = "lua",
      event = "long_pressed",  label = "长按处理代码",
      default = "", multiline = true, lines = 6,
      description = "长按开关时执行的 Lua 代码" },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "状态改变处理代码",
      default = "", multiline = true, lines = 6,
      description = "开关状态变化时执行的 Lua 代码（self.props.checked 已更新）",
      snippet = "-- self 为开关实例\nprint('checked =', self.props.checked)" },
  },

  -- ===== 自定义渲染提示（供 render_mode = "custom" 时 Qt 端通用绘制使用） =====
  draw_hints = {
    shape                   = "switch", -- 提示 Qt 端按"轨道 + 滑块"组合绘制
    checked_from            = "checked",
    enabled_from            = "enabled",
    track_off_fill_from     = "off_bg_color",
    track_on_fill_from      = "on_bg_color",
    knob_fill_from          = "knob_color",
    border_color_from       = "border_color",
    border_width_from       = "border_width",
    track_radius_ratio_from = "track_radius_ratio",
    knob_radius_ratio_from  = "knob_radius_ratio",
  },
}

-- =====================================================================
-- 工厂方法 new(parent, state)
-- =====================================================================
function Switch.new(parent, state)
  state = state or {}
  local self = {}

  -- ---------------- 初始化属性 ----------------
  self.props = {}
  for _, p in ipairs(Switch.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  -- 事件回调缓存（供 destroy 清理）
  self._cb_handles = {}
  self._event_listeners = {}

  -- 内部状态切换抑制标志（避免 set_property 与事件回调相互递归）
  self._suppress_event = false

  local function to_boolean(value)
    if type(value) == "boolean" then return value end
    if type(value) == "number" then return value ~= 0 end
    if type(value) == "string" then
      local v = value:lower()
      return v == "true" or v == "1" or v == "yes" or v == "on"
    end
    return not not value
  end

  -- ---------------- 样式应用（集中管理） ----------------
  local function PART_MAIN()      return lv.PART_MAIN      or 0x00000  end
  local function PART_INDICATOR() return lv.PART_INDICATOR or 0x20000  end
  local function PART_KNOB()      return lv.PART_KNOB      or 0x30000  end

  -- 关闭态：MAIN part 的背景色（轨道未激活背景）
  local function apply_off_bg()
    if not self.sw then return end
    if self.sw.set_style_bg_color then
      self.sw:set_style_bg_color(common.colorToNumber(self.props.off_bg_color, 0xcccccc), PART_MAIN())
    end
  end

  -- 开启态：INDICATOR part 的背景色
  local function apply_on_bg()
    if not self.sw then return end
    if self.sw.set_style_bg_color then
      self.sw:set_style_bg_color(common.colorToNumber(self.props.on_bg_color, 0x007acc), PART_INDICATOR())
    end
  end

  local function apply_knob_color()
    if not self.sw then return end
    if self.sw.set_style_bg_color then
      self.sw:set_style_bg_color(common.colorToNumber(self.props.knob_color, 0xffffff), PART_KNOB())
    end
  end

  local function apply_border()
    if not self.sw then return end
    if self.sw.set_style_border_width then
      self.sw:set_style_border_width(self.props.border_width or 0, PART_MAIN())
    end
    if self.sw.set_style_border_color then
      self.sw:set_style_border_color(common.colorToNumber(self.props.border_color, 0x888888), PART_MAIN())
    end
  end

  local function apply_radius()
    if not self.sw then return end
    -- LVGL 的 radius 用 LV_RADIUS_CIRCLE(0x7FFF) 表示完全圆角；
    -- 这里近似处理：100% -> 取 height/2，0% -> 0
    local function pct_to_radius(pct, dim)
      pct = pct or 100
      if pct >= 100 then
        return math.floor((dim or 20) / 2)
      elseif pct <= 0 then
        return 0
      else
        return math.floor((dim or 20) * pct / 200)
      end
    end
    if self.sw.set_style_radius then
      self.sw:set_style_radius(pct_to_radius(self.props.track_radius_ratio, self.props.height), PART_MAIN())
      self.sw:set_style_radius(pct_to_radius(self.props.track_radius_ratio, self.props.height), PART_INDICATOR())
      self.sw:set_style_radius(pct_to_radius(self.props.knob_radius_ratio,  self.props.height), PART_KNOB())
    end
  end

  local function apply_checked_state()
    if not self.sw then return end
    local state_checked = (lv.STATE_CHECKED or 0x0001)
    if self.props.checked then
      if self.sw.add_state then self.sw:add_state(state_checked) end
    else
      if self.sw.clear_state then self.sw:clear_state(state_checked) end
    end
  end

  local function apply_enabled_state()
    if not self.sw then return end
    local state_disabled = (lv.STATE_DISABLED or 0x0080)
    if self.props.enabled then
      if self.sw.clear_state then self.sw:clear_state(state_disabled) end
    else
      if self.sw.add_state then self.sw:add_state(state_disabled) end
    end
  end

  local function apply_visible()
    if not self.sw then return end
    if self.props.visible then
      if self.sw.clear_flag and lv.OBJ_FLAG_HIDDEN then
        self.sw:clear_flag(lv.OBJ_FLAG_HIDDEN)
      end
    else
      if self.sw.add_flag and lv.OBJ_FLAG_HIDDEN then
        self.sw:add_flag(lv.OBJ_FLAG_HIDDEN)
      end
    end
  end

  -- ---------------- 创建 LVGL 对象 ----------------
  function self.draw()
    self.sw = lv.switch_create(parent)
    self.sw:set_pos(self.props.x, self.props.y)
    self.sw:set_size(self.props.width, self.props.height)

    apply_off_bg()
    apply_on_bg()
    apply_knob_color()
    apply_border()
    apply_radius()
    apply_checked_state()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  local function fire_event(event_name, event)
    local list = self._event_listeners[event_name]
    if not list then return end
    for _, callback in ipairs(list) do
      local ok, err = pcall(callback, self, event)
      if not ok then print("[switch] callback error:", err) end
    end
  end

  -- ---------------- 事件订阅 ----------------
  function self:on(event_name, callback)
    if type(callback) ~= "function" then return end

    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        if event_name == "value_changed" or event_name == "clicked" then
          local state_checked = (lv.STATE_CHECKED or 0x0001)
          if self.sw and self.sw.has_state then
            self.props.checked = self.sw:has_state(state_checked) and true or false
          end
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[switch] callback error:", err) end
      end
    end

    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "pressed"       then ev_code = lv.EVENT_PRESSED
    elseif event_name == "released"      then ev_code = lv.EVENT_RELEASED
    elseif event_name == "long_pressed"  then ev_code = lv.EVENT_LONG_PRESSED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[switch] unsupported event:", event_name)
      return
    end

    self._event_listeners[event_name] = self._event_listeners[event_name] or {}
    table.insert(self._event_listeners[event_name], callback)

    local cb = make_safe_cb()
    if self.sw.add_event_cb then
      self.sw:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.sw, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  -- ---------------- 属性读写 ----------------
  function self:get_property(name)
    return self.props[name]
  end

  function self:set_property(name, value)
    local old_checked = self.props.checked
    if name == "checked" then value = to_boolean(value) end
    self.props[name] = value

    if name == "off_bg_color" then
      apply_off_bg()
    elseif name == "on_bg_color" then
      apply_on_bg()
    elseif name == "knob_color" then
      apply_knob_color()
    elseif name == "border_color" or name == "border_width" then
      apply_border()
    elseif name == "track_radius_ratio" or name == "knob_radius_ratio" then
      apply_radius()
    elseif name == "x" or name == "y" then
      if self.sw then self.sw:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.sw then
        self.sw:set_size(self.props.width, self.props.height)
        apply_radius() -- 圆角依赖 height
      end
    elseif name == "checked" then
      self._suppress_event = true
      apply_checked_state()
      self._suppress_event = false
      if old_checked ~= self.props.checked then
        fire_event("value_changed", { name = "value_changed", value = self.props.checked })
      end
    elseif name == "enabled" then
      apply_enabled_state()
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

  -- ---------------- 销毁 ----------------
  function self:destroy()
    self._cb_handles = {}
    self._event_listeners = {}
    if self.sw and self.sw.del then
      self.sw:del()
    elseif self.sw and lv.obj_del then
      lv.obj_del(self.sw)
    end
    self.sw = nil
  end

  return self
end

return Switch
