-- 带元数据的按钮示例，演示如何将属性暴露给编辑器
local lv     = require("lvgl")
local common = require("common.common")

local Button = {}

Button.__widget_meta = {
  -- ===== 控件标识 =====
  id             = "lv_button_basic",
  name           = "Button",
  description    = "示例按钮，包含 label 与尺寸/位置/样式属性",
  category       = "基础对象",
  icon           = "img/widgets/button.png",
  preview_image  = "img/widgets/button.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "button", "基础", "交互" },

  -- ===== 渲染策略 =====
  -- type        : 真正的 LVGL 对象类型（运行时使用）
  -- render_mode : Qt 设计器端渲染策略
  --   builtin -> Qt 用内置渲染器按 type 绘制
  --   custom  -> Qt 解析 self.draw() 或使用 draw_hints 通用绘制
  --   image   -> Qt 直接用 preview_image 占位
  type        = "lv_button",
  render_mode = "builtin",

  -- ===== 尺寸约束 =====
  default_size = { w = 120, h = 44 },
  min_size     = { w = 20,  h = 20 },
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
    { group = "布局", name = "x",      type = "number",  default = 0,   label = "X",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 X 坐标" },
    { group = "布局", name = "y",      type = "number",  default = 0,   label = "Y",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 Y 坐标" },
    { group = "布局", name = "width",  type = "number",  default = 120, label = "宽度", unit = "px",
      min = 1, max = 4096 },
    { group = "布局", name = "height", type = "number",  default = 44,  label = "高度", unit = "px",
      min = 1, max = 4096 },

    -- 外观组
    { group = "外观", name = "label",        type = "string", default = "OK",      label = "文本",
      bindable = true, description = "按钮显示文本，可绑定到变量" },
    { group = "外观", name = "color",        type = "color",  default = "#ffffff", label = "文本颜色" },
    { group = "外观", name = "bg_color",     type = "color",  default = "#007acc", label = "背景色" },
    { group = "外观", name = "font_size",    type = "number", default = 16,        label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family",  type = "string", default = "default", label = "字体",
      advanced = true },
    { group = "外观", name = "alignment",    type = "enum",   default = "center",  label = "对齐方式",
      options = {
        { value = "left",   label = "左对齐" },
        { value = "center", label = "居中"   },
        { value = "right",  label = "右对齐" },
      } },
    { group = "外观", name = "radius",       type = "number", default = 4,         label = "圆角",
      unit = "px", min = 0, max = 64 },
    { group = "外观", name = "border_color", type = "color",  default = "#005a9e", label = "边框色",
      advanced = true },
    { group = "外观", name = "border_width", type = "number", default = 0,         label = "边框宽度",
      unit = "px", min = 0, max = 16, advanced = true },

    -- 状态样式
    { group = "状态", name = "pressed_bg_color",  type = "color", default = "#005a9e", label = "按下背景色",
      advanced = true },
    { group = "状态", name = "disabled_bg_color", type = "color", default = "#888888", label = "禁用背景色",
      advanced = true },

    -- 行为组
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true, description = "禁用时按钮变灰且不响应事件" },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  -- ===== 数据绑定声明 =====
  -- 显式列出"可与外部变量(Tag)绑定"的属性及方向
  --   in  : 变量值 -> 控件属性
  --   out : 控件事件 -> 变量
  bindings = {
    { name = "label",   target = "text",    direction = "in"  },
    { name = "enabled", target = "enabled", direction = "in"  },
    { name = "visible", target = "visible", direction = "in"  },
    { name = "clicked", target = "trigger", direction = "out" },
  },

  -- ===== 事件定义（结构化） =====
  events = {
    { name = "clicked",        label = "点击",
      description = "按钮被释放时触发（按下后抬起即触发）",
      params = {} },
    { name = "single_clicked", label = "单击",
      description = "确认为单击（与双击互斥）后触发，存在轻微延迟",
      params = {} },
    { name = "double_clicked", label = "双击",
      description = "在短时间内连续两次点击时触发",
      params = {} },
  },

  -- ===== 事件处理代码（编辑器中用代码编辑器编辑） =====
  event_properties = {
    { name = "on_clicked_handler",        type = "code", language = "lua",
      event = "clicked",        label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      description = "点击按钮时执行的 Lua 代码",
      snippet = "-- self 为按钮实例，可访问 self.props\nprint('clicked')" },
    { name = "on_single_clicked_handler", type = "code", language = "lua",
      event = "single_clicked", label = "单击处理代码",
      default = "", multiline = true, lines = 6,
      description = "单击按钮时执行的 Lua 代码" },
    { name = "on_double_clicked_handler", type = "code", language = "lua",
      event = "double_clicked", label = "双击处理代码",
      default = "", multiline = true, lines = 6,
      description = "双击按钮时执行的 Lua 代码" },
  },

  -- ===== 自定义渲染提示（供 render_mode = "custom" 时 Qt 端通用绘制使用） =====
  draw_hints = {
    shape              = "rounded_rect", -- rect / rounded_rect / ellipse / path / image
    fill_from          = "bg_color",
    text_from          = "label",
    text_color_from    = "color",
    font_size_from     = "font_size",
    font_family_from   = "font_family",
    align_from         = "alignment",
    radius_from        = "radius",
    border_color_from  = "border_color",
    border_width_from  = "border_width",
    pressed_fill_from  = "pressed_bg_color",
    disabled_fill_from = "disabled_bg_color",
  },
}

-- =====================================================================
-- 工厂方法 new(parent, state)
-- =====================================================================
function Button.new(parent, state)
  state = state or {}
  local self = {}

  local function alignment_to_lv(align)
    if align == "left"  then return lv.TEXT_ALIGN_LEFT  or 0 end
    if align == "right" then return lv.TEXT_ALIGN_RIGHT or 2 end
    return lv.TEXT_ALIGN_CENTER or 1
  end

  -- ---------------- 初始化属性 ----------------
  self.props = {}
  for _, p in ipairs(Button.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  -- 事件回调缓存（供 destroy 清理）
  self._cb_handles = {}

  -- ---------------- 样式应用（集中管理） ----------------
  local function apply_bg_color(col)
    if self.btn and self.btn.set_style_bg_color then
      self.btn:set_style_bg_color(col, 0)
    elseif lv.obj_set_style_bg_color then
      lv.obj_set_style_bg_color(self.btn, col, 0)
    end
  end

  local function apply_text_color(col)
    if self.label and self.label.set_style_text_color then
      self.label:set_style_text_color(col, 0)
    end
  end

  local function apply_font_size()
    local size = self.props.font_size
    if self.label and self.label.set_style_text_font and lv.font_get_by_size then
      local f = lv.font_get_by_size(size)
      if f then self.label:set_style_text_font(f, 0) end
    end
  end

  local function apply_alignment()
    if self.label and self.label.set_style_text_align then
      self.label:set_style_text_align(alignment_to_lv(self.props.alignment), 0)
    end
  end

  local function apply_radius()
    if self.btn and self.btn.set_style_radius then
      self.btn:set_style_radius(self.props.radius or 0, 0)
    end
  end

  local function apply_border()
    if self.btn and self.btn.set_style_border_width then
      self.btn:set_style_border_width(self.props.border_width or 0, 0)
    end
    if self.btn and self.btn.set_style_border_color then
      self.btn:set_style_border_color(common.colorToNumber(self.props.border_color, 0x005a9e), 0)
    end
  end

  local function apply_visible()
    if not self.btn then return end
    if self.props.visible then
      if self.btn.clear_flag and lv.OBJ_FLAG_HIDDEN then
        self.btn:clear_flag(lv.OBJ_FLAG_HIDDEN)
      end
    else
      if self.btn.add_flag and lv.OBJ_FLAG_HIDDEN then
        self.btn:add_flag(lv.OBJ_FLAG_HIDDEN)
      end
    end
  end

  local function apply_enabled_visual()
    local col
    if self.props.enabled then
      col = common.colorToNumber(self.props.bg_color, 0x007acc)
    else
      col = common.colorToNumber(self.props.disabled_bg_color, 0x888888)
    end
    apply_bg_color(col)
  end

  -- ---------------- 创建 LVGL 对象 ----------------
  function self.draw()
    self.btn = lv.btn_create(parent)
    self.btn:set_size(self.props.width, self.props.height)
    self.btn:set_pos(self.props.x, self.props.y)

    self.label = lv.label_create(self.btn)
    self.label:set_text(self.props.label)
    self.label:center()

    -- 应用全部初始样式
    apply_enabled_visual()
    apply_text_color(common.colorToNumber(self.props.color, 0xffffff))
    apply_font_size()
    apply_alignment()
    apply_radius()
    apply_border()
    apply_visible()
  end
  self.draw()

  -- ---------------- 事件订阅 ----------------
  -- 统一接口：obj:on(event_name, callback)
  -- callback(self, e) 会在事件触发时被调用
  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[button] callback error:", err) end
      end
    end

    local ev_code
    if     event_name == "clicked"             then ev_code = lv.EVENT_CLICKED
    elseif event_name == "short_clicked"       then ev_code = lv.EVENT_SHORT_CLICKED
    elseif event_name == "single_clicked"      then ev_code = lv.EVENT_SINGLE_CLICKED
    elseif event_name == "double_clicked"      then ev_code = lv.EVENT_DOUBLE_CLICKED
    elseif event_name == "pressed"             then ev_code = lv.EVENT_PRESSED
    elseif event_name == "pressing"            then ev_code = lv.EVENT_PRESSING
    elseif event_name == "press_lost"          then ev_code = lv.EVENT_PRESS_LOST
    elseif event_name == "released"            then ev_code = lv.EVENT_RELEASED
    elseif event_name == "value_changed"       then ev_code = lv.EVENT_VALUE_CHANGED
    elseif event_name == "long_pressed"        then ev_code = lv.EVENT_LONG_PRESSED
    elseif event_name == "long_pressed_repeat" then ev_code = lv.EVENT_LONG_PRESSED_REPEAT
    elseif event_name == "scroll"              then ev_code = lv.EVENT_SCROLL
    elseif event_name == "scroll_begin"        then ev_code = lv.EVENT_SCROLL_BEGIN
    elseif event_name == "scroll_end"          then ev_code = lv.EVENT_SCROLL_END
    elseif event_name == "focused"             then ev_code = lv.EVENT_FOCUSED
    elseif event_name == "defocused"           then ev_code = lv.EVENT_DEFOCUSED
    elseif event_name == "leave"               then ev_code = lv.EVENT_LEAVE
    elseif event_name == "hit_test"            then ev_code = lv.EVENT_HIT_TEST
    elseif event_name == "key"                 then ev_code = lv.EVENT_KEY
    else
      print("[button] unsupported event:", event_name)
      return
    end

    if ev_code == nil then
      print("[button] lvgl event constant unavailable:", event_name)
      return
    end

    local cb = make_safe_cb()
    if self.btn.add_event_cb then
      self.btn:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.btn, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  -- ---------------- 属性读写 ----------------
  function self:get_property(name)
    return self.props[name]
  end

  function self:set_property(name, value)
    self.props[name] = value

    if name == "label" then
      if self.label and self.label.set_text then
        self.label:set_text(value)
      end
    elseif name == "color" then
      apply_text_color(common.colorToNumber(value, 0xffffff))
    elseif name == "bg_color" or name == "disabled_bg_color" then
      apply_enabled_visual()
    elseif name == "font_size" then
      apply_font_size()
    elseif name == "alignment" then
      apply_alignment()
    elseif name == "radius" then
      apply_radius()
    elseif name == "border_color" or name == "border_width" then
      apply_border()
    elseif name == "x" or name == "y" then
      if self.btn then self.btn:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.btn then self.btn:set_size(self.props.width, self.props.height) end
    elseif name == "enabled" then
      apply_enabled_visual()
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

  -- ---------------- 销毁：释放 LVGL 对象与回调 ----------------
  function self:destroy()
    self._cb_handles = {}
    if self.btn and self.btn.del then
      self.btn:del()
    elseif self.btn and lv.obj_del then
      lv.obj_del(self.btn)
    end
    self.btn   = nil
    self.label = nil
  end

  return self
end

return Button
