-- 带元数据的滚轮选择器示例
local lv     = require("lvgl")
local common = require("common.common")

local Roller = {}

Roller.__widget_meta = {
  id             = "lv_roller_basic",
  name           = "Roller",
  description    = "示例滚轮选择器，纵向滚动从一组选项中选择",
  category       = "交互输入",
  icon           = "img/widgets/roller.png",
  preview_image  = "img/widgets/roller.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "roller", "滚轮", "选择", "交互" },

  type        = "lv_roller",
  render_mode = "custom",

  default_size = { w = 100, h = 120 },
  min_size     = { w = 32,  h = 60  },
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
    { group = "布局", name = "width",  type = "number", default = 100, label = "宽度", unit = "px",
      min = 16, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 120, label = "高度", unit = "px",
      min = 32, max = 4096 },

    -- 数据
    { group = "数据", name = "options", type = "string",
      default = "1",
      label = "选项列表", multiline = true, lines = 8,
      description = "每行一个选项" },
    { group = "数据", name = "selected", type = "number", default = 0, label = "选中索引",
      min = 0, max = 999, bindable = true,
      description = "当前选中项的下标(从 0 开始)" },
    { group = "数据", name = "visible_row_count", type = "number", default = 3, label = "可见行数",
      min = 1, max = 20,
      description = "滚轮一次显示的行数（建议奇数，居中行为选中行）" },
    { group = "数据", name = "infinite", type = "boolean", default = false, label = "无限循环",
      description = "启用后到尾后接回到头" },

    -- 外观
    { group = "外观", name = "text_color",  type = "color",  default = "#666666", label = "未选文本色" },
    { group = "外观", name = "sel_text_color", type = "color", default = "#000000", label = "选中文本色" },
    { group = "外观", name = "bg_color",    type = "color",  default = "#ffffff", label = "背景色" },
    { group = "外观", name = "sel_bg_color", type = "color", default = "#e6f2fb", label = "选中行背景色",
      advanced = true },
    { group = "外观", name = "border_color", type = "color", default = "#cccccc", label = "边框色" },
    { group = "外观", name = "border_width", type = "number", default = 1,        label = "边框宽度",
      unit = "px", min = 0, max = 8 },
    { group = "外观", name = "radius",       type = "number", default = 4,        label = "圆角",
      unit = "px", min = 0, max = 64 },
    { group = "外观", name = "font_size",    type = "number", default = 16,       label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family",  type = "string", default = "default", label = "字体",
      advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "selected", target = "value",   direction = "inout" },
    { name = "options",  target = "options", direction = "in"    },
    { name = "enabled",  target = "enabled", direction = "in"    },
    { name = "visible",  target = "visible", direction = "in"    },
    { name = "changed",  target = "trigger", direction = "out"   },
  },

  events = {
    { name = "clicked",       label = "点击",
      description = "滚轮被点击时触发", params = {} },
    { name = "value_changed", label = "选择改变",
      description = "选中项发生变化时触发",
      params = {
        { name = "selected", type = "number", description = "新选中下标" },
      } },
  },

  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "选择改变处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('selected =', self.props.selected)" },
  },

  draw_hints = {
    shape                  = "roller",
    options_from           = "options",
    selected_from          = "selected",
    visible_row_count_from = "visible_row_count",
    text_color_from        = "text_color",
    sel_text_color_from    = "sel_text_color",
    fill_from              = "bg_color",
    sel_fill_from          = "sel_bg_color",
    border_color_from      = "border_color",
    border_width_from      = "border_width",
    radius_from            = "radius",
    font_size_from         = "font_size",
    font_family_from       = "font_family",
    enabled_from           = "enabled",
  },
}

function Roller.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Roller.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()     return lv.PART_MAIN     or 0x00000 end
  local function PART_SELECTED() return lv.PART_SELECTED or 0x40000 end

  local function mode_to_lv()
    if self.props.infinite then return lv.ROLLER_MODE_INFINITE or 1 end
    return lv.ROLLER_MODE_NORMAL or 0
  end

  local function apply_options()
    if self.rl and self.rl.set_options then
      self.rl:set_options(self.props.options or "", mode_to_lv())
    end
  end
  local function apply_selected()
    if self.rl and self.rl.set_selected then
      self.rl:set_selected(self.props.selected or 0, lv.ANIM_OFF or 0)
    end
  end
  local function apply_visible_rows()
    if self.rl and self.rl.set_visible_row_count then
      self.rl:set_visible_row_count(self.props.visible_row_count or 3)
    end
  end
  local function apply_text_color()
    if self.rl and self.rl.set_style_text_color then
      self.rl:set_style_text_color(common.colorToNumber(self.props.text_color, 0x666666), PART_MAIN())
      self.rl:set_style_text_color(common.colorToNumber(self.props.sel_text_color, 0x000000), PART_SELECTED())
    end
  end
  local function apply_bg()
    if self.rl and self.rl.set_style_bg_color then
      self.rl:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0xffffff), PART_MAIN())
      self.rl:set_style_bg_color(common.colorToNumber(self.props.sel_bg_color, 0xe6f2fb), PART_SELECTED())
    end
  end
  local function apply_border()
    if not self.rl then return end
    if self.rl.set_style_border_width then
      self.rl:set_style_border_width(self.props.border_width or 0, PART_MAIN())
    end
    if self.rl.set_style_border_color then
      self.rl:set_style_border_color(common.colorToNumber(self.props.border_color, 0xcccccc), PART_MAIN())
    end
  end
  local function apply_radius()
    if self.rl and self.rl.set_style_radius then
      self.rl:set_style_radius(self.props.radius or 0, PART_MAIN())
    end
  end
  local function apply_font_size()
    if self.rl and self.rl.set_style_text_font and lv.font_get_by_size then
      local f = lv.font_get_by_size(self.props.font_size)
      if f then self.rl:set_style_text_font(f, PART_MAIN()) end
    end
  end
  local function apply_enabled_state()
    if not self.rl then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.rl.clear_state then self.rl:clear_state(sd) end
    else
      if self.rl.add_state   then self.rl:add_state(sd) end
    end
  end
  local function apply_visible()
    if not self.rl then return end
    if self.props.visible then
      if self.rl.clear_flag and lv.OBJ_FLAG_HIDDEN then self.rl:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.rl.add_flag   and lv.OBJ_FLAG_HIDDEN then self.rl:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.rl = lv.roller_create(parent)
    self.rl:set_pos(self.props.x, self.props.y)
    self.rl:set_size(self.props.width, self.props.height)
    apply_options()
    apply_selected()
    apply_visible_rows()
    apply_text_color()
    apply_bg()
    apply_border()
    apply_radius()
    apply_font_size()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        if event_name == "value_changed" then
          if self.rl and self.rl.get_selected then
            self.props.selected = self.rl:get_selected() or 0
          end
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[roller] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[roller] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.rl.add_event_cb then
      self.rl:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.rl, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "options" or name == "infinite" then apply_options(); apply_selected()
    elseif name == "selected" then
      self._suppress_event = true; apply_selected(); self._suppress_event = false
    elseif name == "visible_row_count" then apply_visible_rows()
    elseif name == "text_color" or name == "sel_text_color" then apply_text_color()
    elseif name == "bg_color"   or name == "sel_bg_color"   then apply_bg()
    elseif name == "border_color" or name == "border_width" then apply_border()
    elseif name == "radius"     then apply_radius()
    elseif name == "font_size"  then apply_font_size()
    elseif name == "x" or name == "y" then
      if self.rl then self.rl:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.rl then self.rl:set_size(self.props.width, self.props.height) end
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
    if self.rl and self.rl.del then self.rl:del()
    elseif self.rl and lv.obj_del then lv.obj_del(self.rl) end
    self.rl = nil
  end

  return self
end

return Roller
