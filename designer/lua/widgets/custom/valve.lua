-- 自定义控件：旋转阀门 Valve
-- 由 VduEditor 工程移植，适配当前编辑器的元数据/接口规范
local lv     = require("lvgl")
local common = require("common.common")

local Valve = {}

Valve.__widget_meta = {
  id             = "lv_custom_valve",
  name           = "Valve",
  description    = "旋转阀门控件：可设置当前/开启/关闭角度，运行模式下点击会弹出确认对话框",
  category       = "自定义控件",
  icon           = "img/widgets/valve.png",
  preview_image  = "img/widgets/valve.png",
  schema_version = "1.1",
  version        = "1.0",
  author         = "designer",
  tags           = { "valve", "阀门", "旋转", "自定义" },

  type        = "lv_obj",
  render_mode = "custom",

  default_size = { w = 80, h = 80 },
  min_size     = { w = 16, h = 16 },
  max_size     = { w = 1024, h = 1024 },
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
    { group = "布局", name = "x",      type = "number", default = 0,  label = "X",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "y",      type = "number", default = 0,  label = "Y",    unit = "px",
      min = -4096, max = 4096 },
    { group = "布局", name = "width",  type = "number", default = 80, label = "宽度", unit = "px",
      min = 16, max = 1024 },
    { group = "布局", name = "height", type = "number", default = 80, label = "高度", unit = "px",
      min = 16, max = 1024 },

    -- 角度
    { group = "角度", name = "angle",       type = "number", default = 0,  label = "当前角度",
      unit = "°", min = 0, max = 360, bindable = true },
    { group = "角度", name = "open_angle",  type = "number", default = 90, label = "开启角度",
      unit = "°", min = 0, max = 360 },
    { group = "角度", name = "close_angle", type = "number", default = 0,  label = "关闭角度",
      unit = "°", min = 0, max = 360 },

    -- 外观
    { group = "外观", name = "bg_color",     type = "color",  default = "#e0e0e0", label = "底盘颜色" },
    { group = "外观", name = "border_color", type = "color",  default = "#606060", label = "边框颜色" },
    { group = "外观", name = "handle_color", type = "color",  default = "#ff5722", label = "把手颜色",
      bindable = true },
    { group = "外观", name = "pivot_color",  type = "color",  default = "#333333", label = "中心轴颜色",
      advanced = true },

    -- 行为
    { group = "行为", name = "design_mode", type = "boolean", default = true,  label = "设计模式",
      description = "勾选时点击控件不弹出确认对话框，也不会改变状态" },
    { group = "行为", name = "enabled",     type = "boolean", default = true,  label = "启用",
      bindable = true },
    { group = "行为", name = "visible",     type = "boolean", default = true,  label = "可见",
      bindable = true },
  },

  bindings = {
    { name = "angle",        target = "value",   direction = "inout" },
    { name = "handle_color", target = "color",   direction = "in"    },
    { name = "enabled",      target = "enabled", direction = "in"    },
    { name = "visible",      target = "visible", direction = "in"    },
    { name = "toggled",      target = "trigger", direction = "out"   },
  },

  events = {
    { name = "clicked", label = "点击",
      description = "阀门被点击时触发", params = {} },
    { name = "angle_changed", label = "角度改变",
      description = "当前角度发生变化时触发",
      params = { { name = "angle", type = "number", description = "新的角度（0~360）" } } },
    { name = "toggled", label = "开关切换",
      description = "阀门在开/关之间切换时触发",
      params = { { name = "is_open", type = "boolean", description = "是否处于开启状态" } } },
  },

  event_properties = {
    { name = "on_clicked_handler",       type = "code", language = "lua",
      event = "clicked",       label = "点击处理代码",
      default = "", multiline = true, lines = 6 },
    { name = "on_angle_changed_handler", type = "code", language = "lua",
      event = "angle_changed", label = "角度改变处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('angle =', self.props.angle)" },
    { name = "on_toggled_handler",       type = "code", language = "lua",
      event = "toggled",       label = "开关切换处理代码",
      default = "", multiline = true, lines = 6,
      snippet = "print('is_open =', is_open)" },
  },

  draw_hints = {
    shape            = "valve",
    angle_from       = "angle",
    bg_color_from    = "bg_color",
    border_from      = "border_color",
    handle_from      = "handle_color",
    pivot_from       = "pivot_color",
    enabled_from     = "enabled",
  },
}

-- 类级实例集合（保留 open_all / close_all 能力）
Valve.instances = setmetatable({}, { __mode = "v" })

function Valve.open_all()
  for _, v in pairs(Valve.instances) do
    if v and v.open then v:open() end
  end
end

function Valve.close_all()
  for _, v in pairs(Valve.instances) do
    if v and v.close then v:close() end
  end
end

function Valve.new(parent, state)
  state = state or {}
  local self = {}

  self.props = {}
  for _, p in ipairs(Valve.__widget_meta.properties) do
    self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
  end

  self._cb_handles     = {}
  self._event_listeners = { clicked = {}, angle_changed = {}, toggled = {} }
  self._suppress_event = false
  self.is_open = math.abs((self.props.angle or 0) - (self.props.open_angle or 90)) < 1

  table.insert(Valve.instances, self)

  ----------------------------------------------------------------------------
  -- 内部工具
  ----------------------------------------------------------------------------
  local function fire(event_name, ...)
    if self._suppress_event then return end
    local list = self._event_listeners[event_name]
    if not list then return end
    for _, cb in ipairs(list) do
      local ok, err = pcall(cb, self, ...)
      if not ok then print("[valve] callback error:", err) end
    end
  end

  local function apply_size()
    if not self.container then return end
    local w = math.max(8, math.floor(self.props.width  or 80))
    local h = math.max(8, math.floor(self.props.height or 80))
    local s = math.min(w, h)
    self.container:set_size(w, h)

    local hw = math.floor(s * 0.8)
    local hh = math.max(2, math.floor(s * 0.2))
    if self.handle then
      self.handle:set_size(hw, hh)
      self.handle:center()
      if self.handle.set_style_transform_pivot_x then
        self.handle:set_style_transform_pivot_x(math.floor(hw / 2), 0)
        self.handle:set_style_transform_pivot_y(math.floor(hh / 2), 0)
      end
    end
    if self.pivot then
      local ps = math.max(4, math.floor(s * 0.15))
      self.pivot:set_size(ps, ps)
      self.pivot:center()
    end
  end

  local function apply_pos()
    if self.container then
      self.container:set_pos(self.props.x or 0, self.props.y or 0)
    end
  end

  local function apply_bg_color()
    if self.container and self.container.set_style_bg_color then
      self.container:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0xe0e0e0), 0)
    end
  end

  local function apply_border_color()
    if self.container and self.container.set_style_border_color then
      self.container:set_style_border_color(common.colorToNumber(self.props.border_color, 0x606060), 0)
    end
  end

  local function apply_handle_color()
    if self.handle and self.handle.set_style_bg_color then
      self.handle:set_style_bg_color(common.colorToNumber(self.props.handle_color, 0xff5722), 0)
    end
  end

  local function apply_pivot_color()
    if self.pivot and self.pivot.set_style_bg_color then
      self.pivot:set_style_bg_color(common.colorToNumber(self.props.pivot_color, 0x333333), 0)
    end
  end

  local function apply_angle()
    if self.handle and self.handle.set_style_transform_rotation then
      self.handle:set_style_transform_rotation(math.floor((self.props.angle or 0) * 10), 0)
    end
  end

  local function apply_enabled_state()
    if not self.container then return end
    local sd = lv.STATE_DISABLED or 0x0080
    if self.props.enabled then
      if self.container.clear_state then self.container:clear_state(sd) end
    else
      if self.container.add_state   then self.container:add_state(sd) end
    end
  end

  local function apply_visible()
    if not self.container then return end
    local hidden = lv.OBJ_FLAG_HIDDEN
    if not hidden then return end
    if self.props.visible then
      if self.container.clear_flag then self.container:clear_flag(hidden) end
    else
      if self.container.add_flag   then self.container:add_flag(hidden) end
    end
  end

  ----------------------------------------------------------------------------
  -- 创建对象
  ----------------------------------------------------------------------------
  function self.draw()
    self.container = lv.obj_create(parent)
    self.container:set_pos(self.props.x or 0, self.props.y or 0)

    if self.container.set_style_radius and lv.RADIUS_CIRCLE then
      self.container:set_style_radius(lv.RADIUS_CIRCLE, 0)
    end
    if self.container.set_style_border_width then
      self.container:set_style_border_width(2, 0)
    end
    if self.container.remove_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.container:remove_flag(lv.OBJ_FLAG_SCROLLABLE)
    elseif self.container.clear_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.container:clear_flag(lv.OBJ_FLAG_SCROLLABLE)
    end

    -- 把手
    self.handle = lv.obj_create(self.container)
    if self.handle.set_style_radius then self.handle:set_style_radius(4, 0) end
    if self.handle.remove_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.handle:remove_flag(lv.OBJ_FLAG_SCROLLABLE)
    elseif self.handle.clear_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.handle:clear_flag(lv.OBJ_FLAG_SCROLLABLE)
    end

    -- 中心轴
    self.pivot = lv.obj_create(self.container)
    if self.pivot.set_style_radius and lv.RADIUS_CIRCLE then
      self.pivot:set_style_radius(lv.RADIUS_CIRCLE, 0)
    end
    if self.pivot.remove_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.pivot:remove_flag(lv.OBJ_FLAG_SCROLLABLE)
    elseif self.pivot.clear_flag and lv.OBJ_FLAG_SCROLLABLE then
      self.pivot:clear_flag(lv.OBJ_FLAG_SCROLLABLE)
    end

    apply_size()
    apply_bg_color()
    apply_border_color()
    apply_handle_color()
    apply_pivot_color()
    apply_angle()
    apply_enabled_state()
    apply_visible()

    -- 内置点击：非设计模式弹出确认对话框
    local function on_internal_click()
      fire("clicked")
      if not self.props.design_mode then
        self:show_confirm_dialog()
      end
    end
    if self.container.add_event_cb and lv.EVENT_CLICKED then
      self.container:add_event_cb(on_internal_click, lv.EVENT_CLICKED, nil)
    end
  end

  self.draw()

  ----------------------------------------------------------------------------
  -- 行为 API
  ----------------------------------------------------------------------------
  function self:set_angle(angle)
    self.props.angle = angle or 0
    apply_angle()
    fire("angle_changed", self.props.angle)
  end

  function self:get_angle() return self.props.angle end

  function self:open()
    if self.props.design_mode then return end
    self:set_angle(self.props.open_angle)
    self.is_open = true
    fire("toggled", true)
  end

  function self:close()
    if self.props.design_mode then return end
    self:set_angle(self.props.close_angle)
    self.is_open = false
    fire("toggled", false)
  end

  function self:toggle()
    if self.props.design_mode then return end
    if self.is_open then self:close() else self:open() end
  end

  function self:show_confirm_dialog()
    if self.props.design_mode then return end
    local scr = lv.scr_act and lv.scr_act() or nil
    if not scr then return end

    local modal = lv.obj_create(scr)
    if modal.set_size and lv.pct then modal:set_size(lv.pct(100), lv.pct(100)) end
    if modal.set_style_bg_color then modal:set_style_bg_color(0x000000, 0) end
    if modal.set_style_bg_opa   then modal:set_style_bg_opa(128, 0) end
    if modal.center then modal:center() end

    local panel = lv.obj_create(modal)
    panel:set_size(300, 180)
    if panel.center then panel:center() end
    if panel.set_style_bg_color then panel:set_style_bg_color(0xffffff, 0) end
    if panel.set_style_radius then panel:set_style_radius(8, 0) end

    local label = lv.label_create(panel)
    local action = self.is_open and "关闭" or "开启"
    label:set_text("确认要" .. action .. "阀门吗？")
    if label.set_width then label:set_width(260) end
    if label.set_style_text_align and lv.TEXT_ALIGN_CENTER then
      label:set_style_text_align(lv.TEXT_ALIGN_CENTER, 0)
    end
    if label.align and lv.ALIGN_TOP_MID then
      label:align(lv.ALIGN_TOP_MID, 0, 20)
    end

    local btn_create = lv.btn_create or lv.button_create
    local btn_yes = btn_create(panel)
    btn_yes:set_size(100, 40)
    if btn_yes.align and lv.ALIGN_BOTTOM_LEFT then
      btn_yes:align(lv.ALIGN_BOTTOM_LEFT, 10, -10)
    end
    local lbl_yes = lv.label_create(btn_yes)
    lbl_yes:set_text("确认")
    if lbl_yes.center then lbl_yes:center() end

    local btn_no = btn_create(panel)
    btn_no:set_size(100, 40)
    if btn_no.align and lv.ALIGN_BOTTOM_RIGHT then
      btn_no:align(lv.ALIGN_BOTTOM_RIGHT, -10, -10)
    end
    local lbl_no = lv.label_create(btn_no)
    lbl_no:set_text("取消")
    if lbl_no.center then lbl_no:center() end

    local function close_modal()
      if modal.delete then modal:delete()
      elseif modal.del then modal:del()
      elseif lv.obj_del then lv.obj_del(modal) end
    end

    if btn_yes.add_event_cb and lv.EVENT_CLICKED then
      btn_yes:add_event_cb(function()
        self:toggle()
        close_modal()
      end, lv.EVENT_CLICKED, nil)
    end
    if btn_no.add_event_cb and lv.EVENT_CLICKED then
      btn_no:add_event_cb(function()
        close_modal()
      end, lv.EVENT_CLICKED, nil)
    end
  end

  ----------------------------------------------------------------------------
  -- 元接口
  ----------------------------------------------------------------------------
  function self:on(event_name, callback)
    if not self._event_listeners[event_name] then
      print("[valve] unsupported event:", event_name)
      return
    end
    table.insert(self._event_listeners[event_name], callback)
  end

  function self:get_property(name) return self.props[name] end

  function self:set_property(name, value)
    self.props[name] = value
    if name == "x" or name == "y" then
      apply_pos()
    elseif name == "width" or name == "height" or name == "size" then
      -- 兼容旧的 size 字段：等比例同步
      if name == "size" then
        self.props.width  = value
        self.props.height = value
      end
      apply_size()
      apply_angle()
    elseif name == "angle" then
      self._suppress_event = true
      apply_angle()
      self._suppress_event = false
      fire("angle_changed", self.props.angle)
    elseif name == "bg_color"     then apply_bg_color()
    elseif name == "border_color" then apply_border_color()
    elseif name == "handle_color" then apply_handle_color()
    elseif name == "pivot_color"  then apply_pivot_color()
    elseif name == "enabled"      then apply_enabled_state()
    elseif name == "visible"      then apply_visible()
    end
    return true
  end

  function self:get_properties()
    local out = {}; for k, v in pairs(self.props) do out[k] = v end; return out
  end

  function self:apply_properties(t)
    for k, v in pairs(t) do self:set_property(k, v) end
    return true
  end

  function self:to_state() return self:get_properties() end

  function self:destroy()
    self._cb_handles      = {}
    self._event_listeners = { clicked = {}, angle_changed = {}, toggled = {} }
    if self.container and self.container.del then
      self.container:del()
    elseif self.container and lv.obj_del then
      lv.obj_del(self.container)
    end
    self.container = nil
    self.handle    = nil
    self.pivot     = nil
  end

  return self
end

return Valve
