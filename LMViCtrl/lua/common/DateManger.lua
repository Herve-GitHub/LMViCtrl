local lv = require("lvgl")

local M = {
    values = {},
    bindings = {},   -- 数据点 → 控件列表
}

-- 绑定：数据点 → 控件
function M.bind(point_id, widget_name, prop)
    if not point_id or not widget_name then return end
    prop = prop or "label"

    M.bindings[point_id] = M.bindings[point_id] or {}
    local list = M.bindings[point_id]

    -- 避免重复绑定
    for _, item in ipairs(list) do
        if item.widget == widget_name then
            return
        end
    end

    table.insert(list, {
        widget = widget_name,
        prop = prop
    })
end

-- 更新一个数据点，并自动刷新控件
function M.update_value(point_id, value, status)
    if not point_id then return end

    -- 保存值
    M.values[point_id] = value

    -- 自动刷新绑定的控件
    local list = M.bindings[point_id]
    if not list then return end

    for _, item in ipairs(list) do
        local w = _G.widgets and _G.widgets[item.widget]
        if w and w.set_property then
            w:set_property(item.prop, tostring(value))
        end
    end
end

-- 获取当前值
function M.get_value(point_id)
    return M.values[point_id]
end

-- 批量订阅（只发订阅，不管理连接）
function M.read(...)
    if lv.read then
        return lv.read(...)
    end
end

function M.read_multiple(ids)
    if lv.read_multiple then
        return lv.read_multiple(ids)
    end
end

return M