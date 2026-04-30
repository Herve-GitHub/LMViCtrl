-- 带元数据的标签示例，演示如何将属性暴露给编辑器
local lv     = require("lvgl")
local common = require("common.common")

local Label = {}

Label.__widget_meta = {
  -- ===== 控件标识 =====
  id             = "lv_label_basic",
  name           = "Label",
  description    = "示例标签，用于显示静态或绑定文本",
  category       = "基础控件",
  icon           = "img/widgets/label.png",
  preview_image  = "img/widgets/label.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "label", "文本", "基础" },

  -- ===== 渲染策略 =====
  -- type        : 真正的 LVGL 对象类型（运行时使用）
  -- render_mode : Qt 设计器端渲染策略
  --   builtin -> Qt 用内置渲染器按 type 绘制
  --   custom  -> Qt 解析 self.draw() 或使用 draw_hints 通用绘制
  --   image   -> Qt 直接用 preview_image 占位
  type        = "lv_label",
  render_mode = "builtin",

  -- ===== 尺寸约束 =====
  default_size = { w = 120, h = 28 },
  min_size     = { w = 8,   h = 8  },
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
      description = "为 0 时自动按内容宽度（配合 long_mode = wrap 时建议给定宽度）" },
    { group = "布局", name = "height", type = "number", default = 28,  label = "高度", unit = "px",
      min = 1, max = 4096 },

    -- 外观组
    { group = "外观", name = "text",        type = "string", default = "Label",   label = "文本",
      bindable = true, description = "标签显示的文本，可绑定到变量",
      multiline = true, lines = 3 },
    { group = "外观", name = "color",       type = "color",  default = "#000000", label = "文本颜色" },
    { group = "外观", name = "bg_color",    type = "color",  default = "#00000000", label = "背景色",
      advanced = true, description = "默认透明背景，仅在需要色块时设置" },
    { group = "外观", name = "bg_opa",      type = "number", default = 0,         label = "背景不透明度",
      min = 0, max = 255, advanced = true,
      description = "0 完全透明，255 完全不透明（LVGL opa 范围）" },
    { group = "外观", name = "font_size",   type = "number", default = 14,        label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family", type = "string", default = "default", label = "字体",
      advanced = true },
    { group = "外观", name = "alignment",   type = "enum",   default = "left",    label = "对齐方式",
      options = {
        { value = "left",   label = "左对齐" },
        { value = "center", label = "居中"   },
        { value = "right",  label = "右对齐" },
        { value = "auto",   label = "自动"   },
      } },
    { group = "外观", name = "long_mode",   type = "enum",   default = "wrap",    label = "长文本模式",
      description = "当文本超出标签宽度时的处理方式",
      options = {
        { value = "wrap",       label = "自动换行" },
        { value = "dot",        label = "省略号"   },
        { value = "scroll",     label = "滚动"     },
        { value = "scroll_circ",label = "循环滚动" },
        { value = "clip",       label = "裁剪"     },
      } },
    { group = "外观", name = "recolor",     type = "boolean", default = false,    label = "启用着色标签",
      advanced = true,
      description = "开启后文本中的 #RRGGBB ...# 标记将被解释为局部颜色" },
    { group = "外观", name = "letter_space",type = "number",  default = 0,        label = "字间距",
      unit = "px", min = -16, max = 64, advanced = true },
    { group = "外观", name = "line_space",  type = "number",  default = 0,        label = "行间距",
      unit = "px", min = -16, max = 64, advanced = true },

    -- 行为组
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  -- ===== 数据绑定声明 =====
  bindings = {
    { name = "text",    target = "text",    direction = "in" },
    { name = "color",   target = "color",   direction = "in" },
    { name = "visible", target = "visible", direction = "in" },
  },

  -- ===== 事件定义（结构化） =====
  -- 标签默认不响应点击；如果用户给它绑定了事件回调，仍然支持
  events = {
    { name = "clicked", label = "点击",
      description = "标签被点击时触发（需开启可点击标志）",
      params = {} },
  },

  -- ===== 事件处理代码 =====
  event_properties = {
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      description = "点击标签时执行的 Lua 代码（需 enabled_click = true）",
      snippet = "-- self 为标签实例，可访问 self.props\nprint('label clicked')" },
  },

  -- ===== 自定义渲染提示（供 render_mode = "custom" 时 Qt 端通用绘制使用） =====
  draw_hints = {
    shape             = "rect",
    fill_from         = "bg_color",
    fill_opa_from     = "bg_opa",
    text_from         = "text",
    text_color_from   = "color",
    font_size_from    = "font_size",
    font_family_from  = "font_family",
    align_from        = "alignment",
    long_mode_from    = "long_mode",
    letter_space_from = "letter_space",
    line_space_from   = "line_space",
  },
}

-- =====================================================================
-- 工厂方法 new(parent, state)
-- =====================================================================
function Label.new(parent, state)
  state = state or {}
  local self = {}

  local function alignment_to_lv(align)
    if align == "right"  then return lv.TEXT_ALIGN_RIGHT  or 2 end
    if align == "center" then return lv.TEXT_ALIGN_CENTER or 1 end
    if align == "auto"   then return lv.TEXT_ALIGN_AUTO   or 3 end
    return lv.TEXT_ALIGN_LEFT or 0
  end

  local function long_mode_to_lv(mode)
    if mode == "dot"         then return lv.LABEL_LONG_DOT          or 1 end
    if mode == "scroll"      then return lv.LABEL_LONG_SCROLL       or 2 end
    if mode == "scroll_circ" then return lv.LABEL_LONG_SCROLL_CIRC  or 3 end
    if mode == "clip"        then return lv.LABEL_LONG_CLIP         or 4 end
    return lv.LABEL_LONG_WRAP or 0
  end

  -- ---------------- 初始化属性 ----------------
  self.props = {}
  for _, p in ipairs(Label.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  -- 事件回调缓存（供 destroy 清理）
  self._cb_handles = {}

  -- ---------------- 样式应用（集中管理） ----------------
  local function apply_text_color(col)
    if self.label and self.label.set_style_text_color then
      self.label:set_style_text_color(col, 0)
    end
  end

  local function apply_bg()
    if not self.label then return end
    if self.label.set_style_bg_color then
      self.label:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0x000000), 0)
    end
    if self.label.set_style_bg_opa then
      self.label:set_style_bg_opa(self.props.bg_opa or 0, 0)
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

  local function apply_long_mode()
    if self.label and self.label.set_long_mode then
      self.label:set_long_mode(long_mode_to_lv(self.props.long_mode))
    end
  end

  local function apply_recolor()
    if self.label and self.label.set_recolor then
      self.label:set_recolor(self.props.recolor and true or false)
    end
  end

  local function apply_letter_space()
    if self.label and self.label.set_style_text_letter_space then
      self.label:set_style_text_letter_space(self.props.letter_space or 0, 0)
    end
  end

  local function apply_line_space()
    if self.label and self.label.set_style_text_line_space then
      self.label:set_style_text_line_space(self.props.line_space or 0, 0)
    end
  end

  local function apply_visible()
    if not self.label then return end
    if self.props.visible then
      if self.label.clear_flag and lv.OBJ_FLAG_HIDDEN then
        self.label:clear_flag(lv.OBJ_FLAG_HIDDEN)
      end
    else
      if self.label.add_flag and lv.OBJ_FLAG_HIDDEN then
        self.label:add_flag(lv.OBJ_FLAG_HIDDEN)
      end
    end
  end

  -- ---------------- 创建 LVGL 对象 ----------------
  function self.draw()
    self.label = lv.label_create(parent)
    self.label:set_text(self.props.text)
    self.label:set_pos(self.props.x, self.props.y)
    if self.props.width and self.props.width > 0
       and self.props.height and self.props.height > 0 then
      self.label:set_size(self.props.width, self.props.height)
    end

    -- 应用全部初始样式
    apply_text_color(common.colorToNumber(self.props.color, 0x000000))
    apply_bg()
    apply_font_size()
    apply_alignment()
    apply_long_mode()
    apply_recolor()
    apply_letter_space()
    apply_line_space()
    apply_visible()
  end
  self.draw()

  -- ---------------- 事件订阅 ----------------
  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[label] callback error:", err) end
      end
    end

    local ev_code
    if event_name == "clicked" then
      ev_code = lv.EVENT_CLICKED
    else
      print("[label] unsupported event:", event_name)
      return
    end

    -- 启用点击标志
    if self.label and self.label.add_flag and lv.OBJ_FLAG_CLICKABLE then
      self.label:add_flag(lv.OBJ_FLAG_CLICKABLE)
    end

    local cb = make_safe_cb()
    if self.label.add_event_cb then
      self.label:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.label, cb, ev_code, nil)
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
      if self.label and self.label.set_text then
        self.label:set_text(value)
      end
    elseif name == "color" then
      apply_text_color(common.colorToNumber(value, 0x000000))
    elseif name == "bg_color" or name == "bg_opa" then
      apply_bg()
    elseif name == "font_size" then
      apply_font_size()
    elseif name == "alignment" then
      apply_alignment()
    elseif name == "long_mode" then
      apply_long_mode()
    elseif name == "recolor" then
      apply_recolor()
    elseif name == "letter_space" then
      apply_letter_space()
    elseif name == "line_space" then
      apply_line_space()
    elseif name == "x" or name == "y" then
      if self.label then self.label:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.label and self.props.width > 0 and self.props.height > 0 then
        self.label:set_size(self.props.width, self.props.height)
      end
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
    if self.label and self.label.del then
      self.label:del()
    elseif self.label and lv.obj_del then
      lv.obj_del(self.label)
    end
    self.label = nil
  end

  return self
end

return Label
