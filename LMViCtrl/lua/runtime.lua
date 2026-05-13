--------------------------------------------------------------------
-- runtime.lua
--
-- QtLvglDesigner 运行时引导：由设计器编译产物（<project>.lua）通过
--     local runtime = require("runtime"); runtime.run(project)
-- 调用。
--
-- 控件解析约定：
--   * 工程中使用的 widgetId（如 "lv_button_basic"）来自每个 widget
--     模块文件里 __widget_meta.id 字段。
--   * widget 模块文件名为短名（如 widgets/button.lua），不一定与 id 同名。
--   * 因此 runtime 启动时会扫描 widgets/ 目录的所有 *.lua，
--     require 后按 __widget_meta.id（兼容 type 与文件短名）建立分发表。
--   * 每个模块需提供工厂函数 mod.new(parent, state)，返回带 .btn /
--     :on / :destroy 等方法的 self 对象（具体见各 widget 实现）。
--------------------------------------------------------------------

local lv = require("lvgl")
local ok_nav, common = pcall(require, "common.common")
if not ok_nav then
    io.stderr:write("[runtime] warning: common.common module not found, some features may be unavailable\n")
    common = {}
end
local parse_hex_color = common.colorToNumber
local ok_data_client, data_client = pcall(require, "common.data_client")
local M = {}

-- 全局命名的控件实例表：可在事件代码中通过 widgets.<Name> 访问
-- 例如：widgets.Button_1:set_property("label", "Hi")
M.widgets = {}
_G.widgets = M.widgets
if ok_data_client and data_client then
    M.data_client = data_client
    _G.DataClient = data_client
else
    io.stderr:write("[runtime] warning: common.data_client module not found, data interface unavailable\n")
end

-- 事件处理代码运行时的环境：
--   1) widgets.<Name>      —— 始终可用
--   2) <Name>              —— 直接以控件名作为全局访问（按需在 widgets 表里查找）
--   3) 其他名字回退到 _G   —— 让 print 等标准库可用
-- 写入未知名字时落到 _G，避免污染 widgets 表
local g_handler_env = setmetatable({ widgets = M.widgets }, {
    __index = function(_, k)
        local w = M.widgets[k]
        if w ~= nil then return w end
        return _G[k]
    end,
    __newindex = _G,
})

local function log_warn(fmt, ...)
    io.stderr:write(string.format("[runtime] " .. fmt .. "\n", ...))
end
local function log_info(fmt, ...)
    io.stdout:write(string.format("[runtime] " .. fmt .. "\n", ...))
end

-- ─── 1. 定位工程 widgets/ 目录 ────────────────────────────────────────
-- 通过 package.searchpath 探测一个我们必然部署在工程目录的入口
-- （任何 widgets.<x>），从中反推出 widgets/ 的绝对路径。
local function detect_widgets_dir()
    local probe_candidates = {
        "widgets.button", "widgets.label", "widgets.slider",
        "widgets.checkbox", "widgets.bar",  "widgets.arc",
    }
    for _, mod in ipairs(probe_candidates) do
        local p = package.searchpath(mod, package.path)
        if p then
            local dir = p:match("^(.-)[/\\][^/\\]+%.lua$")
            if dir and dir ~= "" then return dir end
        end
    end
    return nil
end

-- ─── 2. 列出 widgets/ 下的 *.lua（递归子目录，返回模块的相对路径短名） ──
-- 例如 widgets/button.lua            -> "button"
--      widgets/custom/valve.lua      -> "custom/valve"
local function load_widget_manifest()
    local ok, manifest = pcall(require, "manifest")
    if not ok or type(manifest) ~= "table" then
        log_warn("manifest.lua not available: %s", tostring(manifest))
        return {}
    end

    local names = {}
    for _, name in ipairs(manifest) do
        if type(name) == "string" and name ~= "" then
            names[#names + 1] = name
        end
    end
    return names
end

local function list_widget_basenames()
    local dir = detect_widgets_dir()
    if not dir then
        log_warn("could not locate widgets/ directory via package.path")
        return load_widget_manifest()
    end

    local sep = package.config:sub(1, 1)
    local cmd
    if sep == "\\" then
        -- /s 递归，/b 仅文件名（带相对路径），/a-d 排除目录
        cmd = string.format('dir /s /b /a-d "%s\\*.lua" 2>NUL', dir)
    else
        cmd = string.format('find "%s" -type f -name "*.lua" 2>/dev/null', dir)
    end

    local names = {}
    local ok, p = false, nil
    if type(io) == "table" and type(io.popen) == "function" then
        ok, p = pcall(io.popen, cmd)
    end
    if not ok or not p then
        log_warn("io.popen unavailable, fallback to manifest.lua")
        return load_widget_manifest()
    end

    -- 计算相对于 widgets/ 的路径（去扩展名），统一用 "/" 分隔
    -- Windows 上 dir 输出可能用 "\\"，detect_widgets_dir 可能用 "/"，
    -- 因此先把两边都归一化为 "/" 再做（大小写不敏感的）前缀剥离。
    local prefix_norm = dir:gsub("\\", "/"):lower()
    local prefix_len  = #prefix_norm
    for line in p:lines() do
        local full = line:gsub("\r$", "")
        if full ~= "" then
            local norm = full:gsub("\\", "/")
            local rel
            if norm:sub(1, prefix_len):lower() == prefix_norm then
                rel = norm:sub(prefix_len + 1)
            else
                -- 退化：找最后一次出现的 "/widgets/"
                local s = norm:lower():find("/widgets/", 1, true)
                if s then rel = norm:sub(s + #"/widgets/") else rel = norm end
            end
            -- 去掉前导分隔符
            rel = rel:gsub("^[/\\]+", "")
            -- 去掉 .lua 后缀
            rel = rel:gsub("%.[Ll][Uu][Aa]$", "")
            if rel ~= "" and rel ~= "manifest" then names[#names + 1] = rel end
        end
    end
    p:close()
    return names
end

-- ─── 3. 加载并注册全部 widget 模块 ───────────────────────────────────
-- registry 同时按 widgetId / type / 短名（含子目录）索引，方便兼容
local function build_registry()
    local registry = {}
    local count = 0

    local basenames = list_widget_basenames()
    for _, short in ipairs(basenames) do
        -- short 形如 "button" 或 "custom/valve"，转成 "widgets.button" / "widgets.custom.valve"
        local modname = "widgets." .. (short:gsub("/", "."))
        local ok, mod = pcall(require, modname)
        if not ok or type(mod) ~= "table" then
            log_warn("failed to load %s: %s", modname, tostring(mod))
        else
            local meta = mod.__widget_meta or {}
            local keys = {}
            if meta.id   and meta.id   ~= "" then keys[#keys + 1] = meta.id   end
            if meta.type and meta.type ~= "" then keys[#keys + 1] = meta.type end
            -- 完整相对短名（含目录），以及最末段（去目录）的 leaf 名
            keys[#keys + 1] = short
            local leaf = short:match("([^/]+)$")
            if leaf and leaf ~= short then keys[#keys + 1] = leaf end
            for _, k in ipairs(keys) do
                if registry[k] == nil then
                    registry[k] = mod
                end
            end
            count = count + 1
        end
    end
    log_info("loaded %d widget modules", count)
    return registry
end

-- 全局 registry：runtime 整个生命周期共用
local g_registry = nil
local function lookup_widget(widget_id)
    if not g_registry then g_registry = build_registry() end
    return g_registry[widget_id]
end

-- 推断工程根目录（即 widgets/ 的父目录）
local function detect_project_root()
    local wdir = detect_widgets_dir()
    if not wdir then return nil end
    -- wdir 形如 ".../<project>/widgets"，去掉末尾的 "/widgets" 或 "\\widgets"
    local root = wdir:match("^(.-)[/\\][^/\\]+$")
    return root
end

-- 加载工程字体并设为默认字体
-- project.font = { file = "fonts/xxx.ttf", size = 16 }
local function setup_project_font(project)
    local f = project.font
    if type(f) ~= "table" then return end
    if not f.file or f.file == "" then return end
    if type(lv.tiny_ttf_create_file) ~= "function" then
        log_warn("font: lv.tiny_ttf_create_file not available, skipping")
        return
    end

    local root = detect_project_root()
    if not root then
        log_warn("font: cannot resolve project root, skipping")
        return
    end
    local sep = package.config:sub(1, 1)
    local abs = root .. sep .. f.file:gsub("[/\\]", sep)
    local size = tonumber(f.size) or 16

    local font = lv.tiny_ttf_create_file(abs, size)
    if not font then
        log_warn("font: failed to load %s @%d", abs, size)
        return
    end
    if type(lv.set_default_font) == "function" then
        lv.set_default_font(font)
    end
    log_info("font: loaded %s @%dpx", abs, size)
end

-- ─── 4. 把 instance 整理成 widget 工厂期望的 state ───────────────────
-- WidgetInstance.properties 已含全部属性键值；同时 instance 顶层还有
-- 实时的 x/y/width/height，作为权威值覆盖到 state 中。
local function build_state(inst)
    local state = {}
    if type(inst.properties) == "table" then
        for k, v in pairs(inst.properties) do state[k] = v end
    end
    if inst.x      ~= nil then state.x      = inst.x      end
    if inst.y      ~= nil then state.y      = inst.y      end
    if inst.width  ~= nil then state.width  = inst.width  end
    if inst.height ~= nil then state.height = inst.height end
    if inst.visible ~= nil and state.visible == nil then
        state.visible = inst.visible
    end
    return state
end

-- 把字符串形式的 Lua 事件处理代码编译成函数
-- 上下文中暴露 self（控件实例）与 e（事件对象）
local function compile_event_handler(code, widget_id, prop_name)
    if type(code) ~= "string" then return nil end
    -- 去掉首尾空白后判定是否为空
    if code:match("^%s*$") then return nil end

    local loader = load or loadstring
    if type(loader) ~= "function" then
        log_warn("event: no load/loadstring available")
        return nil
    end

    -- 把用户代码包装为接收 (self, e) 的函数体
    local chunk_src = "local self, e = ...\n" .. code
    local chunk_name = string.format("=[%s.%s]", tostring(widget_id), tostring(prop_name))

    local fn, err
    if load then
        fn, err = load(chunk_src, chunk_name, "t", g_handler_env)
    else
        fn, err = loadstring(chunk_src, chunk_name)
        if fn and type(setfenv) == "function" then setfenv(fn, g_handler_env) end
    end
    if not fn then
        log_warn("event: compile failed for %s.%s: %s",
                 tostring(widget_id), tostring(prop_name), tostring(err))
        return nil
    end
    return fn
end

-- 把 inst.properties 里的 on_xxx_handler 代码绑定到 widget:on(event, fn)
local function bind_event_handlers(widget, mod, inst)
    if type(widget) ~= "table" or type(widget.on) ~= "function" then return end
    local meta = mod.__widget_meta
    if type(meta) ~= "table" then return end
    local ev_props = meta.event_properties
    if type(ev_props) ~= "table" then return end

    local props = inst.properties or {}
    for _, ep in ipairs(ev_props) do
        local code = props[ep.name]
        local event_name = ep.event
        if event_name and code and code ~= "" then
            local fn = compile_event_handler(code, inst.widgetId, ep.name)
            if fn then
                local ok, err = pcall(widget.on, widget, event_name, fn)
                if not ok then
                    log_warn("event: bind %s.%s failed: %s",
                             tostring(inst.widgetId), tostring(event_name), tostring(err))
                end
            end
        end
    end
end

local function normalize_event_binding(event_binding)
    if type(event_binding) ~= "table" then return "sequence", nil end
    if event_binding[1] ~= nil then return "sequence", event_binding end

    local actions = event_binding.actions
    local mode = event_binding.executionMode or event_binding.execution_mode or "sequence"
    if mode ~= "parallel" then mode = "sequence" end
    if type(actions) ~= "table" then return mode, nil end
    return mode, actions
end

local function action_delay_ms(action)
    if type(action) ~= "table" then return 0 end
    local delay = action.delayMs or action.delay_ms
    if delay == nil and type(action.params) == "table" then
        delay = action.params.delayMs or action.params.delay_ms
    end
    delay = tonumber(delay) or 0
    if delay < 0 then delay = 0 end
    return delay
end

local function run_after(delay_ms, fn)
    if delay_ms <= 0 then
        fn()
        return
    end

    if type(lv.timer_create) ~= "function" then
        log_warn("event action: lv.timer_create unavailable, running delayed action immediately")
        fn()
        return
    end

    local timer
    local ok, err = pcall(function()
        timer = lv.timer_create(function(t)
            if type(lv.timer_delete) == "function" then
                pcall(lv.timer_delete, t or timer)
            end
            fn()
        end, delay_ms, nil)
    end)
    if not ok or not timer then
        log_warn("event action: failed to create delay timer: %s", tostring(err))
        fn()
    end
end

local function condition_allows_action(action, self, e)
    if type(action) ~= "table" or action.enabled == false then return false end
    local condition = action.condition
    if type(condition) ~= "string" or condition:match("^%s*$") then return true end

    local loader = load or loadstring
    if type(loader) ~= "function" then
        log_warn("event action: no load/loadstring available for condition")
        return false
    end

    local chunk_src = "local self, e, action = ...\nreturn (" .. condition .. ")"
    local chunk_name = string.format("=[event_action.%s.condition]", tostring(action.id or action.type))
    local fn, err
    if load then
        fn, err = load(chunk_src, chunk_name, "t", g_handler_env)
    else
        fn, err = loadstring(chunk_src, chunk_name)
        if fn and type(setfenv) == "function" then setfenv(fn, g_handler_env) end
    end
    if not fn then
        log_warn("event action: condition compile failed: %s", tostring(err))
        return false
    end

    local ok, result = pcall(fn, self, e, action)
    if not ok then
        log_warn("event action: condition failed: %s", tostring(result))
        return false
    end
    return not not result
end

local function execute_event_action(action, self, e)
    if not condition_allows_action(action, self, e) then return end
    local action_type = action.type

    if action_type == "load_screen" then
        local pm = _G.PageManager
        if not pm then
            log_warn("event action: PageManager unavailable")
            return
        end
        if action.targetId and action.targetId ~= "" and type(pm.goto_page_by_id) == "function" then
            pm.goto_page_by_id(action.targetId)
        elseif action.targetName and action.targetName ~= "" and type(pm.goto_page_by_name) == "function" then
            pm.goto_page_by_name(action.targetName)
        elseif type(pm.goto_page) == "function" and type(action.params) == "table" then
            pm.goto_page(action.params.page_index or action.params.index or 1)
        end
        return
    end

    if action_type == "custom_code" then
        local fn = compile_event_handler(action.code, "event_action", action.id or "custom_code")
        if fn then
            local ok, err = pcall(fn, self, e)
            if not ok then log_warn("event action custom_code failed: %s", tostring(err)) end
        end
        return
    end

    if action_type == "set_property" then
        local target = action.targetName and M.widgets[action.targetName]
        local params = action.params or {}
        if target and type(target.set_property) == "function" then
            target:set_property(params.property, params.value)
        else
            log_warn("event action: target widget not found or cannot set property: %s", tostring(action.targetName))
        end
        return
    end

    if action_type == "call_method" then
        local target = action.targetName and M.widgets[action.targetName]
        local method = action.method or (action.params and action.params.method)
        if target and method and type(target[method]) == "function" then
            target[method](target)
        else
            log_warn("event action: cannot call %s on %s", tostring(method), tostring(action.targetName))
        end
        return
    end

    if action_type == "freemaster" then
        if _G.Freemaster and type(_G.Freemaster.execute) == "function" then
            _G.Freemaster.execute(action)
        else
            log_warn("event action: Freemaster API unavailable for target %s", tostring(action.targetName))
        end
        return
    end

    log_warn("event action: unsupported action type %s", tostring(action_type))
end

local function execute_event_actions_sequence(actions, self, e, index)
    index = index or 1
    local action = actions[index]
    if action == nil then return end
    run_after(action_delay_ms(action), function()
        execute_event_action(action, self, e)
        execute_event_actions_sequence(actions, self, e, index + 1)
    end)
end

local function execute_event_actions_parallel(actions, self, e)
    for _, action in ipairs(actions) do
        run_after(action_delay_ms(action), function()
            execute_event_action(action, self, e)
        end)
    end
end

local function bind_event_actions(widget, inst)
    if type(widget) ~= "table" or type(widget.on) ~= "function" then return end
    if type(inst.events) ~= "table" then return end

    for event_name, event_binding in pairs(inst.events) do
        local execution_mode, actions = normalize_event_binding(event_binding)
        if type(actions) == "table" and #actions > 0 then
            local ok, err = pcall(widget.on, widget, event_name, function(self, e)
                if execution_mode == "parallel" then
                    execute_event_actions_parallel(actions, self, e)
                else
                    execute_event_actions_sequence(actions, self, e, 1)
                end
            end)
            if not ok then
                log_warn("event: bind action %s failed: %s", tostring(event_name), tostring(err))
            end
        end
    end
end

-- ─── 5. 构建单个控件 ─────────────────────────────────────────────────
local function build_widget(parent, inst)
    local mod = lookup_widget(inst.widgetId)
    if not mod then
        log_warn("widget module not found for widgetId=%s", tostring(inst.widgetId))
        return nil
    end

    if type(mod.new) ~= "function" then
        log_warn("widget %s has no new() factory", tostring(inst.widgetId))
        return nil
    end

    local state = build_state(inst)
    local ok, ret = pcall(mod.new, parent, state)
    if not ok then
        log_warn("new() failed for %s: %s", tostring(inst.widgetId), tostring(ret))
        return nil
    end

    -- 按 instance.name 注册到全局 widgets 表，便于跨控件互操作
    local nm = inst.name
    if type(nm) == "string" and nm ~= "" then
        if M.widgets[nm] ~= nil then
            log_warn("widget name conflict: %s (overwriting)", nm)
        end
        M.widgets[nm] = ret
    end

    bind_event_handlers(ret, mod, inst)
    bind_event_actions(ret, inst)
    return ret
end

-- ─── 6. 构建单个 screen ─────────────────────────────────────────────
local function build_screen(scr)
    local screen = lv.obj_create(nil)

    local bg = parse_hex_color(scr.bgColor,nil)
    if bg and type(screen.set_style_bg_color) == "function" then
        screen:set_style_bg_color(bg, 0)
        screen:set_style_bg_opa(255, 0)
    end

    if type(scr.widgets) == "table" then
        -- 按 zOrder 升序构建，使 z 大的覆盖在上
        local list = {}
        for i, w in ipairs(scr.widgets) do list[i] = w end
        table.sort(list, function(a, b) return (a.zOrder or 0) < (b.zOrder or 0) end)
        for _, inst in ipairs(list) do
            build_widget(screen, inst)
        end
    end
    return screen
end

local function load_screen_object(screen_obj)
    if not screen_obj then return false end
    if type(lv.screen_load) == "function" then
        lv.screen_load(screen_obj)
        return true
    elseif type(lv.scr_load) == "function" then
        lv.scr_load(screen_obj)
        return true
    end
    log_warn("page: lv.screen_load/lv.scr_load is not available")
    return false
end

local function create_page_manager(pages)
    local manager = {
        pages = pages or {},
        current_index = 0,
    }

    function manager.get_page_count()
        return #manager.pages
    end

    function manager.goto_page(page_index)
        page_index = tonumber(page_index)
        if not page_index then
            log_warn("page: invalid page index")
            return false
        end
        page_index = math.floor(page_index)
        if page_index < 1 or page_index > #manager.pages then
            log_warn("page: index %s out of range 1-%d", tostring(page_index), #manager.pages)
            return false
        end

        local page = manager.pages[page_index]
        if not load_screen_object(page and page.object) then
            return false
        end
        manager.current_index = page_index
        return true
    end

    function manager.goto_page_by_id(screen_id)
        if type(screen_id) ~= "string" or screen_id == "" then return false end
        for i, page in ipairs(manager.pages) do
            if page.id == screen_id then return manager.goto_page(i) end
        end
        log_warn("page: id %s not found", tostring(screen_id))
        return false
    end

    function manager.goto_page_by_name(name)
        if type(name) ~= "string" or name == "" then return false end
        for i, page in ipairs(manager.pages) do
            if page.name == name then return manager.goto_page(i) end
        end
        log_warn("page: name %s not found", tostring(name))
        return false
    end

    manager.select_page = manager.goto_page
    return manager
end

-- ─── 7. 主入口 ──────────────────────────────────────────────────────
--- @param project table  由 compileToLua 生成的工程数据
function M.run(project)
    if type(project) ~= "table" then
        log_warn("run: invalid project")
        return
    end

    -- 重置 widgets 注册表（保留同一张表的引用，便于已持有该表的代码继续生效）
    for k in pairs(M.widgets) do M.widgets[k] = nil end
    if type(project.screens) ~= "table" or #project.screens == 0 then
        log_warn("run: no screens to render")
        return
    end

    -- 1) 字体：在创建任何屏/控件之前完成，使后续 widget 默认使用此字体
    setup_project_font(project)

    -- 2) 按 order 升序，第一个为活动屏
    local screens = {}
    for i, s in ipairs(project.screens) do screens[i] = s end
    table.sort(screens, function(a, b) return (a.order or 0) < (b.order or 0) end)

    local pages = {}
    for i, scr in ipairs(screens) do
        local obj = build_screen(scr)
        pages[i] = {
            id = scr.id,
            name = scr.name,
            order = scr.order,
            data = scr,
            object = obj,
        }
    end

    _G.PageManager = create_page_manager(pages)
    _G.PageManager.goto_page(1)

    if ok_data_client and data_client and type(data_client.start) == "function" then
        local cfg = project.dataClient or project.data_client
        if type(cfg) == "table" and cfg.enabled then
            local ok, err = data_client.start(cfg)
            if not ok then
                log_warn("data client: start failed: %s", tostring(err))
            end
        end
    end
end

return M
