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

local M = {}

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

-- ─── 2. 列出 widgets/ 下的 *.lua（仅取短名，不带扩展名） ──────────────
local function list_widget_basenames()
    local dir = detect_widgets_dir()
    if not dir then
        log_warn("could not locate widgets/ directory via package.path")
        return {}
    end

    local sep = package.config:sub(1, 1)
    local cmd
    if sep == "\\" then
        cmd = string.format('dir /b "%s\\*.lua" 2>NUL', dir)
    else
        cmd = string.format('ls "%s"/*.lua 2>/dev/null', dir)
    end

    local p = io.popen(cmd)
    local names = {}
    if not p then
        log_warn("io.popen failed for: %s", cmd)
        return names
    end
    for line in p:lines() do
        local short = line:match("([^/\\]+)%.lua$")
        if short then table.insert(names, short) end
    end
    p:close()
    return names
end

-- ─── 3. 加载并注册全部 widget 模块 ───────────────────────────────────
-- registry 同时按 widgetId / type / 短名 索引，方便兼容
local function build_registry()
    local registry = {}
    local count = 0

    local basenames = list_widget_basenames()
    for _, short in ipairs(basenames) do
        local modname = "widgets." .. short
        local ok, mod = pcall(require, modname)
        if not ok or type(mod) ~= "table" then
            log_warn("failed to load %s: %s", modname, tostring(mod))
        else
            local meta = mod.__widget_meta or {}
            local keys = {}
            if meta.id   and meta.id   ~= "" then keys[#keys + 1] = meta.id   end
            if meta.type and meta.type ~= "" then keys[#keys + 1] = meta.type end
            keys[#keys + 1] = short
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
    return ret
end

-- ─── 6. 构建单个 screen ─────────────────────────────────────────────
local function build_screen(scr)
    local screen = lv.obj_create(nil)

    -- TODO: 按 scr.bgColor 设置屏幕背景色（待 lv 绑定补 set_style_bg_color）

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

-- ─── 7. 主入口 ──────────────────────────────────────────────────────
--- @param project table  由 compileToLua 生成的工程数据
function M.run(project)
    if type(project) ~= "table" then
        log_warn("run: invalid project")
        return
    end
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

    local first_obj
    for i, scr in ipairs(screens) do
        local obj = build_screen(scr)
        if i == 1 then first_obj = obj end
    end

    -- 把首屏切换为活动屏（若绑定暴露了 screen_load）
    print(first_obj)
	print(type(lv.screen_load))
	print(type(lv.scr_load))
    if first_obj and type(lv.screen_load) == "function" then
        lv.screen_load(first_obj)
    elseif first_obj and type(lv.scr_load) == "function" then
        lv.scr_load(first_obj)
    end
end

return M
