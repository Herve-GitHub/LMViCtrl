-- 带元数据的复选框示例，演示如何将属性暴露给编辑器
local lv     = require("lvgl")
local common = require("common.common")

local Checkbox = {}

Checkbox.__widget_meta = {
  -- ===== 控件标识 =====
  id             = "lv_checkbox_basic",
  name           = "Checkbox",
  description    = "示例复选框，包含文本与勾选状态",
  category       = "交互输入",
  icon           = "img/widgets/checkbox.png",
  preview_image  = "img/widgets/checkbox.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "checkbox", "复选框", "交互" },

  -- ===== 渲染策略 =====
  -- type        : 真正的 LVGL 对象类型（运行时使用）
  -- render_mode : Qt 设计器端渲染策略
  --   builtin -> Qt 用内置渲染器按 type 绘制
  --   custom  -> Qt 解析 self.draw() 或使用 draw_hints 通用绘制
  --   image   -> Qt 直接用 preview_image 占位
  type        = "lv_checkbox",
  render_mode = "custom",

  -- ===== 尺寸约束 =====
  default_size = { w = 120, h = 24 },
  min_size     = { w = 16,  h = 16 },
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
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 X 坐标" },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y",    unit = "px",
      min = -4096, max = 4096, description = "相对父容器的 Y 坐标" },
    { group = "布局", name = "width",  type = "number", default = 120, label = "宽度", unit = "px",
      min = 1, max = 4096,
      description = "为 0 时自动按内容宽度" },
    { group = "布局", name = "height", type = "number", default = 24,  label = "高度", unit = "px",
      min = 1, max = 4096 },

    -- 外观组
    { group = "外观", name = "text",         type = "string", default = "Checkbox", label = "文本",
      bindable = true, description = "复选框旁显示的文本，可绑定到变量" },
    { group = "外观", name = "color",        type = "color",  default = "#000000",  label = "文本颜色" },
    { group = "外观", name = "font_size",    type = "number", default = 14,         label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family",  type = "string", default = "default",  label = "字体",
      advanced = true },
    { group = "外观", name = "box_bg_color", type = "color",  default = "#ffffff",  label = "勾选框背景色",
      advanced = true },
    { group = "外观", name = "checked_bg_color", type = "color", default = "#007acc", label = "勾选状态背景色",
      advanced = true, description = "处于 checked 状态时勾选框的背景色" },
    { group = "外观", name = "border_color", type = "color",  default = "#888888",  label = "勾选框边框色",
      advanced = true },
    { group = "外观", name = "border_width", type = "number", default = 1,          label = "勾选框边框宽度",
      unit = "px", min = 0, max = 8, advanced = true },
    { group = "外观", name = "radius",       type = "number", default = 2,          label = "勾选框圆角",
      unit = "px", min = 0, max = 32, advanced = true },

    -- 状态组
    { group = "状态", name = "checked", type = "boolean", default = false, label = "已勾选",
      bindable = true, description = "复选框当前是否被勾选" },

    -- 行为组
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true, description = "禁用时复选框变灰且不响应事件" },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  -- ===== 数据绑定声明 =====
  -- 显式列出"可与外部变量(Tag)绑定"的属性及方向
  --   in  : 变量值 -> 控件属性
  --   out : 控件事件 -> 变量
  --   inout: 双向（控件被勾选会写回变量，变量变化也会更新控件）
  bindings = {
    { name = "text",    target = "text",    direction = "in"   },
    { name = "checked", target = "value",   direction = "inout" },
    { name = "enabled", target = "enabled", direction = "in"   },
    { name = "visible", target = "visible", direction = "in"   },
    { name = "changed", target = "trigger", direction = "out"  },
  },

  -- ===== 事件定义（结构化） =====
  events = {
    { name = "clicked",     label = "点击",
      description = "复选框被释放时触发",
      params = {} },
    { name = "value_changed", label = "状态改变",
      description = "勾选状态发生变化时触发",
      params = { { name = "checked", type = "boolean", description = "新的勾选状态" } } },
  },

  -- ===== 事件处理代码（编辑器中用代码编辑器编辑） =====
  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      description = "点击复选框时执行的 Lua 代码",
      snippet = "-- self 为复选框实例\nprint('clicked')" },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "状态改变处理代码",
      default = "", multiline = true, lines = 6,
      description = "勾选状态变化时执行的 Lua 代码（self.props.checked 已更新）",
      snippet = "-- self 为复选框实例\nprint('checked =', self.props.checked)" },
  },

  -- ===== 自定义渲染提示（供 render_mode = "custom" 时 Qt 端通用绘制使用） =====
  draw_hints = {
    shape              = "checkbox", -- 提示 Qt 端按"勾选框 + 文本"组合绘制
    text_from          = "text",
    text_color_from    = "color",
    font_size_from     = "font_size",
    font_family_from   = "font_family",
    checked_from       = "checked",
    box_fill_from      = "box_bg_color",
    checked_fill_from  = "checked_bg_color",
    border_color_from  = "border_color",
    border_width_from  = "border_width",
    radius_from        = "radius",
  },
}

-- =====================================================================
-- 工厂方法 new(parent, state)
-- =====================================================================
function Checkbox.new(parent, state)
  state = state or {}
  local self = {}

  -- ---------------- 初始化属性 ----------------
  self.props = {}
  for _, p in ipairs(Checkbox.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  -- 事件回调缓存（供 destroy 清理）
  self._cb_handles = {}

  -- 内部状态切换抑制标志（避免 set_property 与事件回调相互递归）
  self._suppress_event = false

  -- ---------------- 样式应用（集中管理） ----------------
  local function apply_text_color(col)
    if self.cb and self.cb.set_style_text_color then
      -- 主部件设置文本颜色
      self.cb:set_style_text_color(col, 0)
    end
  end

  local function apply_font_size()
    local size = self.props.font_size
    if self.cb and self.cb.set_style_text_font and lv.font_get_by_size then
      local f = lv.font_get_by_size(size)
      if f then self.cb:set_style_text_font(f, 0) end
    end
  end

  -- 勾选框（indicator part）样式：背景色 / 边框 / 圆角
  -- LVGL 中复选框的勾选框是 INDICATOR part，PART 选择器在不同绑定下命名不同
  local function PART_INDICATOR()
    return lv.PART_INDICATOR or 0x20000
  end

  local function apply_box_bg()
    if not self.cb then return end
    local part = PART_INDICATOR()
    if self.cb.set_style_bg_color then
      self.cb:set_style_bg_color(common.colorToNumber(self.props.box_bg_color, 0xffffff), part)
    end
  end

  local function apply_checked_bg()
    if not self.cb then return end
    local part = PART_INDICATOR()
    -- LVGL 通过 STATE_CHECKED 选择器区分；这里简单写到 indicator 默认状态作为备用
    local state_checked = (lv.STATE_CHECKED or 0x0001)
    if self.cb.set_style_bg_color then
      self.cb:set_style_bg_color(common.colorToNumber(self.props.checked_bg_color, 0x007acc),
                                  part | state_checked)
    end
  end

  local function apply_border()
    if not self.cb then return end
    local part = PART_INDICATOR()
    if self.cb.set_style_border_width then
      self.cb:set_style_border_width(self.props.border_width or 0, part)
    end
    if self.cb.set_style_border_color then
      self.cb:set_style_border_color(common.colorToNumber(self.props.border_color, 0x888888), part)
    end
  end

  local function apply_radius()
    if not self.cb then return end
    local part = PART_INDICATOR()
    if self.cb.set_style_radius then
      self.cb:set_style_radius(self.props.radius or 0, part)
    end
  end

  local function apply_checked_state()
    if not self.cb then return end
    local state_checked = (lv.STATE_CHECKED or 0x0001)
    if self.props.checked then
      if self.cb.add_state then
        self.cb:add_state(state_checked)
      end
    else
      if self.cb.clear_state then
        self.cb:clear_state(state_checked)
      end
    end
  end

  local function apply_enabled_state()
    if not self.cb then return end
    local state_disabled = (lv.STATE_DISABLED or 0x0080)
    if self.props.enabled then
      if self.cb.clear_state then self.cb:clear_state(state_disabled) end
    else
      if self.cb.add_state then self.cb:add_state(state_disabled) end
    end
  end

  local function apply_visible()
    if not self.cb then return end
    if self.props.visible then
      if self.cb.clear_flag and lv.OBJ_FLAG_HIDDEN then
        self.cb:clear_flag(lv.OBJ_FLAG_HIDDEN)
      end
    else
      if self.cb.add_flag and lv.OBJ_FLAG_HIDDEN then
        self.cb:add_flag(lv.OBJ_FLAG_HIDDEN)
      end
    end
  end

  local function apply_text(value)
    if not self.cb then return end
    value = value or ""
    if self.cb.set_checkbox_text then
      self.cb:set_checkbox_text(value)
    elseif lv.checkbox_set_text then
      lv.checkbox_set_text(self.cb, value)
    elseif self.cb.set_text then
      self.cb:set_text(value)
    end
  end

  -- ---------------- 创建 LVGL 对象 ----------------
  function self.draw()
    self.cb = lv.checkbox_create(parent)
    apply_text(self.props.text)
    self.cb:set_pos(self.props.x, self.props.y)
    if self.props.width and self.props.width > 0
       and self.props.height and self.props.height > 0 then
      self.cb:set_size(self.props.width, self.props.height)
    end

    -- 应用全部初始样式与状态
    apply_text_color(common.colorToNumber(self.props.color, 0x000000))
    apply_font_size()
    apply_box_bg()
    apply_checked_bg()
    apply_border()
    apply_radius()
    apply_checked_state()
    apply_enabled_state()
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
        -- 同步勾选状态到 self.props
        if event_name == "value_changed" or event_name == "clicked" then
          local state_checked = (lv.STATE_CHECKED or 0x0001)
          if self.cb and self.cb.has_state then
            self.props.checked = self.cb:has_state(state_checked) and true or false
          end
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[checkbox] callback error:", err) end
      end
    end

    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[checkbox] unsupported event:", event_name)
      return
    end

    local cb = make_safe_cb()
    if self.cb.add_event_cb then
      self.cb:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.cb, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  -- ---------------- 属性读写 ----------------
  function self:get_property(name)
    return self.props[name]
  end

  function self:set_property(name, value)
    self.props[name] = value

    if name == "text" then
      apply_text(value)
    elseif name == "color" then
      apply_text_color(common.colorToNumber(value, 0x000000))
    elseif name == "font_size" then
      apply_font_size()
    elseif name == "box_bg_color" then
      apply_box_bg()
    elseif name == "checked_bg_color" then
      apply_checked_bg()
    elseif name == "border_color" or name == "border_width" then
      apply_border()
    elseif name == "radius" then
      apply_radius()
    elseif name == "x" or name == "y" then
      if self.cb then self.cb:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.cb and self.props.width > 0 and self.props.height > 0 then
        self.cb:set_size(self.props.width, self.props.height)
      end
    elseif name == "checked" then
      -- 通过外部赋值修改勾选状态时不触发用户回调
      self._suppress_event = true
      apply_checked_state()
      self._suppress_event = false
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

  -- ---------------- 销毁：释放 LVGL 对象与回调 ----------------
  function self:destroy()
    self._cb_handles = {}
    if self.cb and self.cb.delete then
      self.cb:delete()
    elseif self.cb and self.cb.del then
      self.cb:del()
    elseif self.cb and lv.obj_delete then
      lv.obj_delete(self.cb)
    elseif self.cb and lv.obj_del then
      lv.obj_del(self.cb)
    end
    self.cb = nil
  end

  return self
end

return Checkbox
