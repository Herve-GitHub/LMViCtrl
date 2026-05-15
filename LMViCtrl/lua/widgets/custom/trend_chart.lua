-- 自定义控件：趋势图 TrendChart
local lv     = require("lvgl")
local common = require("common.common")

local TrendChart = {}

TrendChart.__widget_meta = {
    id             = "lv_custom_trend_chart",
    name           = "Trend Chart",
    description    = "折线/趋势图控件，可按固定间隔追加数据并触发更新事件",
    category       = "自定义控件",
    icon           = "img/widgets/chart.png",
    preview_image  = "img/widgets/chart.png",
    schema_version = "1.1",
    version        = "1.0",
    author         = "designer",
    tags           = { "trend", "chart", "趋势图", "折线", "自定义" },

    type        = "lv_chart",
    render_mode = "custom",

    default_size = { w = 300, h = 120 },
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
        { group = "布局", name = "x",      type = "number", default = 0,   label = "X",    unit = "px",
            min = -4096, max = 4096 },
        { group = "布局", name = "y",      type = "number", default = 0,   label = "Y",    unit = "px",
            min = -4096, max = 4096 },
        { group = "布局", name = "width",  type = "number", default = 300, label = "宽度", unit = "px",
            min = 80, max = 4096 },
        { group = "布局", name = "height", type = "number", default = 120, label = "高度", unit = "px",
            min = 60, max = 4096 },

        { group = "图表", name = "point_count", type = "number", default = 300, label = "点数",
            min = 2, max = 5000 },
        { group = "图表", name = "update_interval", type = "number", default = 1000,
            label = "刷新间隔", unit = "ms", min = 10, max = 60000 },
        { group = "图表", name = "range_min", type = "number", default = 0, label = "最小值" },
        { group = "图表", name = "range_max", type = "number", default = 100, label = "最大值" },
        { group = "图表", name = "div_y", type = "number", default = 3, label = "横向网格数",
            min = 0, max = 32 },
        { group = "图表", name = "div_x", type = "number", default = 0, label = "纵向网格数",
            min = 0, max = 32 },

        { group = "数据", name = "initial_data", type = "string", default = "",
            label = "初始数据(逗号分隔)", multiline = true, lines = 3, bindable = true,
            description = "留空则使用当前值填充预览" },
        { group = "数据", name = "current_value", type = "number", default = 50,
            label = "当前值", bindable = true },

        { group = "外观", name = "series_color", type = "color", default = "#2196f3", label = "折线颜色" },
        { group = "外观", name = "bg_color",     type = "color", default = "#ffffff", label = "背景色" },
        { group = "外观", name = "grid_color",   type = "color", default = "#d8dee6", label = "网格颜色" },
        { group = "外观", name = "axis_color",   type = "color", default = "#808894", label = "边框颜色" },
        { group = "外观", name = "line_width",   type = "number", default = 2, label = "折线粗细",
            unit = "px", min = 1, max = 16 },
        { group = "外观", name = "show_points",  type = "boolean", default = false, label = "显示数据点" },
        { group = "外观", name = "point_size",   type = "number", default = 3, label = "数据点大小",
            unit = "px", min = 0, max = 32, advanced = true },

        { group = "行为", name = "auto_update", type = "boolean", default = true, label = "自动更新" },
        { group = "行为", name = "enabled", type = "boolean", default = true, label = "启用",
            bindable = true },
        { group = "行为", name = "visible", type = "boolean", default = true, label = "可见",
            bindable = true },
    },

    bindings = {
        { name = "current_value", target = "value",   direction = "inout" },
        { name = "initial_data",  target = "value",   direction = "in"    },
        { name = "enabled",       target = "enabled", direction = "in"    },
        { name = "visible",       target = "visible", direction = "in"    },
        { name = "updated",       target = "trigger", direction = "out"   },
    },

    events = {
        { name = "updated", label = "数据更新",
            description = "趋势图追加新数据时触发",
            params = { { name = "value", type = "number", description = "追加的数据值" } } },
        { name = "clicked", label = "点击",
            description = "趋势图被点击时触发", params = {} },
    },

    event_properties = {
        { name = "on_updated_handler", type = "code", language = "lua",
            event = "updated", label = "更新处理代码",
            default = "", multiline = true, lines = 6,
            snippet = "" },
        { name = "on_clicked_handler", type = "code", language = "lua",
            event = "clicked", label = "点击处理代码",
            default = "", multiline = true, lines = 6 },
    },

    draw_hints = {
        shape              = "chart",
        chart_type_from    = "line",
        point_count_from   = "point_count",
        y_min_from         = "range_min",
        y_max_from         = "range_max",
        div_x_from         = "div_x",
        div_y_from         = "div_y",
        show_grid_from     = "true",
        grid_color_from    = "grid_color",
        series1_data_from  = "initial_data",
        series1_color_from = "series_color",
        bg_color_from      = "bg_color",
        axis_color_from    = "axis_color",
        line_width_from    = "line_width",
        point_size_from    = "point_size",
        show_points_from   = "show_points",
        enabled_from       = "enabled",
    },
}

local function parse_series(text)
    local out = {}
    if type(text) ~= "string" or text == "" then return out end
    for value in (text .. ","):gmatch("([^,]*),") do
        local s = value:gsub("%s", "")
        if s ~= "" then
            local n = tonumber(s)
            if n then table.insert(out, n) end
        end
    end
    return out
end

local function clamp(value, min_value, max_value)
    local n = tonumber(value) or 0
    if n < min_value then return min_value end
    if n > max_value then return max_value end
    return n
end

function TrendChart.new(parent, state)
    state = state or {}
    local self = {}

    self.props = {}
    for _, p in ipairs(TrendChart.__widget_meta.properties) do
        self.props[p.name] = state[p.name] ~= nil and state[p.name] or p.default
    end

    self._cb_handles = {}
    self._event_listeners = { updated = {}, clicked = {} }
    self._suppress_event = false

    local function fire(event_name, ...)
        if self._suppress_event then return end
        local list = self._event_listeners[event_name]
        if not list then return end
        for _, cb in ipairs(list) do
            local ok, err = pcall(cb, self, ...)
            if not ok then print("[trend_chart] callback error:", err) end
        end
    end

    local function axis_primary_y()
        return lv.CHART_AXIS_PRIMARY_Y or 0
    end

    local function apply_pos()
        if self.chart then self.chart:set_pos(self.props.x or 0, self.props.y or 0) end
    end

    local function apply_size()
        if self.chart then self.chart:set_size(self.props.width or 300, self.props.height or 120) end
    end

    local function apply_range()
        if self.chart and self.chart.set_range then
            self.chart:set_range(axis_primary_y(), self.props.range_min or 0, self.props.range_max or 100)
        end
    end

    local function apply_grid()
        if self.chart and self.chart.set_div_line_count then
            self.chart:set_div_line_count(self.props.div_y or 3, self.props.div_x or 0)
        end
    end

    local function apply_appearance()
        if not self.chart then return end
        local part_main = lv.PART_MAIN or 0
        local part_items = lv.PART_ITEMS or 0x050000
        local point_size = self.props.show_points and (tonumber(self.props.point_size) or 3) or 0

        if self.chart.set_style_bg_color then
            self.chart:set_style_bg_color(common.colorToNumber(self.props.bg_color, 0xffffff), part_main)
        end
        if self.chart.set_style_bg_opa then self.chart:set_style_bg_opa(lv.OPA_COVER or 255, part_main) end
        if self.chart.set_style_border_color then
            self.chart:set_style_border_color(common.colorToNumber(self.props.axis_color, 0x808894), part_main)
        end
        if self.chart.set_style_line_color then
            self.chart:set_style_line_color(common.colorToNumber(self.props.grid_color, 0xd8dee6), part_main)
        end
        if self.chart.set_style_line_width then
            self.chart:set_style_line_width(tonumber(self.props.line_width) or 2, part_items)
        end
        if self.chart.set_style_width  then self.chart:set_style_width(point_size, part_items) end
        if self.chart.set_style_height then self.chart:set_style_height(point_size, part_items) end
    end

    local function apply_point_count()
        if self.chart and self.chart.set_point_count then
            self.chart:set_point_count(math.floor(tonumber(self.props.point_count) or 300))
        end
    end

    function self:push_value(value, silent)
        local min_value = tonumber(self.props.range_min) or 0
        local max_value = tonumber(self.props.range_max) or 100
        if max_value < min_value then max_value = min_value end
        local next_value = clamp(value, min_value, max_value)
        self.props.current_value = next_value
        if self.chart and self.series and self.chart.set_next_value then
            self.chart:set_next_value(self.series, next_value)
            if self.chart.refresh then self.chart:refresh() end
        end
        if not silent then fire("updated", next_value) end
        return next_value
    end

    local function rebuild_series()
        if not self.chart then return end
        self.series = nil
        if self.chart.add_series then
            self.series = self.chart:add_series(common.colorToNumber(self.props.series_color, 0x2196f3), axis_primary_y())
        end
        if not self.series then return end

        local data = parse_series(self.props.initial_data)
        if #data == 0 then data = { self.props.current_value or 50 } end
        for _, value in ipairs(data) do
            self:push_value(value, true)
        end
        if self.chart.refresh then self.chart:refresh() end
    end

    local function apply_enabled()
        if not self.chart then return end
        local disabled = lv.STATE_DISABLED or 0x0080
        if self.props.enabled then
            if self.chart.clear_state then self.chart:clear_state(disabled) end
        else
            if self.chart.add_state then self.chart:add_state(disabled) end
        end
    end

    local function apply_visible()
        if not self.chart then return end
        if self.props.visible then
            if self.chart.clear_flag and lv.OBJ_FLAG_HIDDEN then self.chart:clear_flag(lv.OBJ_FLAG_HIDDEN) end
        else
            if self.chart.add_flag and lv.OBJ_FLAG_HIDDEN then self.chart:add_flag(lv.OBJ_FLAG_HIDDEN) end
        end
    end

    local function random_value()
        local min_value = tonumber(self.props.range_min) or 0
        local max_value = tonumber(self.props.range_max) or 100
        if max_value < min_value then max_value = min_value end
        return math.random(min_value, max_value)
    end

    function self.draw()
        self.chart = lv.chart_create(parent)
        self.chart:set_pos(self.props.x or 0, self.props.y or 0)
        self.chart:set_size(self.props.width or 300, self.props.height or 120)
        if self.chart.set_type then self.chart:set_type(lv.CHART_TYPE_LINE or 1) end
        if self.chart.set_update_mode then self.chart:set_update_mode(lv.CHART_UPDATE_MODE_SHIFT or 0) end
        apply_point_count()
        apply_range()
        apply_grid()
        apply_appearance()
        rebuild_series()
        apply_enabled()
        apply_visible()

        if self.chart.add_event_cb and lv.EVENT_CLICKED then
            local cb = function() fire("clicked") end
            self.chart:add_event_cb(cb, lv.EVENT_CLICKED, nil)
            table.insert(self._cb_handles, { event = "clicked", cb = cb })
        end
    end

    local function delete_chart()
        self.series = nil
        if self.chart and self.chart.del then
            self.chart:del()
        elseif self.chart and lv.obj_del then
            lv.obj_del(self.chart)
        end
        self.chart = nil
        self._cb_handles = {}
    end

    local function redraw_chart()
        local was_running = self.timer ~= nil
        if was_running then self:stop() end
        delete_chart()
        self.draw()
        if was_running and self.props.auto_update then self:start() end
    end

    function self:update(value)
        return self:push_value(value ~= nil and value or random_value(), false)
    end

    function self:start()
        if self.timer then return end
        self.timer = lv.timer_create(function()
            self:update()
        end, self.props.update_interval or 1000)
    end

    function self:stop()
        if self.timer then
            lv.timer_delete(self.timer)
            self.timer = nil
        end
    end

    self.draw()

    if self.props.auto_update then
        self:start()
    end

    function self:on(event_name, callback)
        if not self._event_listeners[event_name] then
            print("[trend_chart] unsupported event:", event_name)
            return
        end
        table.insert(self._event_listeners[event_name], callback)
    end

    function self:get_property(name) return self.props[name] end

    function self:set_property(name, value)
        self.props[name] = value
        if name == "x" or name == "y" then
            apply_pos()
        elseif name == "width" or name == "height" then
            apply_size()
        elseif name == "point_count" then
            redraw_chart()
        elseif name == "update_interval" then
            if self.timer then self:stop(); self:start() end
        elseif name == "range_min" or name == "range_max" then
            apply_range()
        elseif name == "div_y" or name == "div_x" then
            apply_grid()
        elseif name == "initial_data" or name == "series_color" then
            redraw_chart()
        elseif name == "current_value" then
            self:push_value(value, false)
        elseif name == "bg_color" or name == "grid_color" or name == "axis_color"
                or name == "line_width" or name == "show_points" or name == "point_size" then
            apply_appearance()
        elseif name == "auto_update"  then
            if self.props.auto_update then self:start() else self:stop() end
        elseif name == "enabled" then
            apply_enabled()
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

    function self:apply_properties(t)
        for k, v in pairs(t) do self:set_property(k, v) end
        return true
    end

    function self:to_state() return self:get_properties() end

    function self:destroy()
        self:stop()
        self._event_listeners = { updated = {}, clicked = {} }
        delete_chart()
    end

    return self
end

return TrendChart
