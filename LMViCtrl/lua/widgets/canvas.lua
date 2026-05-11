-- 带元数据的画布示例
local lv     = require("lvgl")
local common = require("common.common")

local Canvas = {}

Canvas.__widget_meta = {
  id             = "lv_canvas_basic",
  name           = "Canvas",
  description    = "示例画布，可作为自定义绘制区域，提供基础填充/网格/边框",
  category       = "绘制与媒体",
  icon           = "img/widgets/canvas.png",
  preview_image  = "img/widgets/canvas.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "canvas", "画布", "绘制" },

  type        = "lv_canvas",
  render_mode = "custom",

  default_size = { w = 200, h = 150 },
  min_size     = { w = 16,  h = 16  },
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
    { group = "布局", name = "x",      type = "number", default = 0,   label = "X", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "y",      type = "number", default = 0,   label = "Y", unit = "px", min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 200, label = "宽度", unit = "px", min = 8, max = 4096 },
    { group = "布局", name = "height", type = "number", default = 150, label = "高度", unit = "px", min = 8, max = 4096 },

    -- 缓冲区
    { group = "缓冲区", name = "buffer_format", type = "enum", default = "rgb565", label = "色彩格式",
      options = {
        { value = "rgb565",  label = "RGB565 (16bpp)" },
        { value = "rgb888",  label = "RGB888 (24bpp)" },
        { value = "argb8888", label = "ARGB8888 (32bpp)" },
      },
      description = "决定每像素字节数与可用透明度" },
    { group = "缓冲区", name = "auto_alloc",   type = "boolean", default = true, label = "自动分配缓冲区",
      description = "禁用时需在外部代码中显式调用 set_buffer" },

    -- 默认绘制
    { group = "默认绘制", name = "fill_color",  type = "color", default = "#ffffff", label = "填充色",
      bindable = true },
    { group = "默认绘制", name = "fill_opa",    type = "number", default = 255, label = "填充不透明度",
      min = 0, max = 255 },
    { group = "默认绘制", name = "show_grid",   type = "boolean", default = false, label = "显示网格",
      description = "在画布上绘制等距网格作为参考" },
    { group = "默认绘制", name = "grid_step",   type = "number", default = 16, label = "网格间距", unit = "px",
      min = 2, max = 256 },
    { group = "默认绘制", name = "grid_color",  type = "color", default = "#dde2e8", label = "网格颜色" },
    { group = "默认绘制", name = "show_diagonals", type = "boolean", default = false, label = "对角辅助线",
      advanced = true },

    -- 外观
    { group = "外观", name = "border_color", type = "color", default = "#cfd6dd", label = "边框颜色" },
    { group = "外观", name = "border_width", type = "number", default = 1, label = "边框宽度", unit = "px",
      min = 0, max = 16 },
    { group = "外观", name = "radius",       type = "number", default = 0, label = "圆角半径", unit = "px",
      min = 0, max = 200 },

    -- 行为
    { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用", bindable = true },
    { group = "行为", name = "visible", type = "boolean", default = true, label = "可见", bindable = true },
  },

  bindings = {
    { name = "fill_color", target = "color",   direction = "in" },
    { name = "enabled",    target = "enabled", direction = "in" },
    { name = "visible",    target = "visible", direction = "in" },
  },

  events = {
    { name = "clicked",  label = "点击",    params = {} },
    { name = "pressed",  label = "按下",    params = {} },
    { name = "released", label = "释放",    params = {} },
  },

  event_properties = {
    { name = "on_clicked_handler", type = "code", language = "lua",
      event = "clicked", label = "点击处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "-- 在此使用 self.canvas:set_px / fill_bg 等方法绘制" },
  },

  draw_hints = {
    shape               = "canvas",
    fill_color_from     = "fill_color",
    fill_opa_from       = "fill_opa",
    show_grid_from      = "show_grid",
    grid_step_from      = "grid_step",
    grid_color_from     = "grid_color",
    show_diagonals_from = "show_diagonals",
    border_color_from   = "border_color",
    border_width_from   = "border_width",
    radius_from         = "radius",
    buffer_format_from  = "buffer_format",
    enabled_from        = "enabled",
  },
}

function Canvas.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Canvas.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end
  self._cb_handles = {}
  self._buffer = nil

  local function fmt_to_lv(f)
    if f == "rgb888"   then return lv.IMG_CF_TRUE_COLOR        or lv.COLOR_FORMAT_RGB888 or 4 end
    if f == "argb8888" then return lv.IMG_CF_TRUE_COLOR_ALPHA  or lv.COLOR_FORMAT_ARGB8888 or 5 end
    return lv.IMG_CF_RGB565 or lv.COLOR_FORMAT_RGB565 or 1
  end
  local function bytes_per_px(f)
    if f == "rgb888"   then return 3 end
    if f == "argb8888" then return 4 end
    return 2
  end

  local function alloc_buffer()
    if not self.props.auto_alloc then return end
    if not self.canvas then return end
    local w = self.props.width  or 200
    local h = self.props.height or 150
    local bpp = bytes_per_px(self.props.buffer_format)
    local size = w * h * bpp
    -- 缓冲区分配优先用 lv 提供的 helper；不可用则跳过
    if lv.canvas_buf_alloc then
      self._buffer = lv.canvas_buf_alloc(size)
    elseif lv.mem_alloc then
      self._buffer = lv.mem_alloc(size)
    else
      self._buffer = nil
    end
    if self._buffer and self.canvas.set_buffer then
      self.canvas:set_buffer(self._buffer, w, h, fmt_to_lv(self.props.buffer_format))
    end
  end

  local function apply_fill()
    if not self.canvas then return end
    if self.canvas.fill_bg then
      self.canvas:fill_bg(common.colorToNumber(self.props.fill_color, 0xffffff),
                          self.props.fill_opa or 255)
    end
  end

  local function draw_grid()
    if not self.canvas then return end
    if not self.props.show_grid then return end
    if not self.canvas.set_px then return end
    local w   = self.props.width  or 200
    local h   = self.props.height or 150
    local step = math.max(2, self.props.grid_step or 16)
    local c   = common.colorToNumber(self.props.grid_color, 0xdde2e8)
    for x = 0, w - 1, step do
      for y = 0, h - 1 do self.canvas:set_px(x, y, c) end
    end
    for y = 0, h - 1, step do
      for x = 0, w - 1 do self.canvas:set_px(x, y, c) end
    end
  end

  local function apply_border()
    if not self.canvas then return end
    if self.canvas.set_style_border_color then
      self.canvas:set_style_border_color(common.colorToNumber(self.props.border_color, 0xcfd6dd), 0)
    end
    if self.canvas.set_style_border_width then
      self.canvas:set_style_border_width(self.props.border_width or 1, 0)
    end
    if self.canvas.set_style_radius then
      self.canvas:set_style_radius(self.props.radius or 0, 0)
    end
  end

  local function apply_visible()
    if not self.canvas then return end
    if self.props.visible then
      if self.canvas.clear_flag and lv.OBJ_FLAG_HIDDEN then self.canvas:clear_flag(lv.OBJ_FLAG_HIDDEN) end
    else
      if self.canvas.add_flag   and lv.OBJ_FLAG_HIDDEN then self.canvas:add_flag(lv.OBJ_FLAG_HIDDEN)   end
    end
  end
  local function apply_enabled()
    if not self.canvas then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.canvas.clear_state then self.canvas:clear_state(sd) end
    else
      if self.canvas.add_state   then self.canvas:add_state(sd) end
    end
  end

  function self.draw()
    self.canvas = lv.canvas_create(parent)
    self.canvas:set_pos(self.props.x, self.props.y)
    self.canvas:set_size(self.props.width, self.props.height)
    alloc_buffer()
    apply_fill()
    draw_grid()
    apply_border()
    apply_enabled()
    apply_visible()
  end
  self.draw()

  function self:on(event_name, callback)
    local function safe_cb()
      return function(e)
        local ok, err = pcall(callback, self, e)
        if not ok then print("[canvas] callback error:", err) end
      end
    end
    local ev_code
    if     event_name == "clicked"  then ev_code = lv.EVENT_CLICKED
    elseif event_name == "pressed"  then ev_code = lv.EVENT_PRESSED
    elseif event_name == "released" then ev_code = lv.EVENT_RELEASED
    else print("[canvas] unsupported event:", event_name); return end
    local cb = safe_cb()
    if self.canvas.add_event_cb then self.canvas:add_event_cb(cb, ev_code, nil)
    elseif lv.obj_add_event_cb then lv.obj_add_event_cb(self.canvas, cb, ev_code, nil) end
    table.insert(self._cb_handles, { event = event_name, cb = cb })
  end

  function self:get_property(name) return self.props[name] end
  function self:set_property(name, value)
    self.props[name] = value
    if name == "x" or name == "y" then
      if self.canvas then self.canvas:set_pos(self.props.x, self.props.y) end
    elseif name == "width" or name == "height" or name == "buffer_format" then
      if self.canvas then
        self.canvas:set_size(self.props.width, self.props.height)
        alloc_buffer(); apply_fill(); draw_grid()
      end
    elseif name == "fill_color" or name == "fill_opa" then apply_fill(); draw_grid()
    elseif name == "show_grid" or name == "grid_step" or name == "grid_color"
        or name == "show_diagonals" then
      apply_fill(); draw_grid()
    elseif name == "border_color" or name == "border_width" or name == "radius" then
      apply_border()
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
    if self.canvas and self.canvas.del then self.canvas:del()
    elseif self.canvas and lv.obj_del then lv.obj_del(self.canvas) end
    self.canvas = nil
    self._buffer = nil
  end

  return self
end

return Canvas
