local lv = require("lvgl")

local M = {
    values = {},
    bindings = {},
    subscribed = {},
}

function M.bind(point_id, widget_name, prop)
    if not point_id or not widget_name then return end
    prop = prop or "label"

    M.bindings[point_id] = M.bindings[point_id] or {}
    local list = M.bindings[point_id]
    for _, item in ipairs(list) do
        if item.widget == widget_name then return end
    end
    table.insert(list, { widget = widget_name, prop = prop })
end

-- 🔥 修复1：安全取控件（从runtime而非_G）
-- 🔥 终极安全获取控件（全局 widgets 表，永不失败）
local function get_widget(name)
    if not name then return nil end
    -- 直接用全局唯一的 widgets 表，不走 runtime 间接获取
    return _G.widgets and _G.widgets[name]
end

-- 🔥 修复2：线程安全更新（LVGL要求主线程执行）
function M.update_value(point_id, value, status)
    if not point_id or value == nil then return end
    M.values[point_id] = value

    local list = M.bindings[point_id]
    if not list then return end

    -- 异步抛到LVGL主线程执行，杜绝线程冲突
    lv.async_call(function()
        for _, item in ipairs(list) do
            local w = get_widget(item.widget)
            if w and type(w.set_property) == "function" then
                w:set_property(item.prop, tostring(value))
            end
        end
    end)
end

-- 修复3：每个点位只订阅一次
function M.read(...)
    local args = {...}
    local batch = {}
    for _, point_id in ipairs(args) do
        if point_id and not M.subscribed[point_id] then
            M.subscribed[point_id] = true
            table.insert(batch, point_id)
        end
    end
    if #batch > 0 and lv.read then
        lv.read(table.unpack(batch))
    end
end

function M.get_value(point_id)
    return M.values[point_id]
end

return M