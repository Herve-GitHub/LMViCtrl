-- 带元数据的平铺页示例（lv_tileview）
local lv     = require("lvgl")
local common = require("common.common")

local TileView = {}

TileView.__widget_meta = {
  id             = "lv_tileview_basic",
  name           = "TileView",
  description    = "示例平铺页：网格化分页，可在分页间滑动",
  category       = "容器与页面组织",
  icon           = "img/widgets/tileview.png",
  preview_image  = "img/widgets/tileview.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "tileview", "平铺", "页面" },

  type        = "lv_tileview",
  render_mode = "custom",

  default_size = { w = 320, h = 220 },
  min_size     = { w = 100, h = 80  },
  max_size     = { w = 4096, h = 4096 },
  resizable    = true,

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
    { group = "布局", name = "x", type = "number", default = 0,   label = "X", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "y", type = "number", default = 0,   label = "Y", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 320, label = "宽度", unit = "px", min = 80, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 220, label = "高度", unit = "px", min = 60, max = 4096 },

    { group = "网格", name = "cols", type = "number", default = 3, label = "列数", min = 1, max = 16 },
    { group = "网格", name = "rows", type = "number", default = 2, label = "行数", min = 1, max = 16 },
    { group = "网格", name = "current_col", type = "number", default = 0, label = "当前列", min = 0, max = 15, bindable = true },
    { group = "网格", name = "current_row", type = "number", default = 0, label = "当前行", min = 0, max = 15, bindable = true },
    { group = "网格", name = "labels", type = "string",
      default = "Tile 1,Tile 2,Tile 3\nTile 4,Tile 5,Tile 6",
      label = "格子标签(行换行 列逗号)", multiline = true, lines = 4 },

    { group = "外观", name = "bg_color",      type = "color", default = "#ffffff", label = "背景色" },
    { group = "外观", name = "tile_bg",       type = "color", default = "#f4f6f9", label = "格子背景" },
    { group = "外观", name = "active_bg",     type = "color", default = "#1976d2", label = "当前格背景" },
    { group = "外观", name = "tile_text",     type = "color", default = "#1f2933", label = "格子文字" },
    { group = "外观", name = "active_text",   type = "color", default = "#ffffff", label = "当前格文字" },
    { group = "外观", name = "indicator_color", type = "color", default = "#1976d2", label = "页指示器颜色" },
    { group = "外观", name = "show_indicator", type = "boolean", default = true, label = "显示页指示器" },
    { group = "外观", name = "border_color",  type = "color", default = "#cfd6dd", label = "格子边框", advanced = true },
    { group = "外观", name = "border_width",  type = "number", default = 1, label = "边框宽度", unit = "px",
      min = 0, max = 16, advanced = true },
    { group = "外观", name = "gap",           type = "number", default = 4, label = "格子间距", unit = "px", min = 0, max = 64 },
    { group = "外观", name = "font_size",     type = "number", default = 13, label = "字号", unit = "px", min = 6, max = 64 },

    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用", bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见", bindable = true },
  },

  bindings = {
    { name = "current_col", target = "value",   direction = "inout" },
    { name = "current_row", target = "value",   direction = "inout" },
    { name = "enabled",     target = "enabled", direction = "in" },
    { name = "visible",     target = "visible", direction = "in" },
  },

  events = {
    { name = "value_changed", label = "切换格子", params = {} },
    { name = "clicked",       label = "点击",     params = {} },
  },

  event_properties = {
    { name = "on_value_changed_handler", type = "code", language = "lua",
      event = "value_changed", label = "切换处理代码",
      default = "", multiline = true, lines = 6 },
  },

  draw_hints = {
    shape                = "tileview",
    cols_from            = "cols",
    rows_from            = "rows",
    current_col_from     = "current_col",
    current_row_from     = "current_row",
    labels_from          = "labels",
    bg_color_from        = "bg_color",
    tile_bg_from         = "tile_bg",
    active_bg_from       = "active_bg",
    tile_text_from       = "tile_text",
    active_text_from     = "active_text",
    indicator_color_from = "indicator_color",
    show_indicator_from  = "show_indicator",
    border_color_from    = "border_color",
    border_width_from    = "border_width",
    gap_from             = "gap",
    font_size_from       = "font_size",
    enabled_from         = "enabled",
  },
}

function TileView.new(parent, state)
  state = state or {}
  local self = {}
  self.props = {}
  for _, p in ipairs(TileView.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}
  self._tiles = {}

  local function build()
    if not self.tv then return end
    self._tiles = {}
    if not lv.tileview_add_tile then return end
    local DIR_ALL = (lv.DIR_TOP or 1) | (lv.DIR_BOTTOM or 2) | (lv.DIR_LEFT or 4) | (lv.DIR_RIGHT or 8)
    for r = 0, (self.props.rows or 1) - 1 do
      for c = 0, (self.props.cols or 1) - 1 do
        local t
        if self.tv.tileview_add_tile then
          t = self.tv:tileview_add_tile(c, r, DIR_ALL)
        else
          t = lv.tileview_add_tile(self.tv, c, r, DIR_ALL)
        end
        table.insert(self._tiles, t)
      end
    end
    if self.tv.set_tile_id then
      self.tv:set_tile_id(self.props.current_col or 0, self.props.current_row or 0,
                           lv.ANIM_OFF or 0)
    end
  end

  local function apply_visible()
    if not self.tv then return end
    if self.props.visible then
      if self.tv.clear_flag and lv.OBJ_FLAG_HIDDEN then self.tv:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.tv.add_flag   and lv.OBJ_FLAG_HIDDEN then self.tv:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.tv then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then if self.tv.clear_state then self.tv:clear_state(sd) end
    else if self.tv.add_state then self.tv:add_state(sd) end end
  end

  function self.draw()
    self.tv = lv.tileview_create(parent)
    self.tv:set_pos(self.props.x, self.props.y)
    self.tv:set_size(self.props.width, self.props.height)
    build()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[tileview] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"       then ev_code = lv.EVENT_CLICKED
    elseif event_name == "value_changed" then ev_code = lv.EVENT_VALUE_CHANGED
    else print("[tileview] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.tv.add_event_cb then self.tv:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.tv, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "current_col" or name == "current_row" then
      if self.tv and self.tv.set_tile_id then
        self.tv:set_tile_id(self.props.current_col or 0, self.props.current_row or 0,
                             lv.ANIM_OFF or 0)
      end
    elseif name == "x" or name == "y" then
      if self.tv then self.tv:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" then
      if self.tv then self.tv:set_size(self.props.width, self.props.height) end
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
    self._cb_handles = {}; self._tiles = {}
    if self.tv and self.tv.del then self.tv:del()
    elseif self.tv and lv.obj_del then lv.obj_del(self.tv) end
    self.tv = nil
  end
  return self
end

return TileView
