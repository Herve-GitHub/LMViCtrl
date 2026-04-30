-- 带元数据的下拉框示例
local lv     = require("lvgl")
local common = require("common.common")

local Dropdown = {}

Dropdown.__widget_meta = {
  id             = "lv_dropdown_basic",
  name           = "Dropdown",
  description    = "示例下拉框，从一组选项中选择一个",
  category       = "交互输入",
  icon           = "img/widgets/dropdown.png",
  preview_image  = "img/widgets/dropdown.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "dropdown", "下拉", "选择", "交互" },

  type        = "lv_dropdown",
  render_mode = "custom",

  default_size = { w = 160, h = 40 },
  min_size     = { w = 32,  h = 20 },
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
    { group = "布局", name = "width",  type = "number", default = 160, label = "宽度", unit = "px",
      min = 16, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 40,  label = "高度", unit = "px",
      min = 16, max = 4096 },

    -- 数据
    { group = "数据", name = "options", type = "string", default = "Option 1\nOption 2\nOption 3",
      label = "选项列表", multiline = true, lines = 6,
      description = "每行一个选项" },
    { group = "数据", name = "selected", type = "number", default = 0, label = "选中索引",
      min = 0, max = 999, bindable = true,
      description = "当前选中项的下标(从 0 开始)" },

    -- 行为
    { group = "行为", name = "direction", type = "enum", default = "down", label = "展开方向",
      options = {
        { value = "down",  label = "向下" },
        { value = "up",    label = "向上" },
        { value = "left",  label = "向左" },
        { value = "right", label = "向右" },
      } },

    -- 外观
    { group = "外观", name = "text_color",   type = "color",  default = "#000000", label = "文本颜色" },
    { group = "外观", name = "bg_color",     type = "color",  default = "#ffffff", label = "背景色" },
    { group = "外观", name = "border_color", type = "color",  default = "#888888", label = "边框色" },
    { group = "外观", name = "border_width", type = "number", default = 1,         label = "边框宽度",
      unit = "px", min = 0, max = 8 },
    { group = "外观", name = "radius",       type = "number", default = 4,         label = "圆角",
      unit = "px", min = 0, max = 64 },
    { group = "外观", name = "font_size",    type = "number", default = 14,        label = "字体大小",
      unit = "px", min = 8, max = 96 },
    { group = "外观", name = "font_family",  type = "string", default = "default", label = "字体",
      advanced = true },
    { group = "外观", name = "arrow_color",  type = "color",  default = "#666666", label = "箭头颜色",
      advanced = true },

    -- 列表外观（展开后的下拉列表）
    { group = "列表外观", name = "list_bg_color",     type = "color", default = "#ffffff", label = "列表背景色",
      advanced = true },
    { group = "列表外观", name = "list_text_color",   type = "color", default = "#000000", label = "列表文本色",
      advanced = true },
    { group = "列表外观", name = "list_sel_bg_color", type = "color", default = "#007acc", label = "选中项背景色",
      advanced = true },
    { group = "列表外观", name = "list_sel_text_color", type = "color", default = "#ffffff", label = "选中项文字色",
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
      description = "下拉框被点击时触发", params = {} },
    { name = "value_changed", label = "选择改变",
      description = "选中项发生变化时触发",
      params = {
        { name = "selected", type = "number", description = "新选中下标" },
        { name = "text",     type = "string", description = "新选中项的文本" },
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
    shape              = "dropdown",
    options_from       = "options",
    selected_from      = "selected",
    text_color_from    = "text_color",
    fill_from          = "bg_color",
    border_color_from  = "border_color",
    border_width_from  = "border_width",
    radius_from        = "radius",
    font_size_from     = "font_size",
    font_family_from   = "font_family",
    arrow_color_from   = "arrow_color",
    enabled_from       = "enabled",
  },
}

function Dropdown.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Dropdown.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles    = {}
  self._suppress_event = false

  local function PART_MAIN()      return lv.PART_MAIN      or 0x00000 end
  local function PART_INDICATOR() return lv.PART_INDICATOR or 0x20000 end
  local function PART_SELECTED()  return lv.PART_SELECTED  or 0x40000 end

  local function dir_to_lv(d)
    if d == "up"    then return lv.DIR_TOP    or 4 end
    if d == "left"  then return lv.DIR_LEFT   or 1 end
    if d == "right" then return lv.DIR_RIGHT  or 2 end
    return lv.DIR_BOTTOM or 8
  end

  local function apply_options()
    if self.dd and self.dd.set_options then
      self.dd:set_options(self.props.options or "")
    end
  end
  local function apply_selected()
    if self.dd and self.dd.set_selected then
      self.dd:set_selected(self.props.selected or 0)
    end
  end
  local function apply_direction()
    if self.dd and self.dd.set_dir then
      self.dd:set_dir(dir_to_lv(self.props.direction))
    end
  end
  local function apply_text_color()
    if self.dd and self.dd.set_style_text_color then
      self.dd:set_style_text_color(common.colorToNumber(self.props.text_color, 0x000000), PART_MAIN())
    end
  end
  local function apply_bg()
    if self.dd and self.dd.set_style_bg_color then
      self.dd:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0xffffff), PART_MAIN())
    end
  end
  local function apply_border()
    if not self.dd then return end
    if self.dd.set_style_border_width then
      self.dd:set_style_border_width(self.props.border_width or 0, PART_MAIN())
    end
    if self.dd.set_style_border_color then
      self.dd:set_style_border_color(common.colorToNumber(self.props.border_color, 0x888888), PART_MAIN())
    end
  end
  local function apply_radius()
    if self.dd and self.dd.set_style_radius then
      self.dd:set_style_radius(self.props.radius or 0, PART_MAIN())
    end
  end
  local function apply_font_size()
    if self.dd and self.dd.set_style_text_font and lv.font_get_by_size then
      local f = lv.font_get_by_size(self.props.font_size)
      if f then self.dd:set_style_text_font(f, PART_MAIN()) end
    end
  end
  local function apply_arrow()
    if self.dd and self.dd.set_style_text_color then
      self.dd:set_style_text_color(common.colorToNumber(self.props.arrow_color, 0x666666), PART_INDICATOR())
    end
  end
  -- 列表（list）样式作用在 dd:get_list() 上
  local function apply_list_styles()
    if not self.dd or not self.dd.get_list then return end
    local list = self.dd:get_list()
    if not list then return end
    if list.set_style_bg_color then
      list:set_style_bg_color(common.colorToNumber(self.props.list_bg_color, 0xffffff), PART_MAIN())
    end
    if list.set_style_text_color then
      list:set_style_text_color(common.colorToNumber(self.props.list_text_color, 0x000000), PART_MAIN())
      list:set_style_text_color(common.colorToNumber(self.props.list_sel_text_color, 0xffffff), PART_SELECTED())
    end
    if list.set_style_bg_color then
      list:set_style_bg_color(common.colorToNumber(self.props.list_sel_bg_color, 0x007acc), PART_SELECTED())
    end
  end
  local function apply_enabled_state()
    if not self.dd then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.dd.clear_state then self.dd:clear_state(sd) end
    else
      if self.dd.add_state   then self.dd:add_state(sd) end
    end
  end
  local function apply_visible()
    if not self.dd then return end
    if self.props.visible then
      if self.dd.clear_flag and lv.OBJ_FLAG_HIDDEN then self.dd:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.dd.add_flag   and lv.OBJ_FLAG_HIDDEN then self.dd:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end

  function self.draw()
    self.dd = lv.dropdown_create(parent)
    self.dd:set_pos(self.props.x, self.props.y)
    self.dd:set_size(self.props.width, self.props.height)
    apply_options()
    apply_selected()
    apply_direction()
    apply_text_color()
    apply_bg()
    apply_border()
    apply_radius()
    apply_font_size()
    apply_arrow()
    apply_list_styles()
    apply_enabled_state()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function make_safe_cb()
      return function(e)
        if not self.props.enabled then return end
        if event_name == "value_changed" then
          if self.dd and self.dd.get_selected then
            self.props.selected = self.dd:get_selected() or 0
          end
        end
        if self._suppress_event then return end
        local ok, err = pcall(callback, self, e)
        if not ok then print("[dropdown] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else
      print("[dropdown] unsupported event:", event_name)
      return
    end
    local cb = make_safe_cb()
    if self.dd.add_event_cb then
      self.dd:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then
      lv.obj_add_event_cb(self.dd, cb, ev_code, nil)
    end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "options" then apply_options(); apply_selected()
    elseif name == "selected"     then
      self._suppress_event = true; apply_selected(); self._suppress_event = false
    elseif name == "direction"    then apply_direction()
    elseif name == "text_color"   then apply_text_color()
    elseif name == "bg_color"     then apply_bg()
    elseif name == "border_color" or name == "border_width" then apply_border()
    elseif name == "radius"       then apply_radius()
    elseif name == "font_size"    then apply_font_size()
    elseif name == "arrow_color"  then apply_arrow()
    elseif name == "list_bg_color" or name == "list_text_color"
        or name == "list_sel_bg_color" or name == "list_sel_text_color" then
      apply_list_styles()
    elseif name == "x" or name == "y" then
      if self.dd then self.dd:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.dd then self.dd:set_size(self.props.width, self.props.height) end
    elseif name == "enabled"      then apply_enabled_state()
    elseif name == "visible"      then apply_visible()
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
    if self.dd and self.dd.del then self.dd:del()
    elseif self.dd and lv.obj_del then lv.obj_del(self.dd) end
    self.dd = nil
  end

  return self
end

return Dropdown
