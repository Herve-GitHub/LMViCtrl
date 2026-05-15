local lv = require("lvgl")

local M = {
    values = {},
    status = {},
    connected = false,
    config = {},
}

local message_handlers = {}
local connect_handlers = {}
local error_handlers = {}

local function log(fmt, ...)
    io.stdout:write(string.format("[data_client] " .. fmt .. "\n", ...))
end

local function warn(fmt, ...)
    io.stderr:write(string.format("[data_client] " .. fmt .. "\n", ...))
end

local function normalize_ws_path(path)
    if type(path) ~= "string" or path == "" then return "/ws" end
    if path:sub(1, 1) ~= "/" then return "/" .. path end
    return path
end

local function build_ws_url(config)
    if type(config.url) == "string" and config.url ~= "" then
        return config.url
    end

    local server = config.server or config.host
    if type(server) ~= "string" or server == "" then return nil end
    if server:match("^wss?://") then return server end

    local scheme = config.secure and "wss://" or "ws://"
    return scheme .. server .. normalize_ws_path(config.websocketPath or config.wsPath)
end

local function notify(list, ...)
    for _, fn in ipairs(list) do
        local ok, err = pcall(fn, ...)
        if not ok then warn("callback failed: %s", tostring(err)) end
    end
end

local function apply_bindings(point_id, value, status)
    local bindings = M.config.bindings
    if type(bindings) ~= "table" then return end

    local binding = bindings[point_id]
    if type(binding) ~= "table" then return end

    local list = binding[1] and binding or { binding }
    for _, item in ipairs(list) do
        local widget_name = item.widget or item.target or item.name
        local property = item.property or item.prop or "text"
        local widget = widget_name and widgets and widgets[widget_name]
        if widget and type(widget.set_property) == "function" then
            widget:set_property(property, tostring(value))
        end
    end
end

local function on_connect(connected, state)
    M.connected = not not connected
    log("connection %s (%s)", M.connected and "open" or "closed", tostring(state))
    notify(connect_handlers, M.connected, state)
end

local function on_message(point_id, value, item_status)
    if point_id == nil then return end
    M.values[point_id] = value
    M.status[point_id] = item_status
    apply_bindings(point_id, value, item_status)
    notify(message_handlers, point_id, value, item_status)
end

local function on_error(err)
    warn("network error: %s", tostring(err))
    notify(error_handlers, err)
end

function M.add_message_handler(fn)
    if type(fn) == "function" then message_handlers[#message_handlers + 1] = fn end
end

function M.add_connect_handler(fn)
    if type(fn) == "function" then connect_handlers[#connect_handlers + 1] = fn end
end

function M.add_error_handler(fn)
    if type(fn) == "function" then error_handlers[#error_handlers + 1] = fn end
end

function M.start(config)
    config = config or {}
    M.config = config

    if config.enabled == false then
        log("disabled")
        return true
    end

    if type(lv.start_network_service) ~= "function" then
        return false, "lv.start_network_service unavailable"
    end

    local ok, err = lv.start_network_service()
    if not ok then return false, err or "failed to start network service" end

    if type(lv.set_callbacks) == "function" then
        lv.set_callbacks(on_connect, on_message, on_error)
    end

    local url = build_ws_url(config)
    if not url then
        log("network service started without websocket url")
        return true
    end

    if type(lv.connect) ~= "function" then return false, "lv.connect unavailable" end
    return lv.connect(url, tonumber(config.timeoutMs) or 5000)
end

function M.stop()
    if type(lv.disconnect) == "function" then lv.disconnect() end
    M.connected = false
end

function M.send(message)
    if type(lv.send) ~= "function" then return false, "lv.send unavailable" end
    return lv.send(message)
end

function M.send_raw(json)
    if type(lv.send_raw) ~= "function" then return false, "lv.send_raw unavailable" end
    return lv.send_raw(json)
end

function M.write(point_id, value)
    if type(lv.write) ~= "function" then return false, "lv.write unavailable" end
    return lv.write(point_id, tostring(value))
end

function M.write_batch(values)
    if type(lv.write_batch) ~= "function" then return false, "lv.write_batch unavailable" end
    return lv.write_batch(values)
end

function M.read(...)
    if type(lv.read) ~= "function" then return false, "lv.read unavailable" end
    return lv.read(...)
end

function M.read_multiple(ids)
    if type(lv.read_multiple) ~= "function" then return false, "lv.read_multiple unavailable" end
    return lv.read_multiple(ids)
end

function M.get_value(point_id)
    return M.values[point_id], M.status[point_id]
end

function M.get_real_data(page, size)
    if type(lv.get_real_data) ~= "function" then return false, "lv.get_real_data unavailable" end
    local server = M.config.httpServer or M.config.server or M.config.host
    local token = M.config.token or ""
    if type(server) ~= "string" or server == "" then return false, "server is empty" end
    return lv.get_real_data(server, token, page or 1, size or 15)
end

function M.query_history(ids_json, count, period, start_time, end_time, agg_type)
    if type(lv.query_sync) ~= "function" then return false, "lv.query_sync unavailable" end
    local server = M.config.httpServer or M.config.server or M.config.host
    local token = M.config.token or ""
    if type(server) ~= "string" or server == "" then return false, "server is empty" end
    return lv.query_sync(server, token, ids_json, count or 100, period or 60,
        start_time or "", end_time or "", agg_type or "LAST")
end

return M
