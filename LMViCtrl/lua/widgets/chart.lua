-- 带元数据的图表示例
local lv     = require("lvgl")
local common = require("common.common")

local Chart = {}

Chart.__widget_meta = {
  id             = "lv_chart_basic",
  name           = "Chart",
  description    = "示例图表，支持折线/柱状/散点；多数据序列",
  category       = "数据展示",
  icon           = "img/widgets/chart.png",
  preview_image  = "img/widgets/chart.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "chart", "图表", "折线", "柱状" },

  type        = "lv_chart",
  render_mode = "custom",

  default_size = { w = 280, h = 180 },
  min_size     = { w = 80,  h = 60  },
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
    { group = "布局", name = "width",  type = "number", default = 280, label = "宽度", unit = "px",
      min = 60, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 180, label = "高度", unit = "px",
      min = 60, max = 4096 },

    -- 类型与范围
    { group = "图表", name = "chart_type", type = "enum", default = "line", label = "类型",
      options = {
        { value = "line",    label = "折线图" },
        { value = "bar",     label = "柱状图" },
        { value = "scatter", label = "散点图" },
      } },
    { group = "图表", name = "point_count", type = "number", default = 8,  label = "采样点数",
      min = 2, max = 256 },
    { group = "图表", name = "y_min",       type = "number", default = 0,   label = "Y 最小值" },
    { group = "图表", name = "y_max",       type = "number", default = 100, label = "Y 最大值" },

    -- 网格
    { group = "网格", name = "div_x", type = "number", default = 4, label = "纵向网格数",
      min = 0, max = 32 },
    { group = "网格", name = "div_y", type = "number", default = 4, label = "横向网格数",
      min = 0, max = 32 },
    { group = "网格", name = "show_grid",  type = "boolean", default = true, label = "显示网格" },
    { group = "网格", name = "grid_color", type = "color",   default = "#dde2e8", label = "网格颜色" },

    -- 数据序列
    { group = "数据", name = "series1_data", type = "string",
      default = "1",
      label = "序列1 数据(逗号分隔)", multiline = true, lines = 3, bindable = true },
    { group = "数据", name = "series1_color", type = "color", default = "#1976d2",
      label = "序列1 颜色" },
    { group = "数据", name = "series2_data", type = "string",
      default = "",
      label = "序列2 数据(可选)", multiline = true, lines = 3, bindable = true,
      description = "留空则不显示", advanced = true },
    { group = "数据", name = "series2_color", type = "color", default = "#e53935",
      label = "序列2 颜色", advanced = true },

    -- 外观
    { group = "外观", name = "bg_color",   type = "color",   default = "#ffffff", label = "背景色" },
    { group = "外观", name = "axis_color", type = "color",   default = "#7a8290", label = "坐标轴颜色" },
    { group = "外观", name = "line_width", type = "number",  default = 2, label = "折线粗细",
      unit = "px", min = 1, max = 16 },
    { group = "外观", name = "point_size", type = "number",  default = 3, label = "数据点大小",
      unit = "px", min = 0, max = 32 },
    { group = "外观", name = "show_points", type = "boolean", default = true, label = "显示数据点" },
    { group = "外观", name = "fill_area",   type = "boolean", default = false, label = "区域填充",
      advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "series1_data", target = "value",   direction = "in" },
    { name = "series2_data", target = "value",   direction = "in" },
    { name = "enabled",      target = "enabled", direction = "in" },
    { name = "visible",      target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "数据改变", params = {} },
    { name = "clicked",       label = "点击",     params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "数据改变代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape             = "chart",
    chart_type_from   = "chart_type",
    point_count_from  = "point_count",
    y_min_from        = "y_min",
    y_max_from        = "y_max",
    div_x_from        = "div_x",
    div_y_from        = "div_y",
    show_grid_from    = "show_grid",
    grid_color_from   = "grid_color",
    series1_data_from = "series1_data",
    series1_color_from = "series1_color",
    series2_data_from = "series2_data",
    series2_color_from = "series2_color",
    bg_color_from     = "bg_color",
    axis_color_from   = "axis_color",
    line_width_from   = "line_width",
    point_size_from   = "point_size",
    show_points_from  = "show_points",
    fill_area_from    = "fill_area",
    enabled_from      = "enabled",
  },
}

local function parse_series(text, n)
  local out = {}
  if type(text) ~= "string" or text == "" then return out end
  for v in (text .. ","):gmatch("([^,]*),") do
    local s = v:gsub("%s", "")
    if s ~= "" then
      local num = tonumber(s)
      if num then table.insert(out, num) end
    end
  end
  return out
end

function Chart.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Chart.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}
  self._series = {}

  local function chart_type_to_lv(t)
    if t == "bar"     then return lv.CHART_TYPE_BAR     or 2 end
    if t == "scatter" then return lv.CHART_TYPE_SCATTER or 3 end
    return lv.CHART_TYPE_LINE or 1
  end

  local function apply_type()
    if self.chart and self.chart.set_type then
      self.chart:set_type(chart_type_to_lv(self.props.chart_type))
    end
  end
  local function apply_range()
    if self.chart and self.chart.set_range then
      local AXIS_PRIMARY_Y = lv.CHART_AXIS_PRIMARY_Y or 0
      self.chart:set_range(AXIS_PRIMARY_Y, self.props.y_min, self.props.y_max)
    end
  end
  local function apply_grid()
    if self.chart and self.chart.set_div_line_count then
      local div_y = self.props.show_grid and self.props.div_y or 0
      local div_x = self.props.show_grid and self.props.div_x or 0
      self.chart:set_div_line_count(div_y, div_x)
    end
  end
  local function apply_appearance()
    if not self.chart then return end
    local PART_MAIN  = lv.PART_MAIN or 0
    local PART_ITEMS = lv.PART_ITEMS or 0x050000
    local bg_color   = common.colorToNumber(self.props.bg_color, 0xffffff)
    local grid_color = common.colorToNumber(self.props.grid_color, 0xdde2e8)
    local axis_color = common.colorToNumber(self.props.axis_color, 0x7a8290)
    local line_width = tonumber(self.props.line_width) or 2
    local point_size = tonumber(self.props.point_size) or 3

    if self.chart.set_style_bg_color then self.chart:set_style_bg_color(bg_color, PART_MAIN) end
    if self.chart.set_style_bg_opa   then self.chart:set_style_bg_opa(lv.OPA_COVER or 255, PART_MAIN) end
    if self.chart.set_style_border_color then self.chart:set_style_border_color(axis_color, PART_MAIN) end
    if self.chart.set_style_border_opa   then self.chart:set_style_border_opa(lv.OPA_COVER or 255, PART_MAIN) end
    if self.chart.set_style_line_color then self.chart:set_style_line_color(grid_color, PART_MAIN) end
    if self.chart.set_style_line_opa then
      self.chart:set_style_line_opa(self.props.show_grid and (lv.OPA_COVER or 255) or (lv.OPA_TRANSP or 0), PART_MAIN)
    end
    if self.chart.set_style_line_width then self.chart:set_style_line_width(line_width, PART_ITEMS) end
    if self.chart.set_style_width  then self.chart:set_style_width(self.props.show_points and point_size or 0, PART_ITEMS) end
    if self.chart.set_style_height then self.chart:set_style_height(self.props.show_points and point_size or 0, PART_ITEMS) end
  end
  local function apply_point_count()
    if self.chart and self.chart.set_point_count then
      self.chart:set_point_count(self.props.point_count)
    end
  end
  local function fill_series(ser, data)
    if not (self.chart and ser) then return end
    if self.chart.set_series_value then
      for i = 1, self.props.point_count do
        local v = data[i]
        if v ~= nil then self.chart:set_series_value(ser, i - 1, v) end
      end
    end
  end
  local function rebuild_series()
    if not self.chart then return end
    -- 移除旧序列（若 LVGL 暴露 remove_series）
    if self.chart.remove_series then
      for _, s in ipairs(self._series) do
        self.chart:remove_series(s)
      end
    end
    self._series = {}
    local AXIS_PRIMARY_Y = lv.CHART_AXIS_PRIMARY_Y or 0

    if self.chart.add_series then
      local s1 = self.chart:add_series(common.colorToNumber(self.props.series1_color, 0x1976d2),
                                       AXIS_PRIMARY_Y)
      table.insert(self._series, s1)
      fill_series(s1, parse_series(self.props.series1_data, self.props.point_count))

      if self.props.series2_data and self.props.series2_data ~= "" then
        local s2 = self.chart:add_series(common.colorToNumber(self.props.series2_color, 0xe53935),
                                         AXIS_PRIMARY_Y)
        table.insert(self._series, s2)
        fill_series(s2, parse_series(self.props.series2_data, self.props.point_count))
      end
    end
    if self.chart.refresh then self.chart:refresh() end
  end

  local function apply_visible()
    if not self.chart then return end
    if self.props.visible then
      if self.chart.clear_flag and lv.OBJ_FLAG_HIDDEN then self.chart:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.chart.add_flag   and lv.OBJ_FLAG_HIDDEN then self.chart:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.chart then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.chart.clear_state then self.chart:clear_state(sd) end
    else
      if self.chart.add_state   then self.chart:add_state(sd) end
    end
  end

  function self.draw()
    self.chart = lv.chart_create(parent)
    self.chart:set_pos(self.props.x, self.props.y)
    self.chart:set_size(self.props.width, self.props.height)
    apply_type()
    apply_range()
    apply_grid()
    apply_appearance()
    apply_point_count()
    rebuild_series()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[chart] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[chart] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.chart.add_event_cb then self.chart:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.chart, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if     name == "chart_type"  then apply_type()
    elseif name == "y_min" or name == "y_max" then apply_range()
    elseif name == "div_x" or name == "div_y" or name == "show_grid" then apply_grid(); apply_appearance()
    elseif name == "bg_color" or name == "grid_color" or name == "axis_color"
        or name == "line_width" or name == "point_size" or name == "show_points" then
      apply_appearance()
    elseif name == "point_count" then apply_point_count(); rebuild_series()
    elseif name == "series1_data" or name == "series1_color"
        or name == "series2_data" or name == "series2_color" then
      rebuild_series()
    elseif name == "x" or name == "y" then
      if self.chart then self.chart:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.chart then self.chart:set_size(self.props.width, self.props.height) end
    elseif name == "enabled" then apply_enabled()
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
    self._series = {}
    if self.chart and self.chart.del then self.chart:del()
    elseif self.chart and lv.obj_del then lv.obj_del(self.chart) end
    self.chart = nil
  end

  return self
end

return Chart
