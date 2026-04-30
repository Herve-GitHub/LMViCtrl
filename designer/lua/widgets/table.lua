-- 带元数据的表格示例
local lv     = require("lvgl")
local common = require("common.common")

local Table = {}

Table.__widget_meta = {
  id             = "lv_table_basic",
  name           = "Table",
  description    = "示例表格控件，支持多行多列、列宽、表头高亮",
  category       = "数据展示",
  icon           = "img/widgets/table.png",
  preview_image  = "img/widgets/table.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "table", "表格", "数据" },

  type        = "lv_table",
  render_mode = "custom",

  default_size = { w = 240, h = 160 },
  min_size     = { w = 60,  h = 40  },
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
    { group = "布局", name = "width",  type = "number", default = 240, label = "宽度", unit = "px",
      min = 40, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 160, label = "高度", unit = "px",
      min = 40, max = 4096 },

    -- 数据
    { group = "数据", name = "row_count",   type = "number", default = 4, label = "行数",
      min = 1, max = 200 },
    { group = "数据", name = "col_count",   type = "number", default = 3, label = "列数",
      min = 1, max = 32 },
    { group = "数据", name = "header_rows", type = "number", default = 1, label = "表头行数",
      min = 0, max = 4, description = "顶部高亮显示的行数" },
    { group = "数据", name = "col_widths",  type = "string", default = "80,80,80",
      label = "列宽(逗号分隔)", description = "例如 80,120,60；空表示均分",
      advanced = true },
    { group = "数据", name = "data",        type = "string",
      default = "标题1,标题2,标题3\n行1A,行1B,行1C\n行2A,行2B,行2C\n行3A,行3B,行3C",
      label = "单元格内容", multiline = true, lines = 6,
      description = "每行用换行分隔，单元格用半角逗号分隔" },

    -- 外观
    { group = "外观", name = "bg_color",     type = "color", default = "#ffffff", label = "背景色" },
    { group = "外观", name = "header_bg",    type = "color", default = "#e8eef7", label = "表头背景" },
    { group = "外观", name = "header_text",  type = "color", default = "#1f2933", label = "表头文字" },
    { group = "外观", name = "cell_text",    type = "color", default = "#333333", label = "单元格文字" },
    { group = "外观", name = "grid_color",   type = "color", default = "#cfd6dd", label = "网格线" },
    { group = "外观", name = "row_height",   type = "number", default = 28,
      label = "行高", unit = "px", min = 12, max = 200 },
    { group = "外观", name = "font_size",    type = "number", default = 12,
      label = "字号", unit = "px", min = 6, max = 64 },
    { group = "外观", name = "stripe_rows",  type = "boolean", default = true, label = "斑马纹" },
    { group = "外观", name = "stripe_color", type = "color",   default = "#f6f8fa", label = "斑马色",
      advanced = true },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
      bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "data",    target = "value",   direction = "in" },
    { name = "enabled", target = "enabled", direction = "in" },
    { name = "visible", target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "选中变化",
      description = "选中的单元格发生变化时触发", params = {} },
    { name = "clicked", label = "点击", params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "选中变化代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape           = "table",
    row_count_from  = "row_count",
    col_count_from  = "col_count",
    header_rows_from = "header_rows",
    col_widths_from = "col_widths",
    data_from       = "data",
    bg_color_from   = "bg_color",
    header_bg_from  = "header_bg",
    header_text_from = "header_text",
    cell_text_from  = "cell_text",
    grid_color_from = "grid_color",
    row_height_from = "row_height",
    font_size_from  = "font_size",
    stripe_rows_from = "stripe_rows",
    stripe_color_from = "stripe_color",
    enabled_from    = "enabled",
  },
}

-- 解析 "a,b,c\nd,e,f" 字符串为 rows×cols 单元格表
local function parse_data(text, rows, cols)
  local out = {}
  if type(text) ~= "string" then text = "" end
  local r = 1
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do
    if r > rows then break end
    local row = {}
    local c = 1
    for cell in (line .. ","):gmatch("([^,]*),") do
      if c > cols then break end
      row[c] = cell
      c = c + 1
    end
    out[r] = row
    r = r + 1
  end
  return out
end

local function parse_widths(text)
  local out = {}
  if type(text) ~= "string" then return out end
  for w in text:gmatch("([^,]+)") do
    local n = tonumber((w:gsub("%s", "")))
    if n then table.insert(out, n) end
  end
  return out
end

function Table.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Table.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}

  local function apply_data()
    if not self.tbl then return end
    if self.tbl.set_col_cnt then self.tbl:set_col_cnt(self.props.col_count) end
    if self.tbl.set_row_cnt then self.tbl:set_row_cnt(self.props.row_count) end

    local widths = parse_widths(self.props.col_widths)
    if self.tbl.set_col_width then
      for i = 1, self.props.col_count do
        local w = widths[i] or math.floor((self.props.width or 240) / self.props.col_count)
        self.tbl:set_col_width(i - 1, w)
      end
    end

    local cells = parse_data(self.props.data, self.props.row_count, self.props.col_count)
    if self.tbl.set_cell_value then
      for r = 1, self.props.row_count do
        local row = cells[r] or {}
        for c = 1, self.props.col_count do
          self.tbl:set_cell_value(r - 1, c - 1, row[c] or "")
        end
      end
    end
  end

  local function apply_visible()
    if not self.tbl then return end
    if self.props.visible then
      if self.tbl.clear_flag and lv.OBJ_FLAG_HIDDEN then self.tbl:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.tbl.add_flag   and lv.OBJ_FLAG_HIDDEN then self.tbl:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.tbl then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.tbl.clear_state then self.tbl:clear_state(sd) end
    else
      if self.tbl.add_state   then self.tbl:add_state(sd) end
    end
  end

  function self.draw()
    self.tbl = lv.table_create(parent)
    self.tbl:set_pos(self.props.x, self.props.y)
    self.tbl:set_size(self.props.width, self.props.height)
    apply_data()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[table] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[table] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.tbl.add_event_cb then self.tbl:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.tbl, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "row_count" or name == "col_count" or name == "data" or name == "col_widths" then
      apply_data()
    elseif name == "x" or name == "y" then
      if self.tbl then self.tbl:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.tbl then self.tbl:set_size(self.props.width, self.props.height); apply_data() end
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
    if self.tbl and self.tbl.del then self.tbl:del()
    elseif self.tbl and lv.obj_del then lv.obj_del(self.tbl) end
    self.tbl = nil
  end

  return self
end

return Table
