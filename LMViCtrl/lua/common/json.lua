-- json.lua
-- 简单的 JSON 编码/解码模块

local json = {}

-- 编码 Lua 值为 JSON 字符串
local function encode_value(val, indent, level)
    local t = type(val)
    level = level or 0
    local spacing = indent and string.rep("  ", level) or ""
    local newline = indent and "\n" or ""
    
    if t == "nil" then
        return "null"
    elseif t == "boolean" then
        return val and "true" or "false"
    elseif t == "number" then
        if val ~= val then
            return "null"  -- NaN
        elseif val >= math.huge then
            return "null"  -- Infinity
        elseif val <= -math.huge then
            return "null"  -- -Infinity
        else
            return tostring(val)
        end
    elseif t == "string" then
        -- 转义特殊字符
        local escaped = val:gsub('[\\"\x00-\x1f]', function(c)
            local replacements = {
                ['\\'] = '\\\\',
                ['"'] = '\\"',
                ['\n'] = '\\n',
                ['\r'] = '\\r',
                ['\t'] = '\\t',
                ['\b'] = '\\b',
                ['\f'] = '\\f',
            }
            return replacements[c] or string.format('\\u%04x', c:byte())
        end)
        return '"' .. escaped .. '"'
    elseif t == "table" then
        -- 检查是否为数组
        local is_array = true
        local max_index = 0
        for k, _ in pairs(val) do
            if type(k) ~= "number" or k <= 0 or math.floor(k) ~= k then
                is_array = false
                break
            end
            if k > max_index then
                max_index = k
            end
        end
        
        -- 空表检查
        if next(val) == nil then
            return "{}"
        end
        
        -- 检查数组是否连续
        if is_array then
            for i = 1, max_index do
                if val[i] == nil then
                    is_array = false
                    break
                end
            end
        end
        
        local parts = {}
        local inner_spacing = indent and string.rep("  ", level + 1) or ""
        
        if is_array then
            for i = 1, max_index do
                table.insert(parts, inner_spacing .. encode_value(val[i], indent, level + 1))
            end
            return "[" .. newline .. table.concat(parts, "," .. newline) .. newline .. spacing .. "]"
        else
            -- 对象
            local keys = {}
            for k in pairs(val) do
                if type(k) == "string" then
                    table.insert(keys, k)
                end
            end
            table.sort(keys)
            
            for _, k in ipairs(keys) do
                local v = val[k]
                local key_str = encode_value(k, false, 0)
                local val_str = encode_value(v, indent, level + 1)
                table.insert(parts, inner_spacing .. key_str .. ": " .. val_str)
            end
            return "{" .. newline .. table.concat(parts, "," .. newline) .. newline .. spacing .. "}"
        end
    else
        error("Cannot encode type: " .. t)
    end
end

-- 编码函数
function json.encode(val, pretty)
    return encode_value(val, pretty, 0)
end

-- 解码 JSON 字符串
local function decode_scan_whitespace(str, pos)
    while pos <= #str do
        local c = str:sub(pos, pos)
        if c == ' ' or c == '\t' or c == '\n' or c == '\r' then
            pos = pos + 1
        else
            break
        end
    end
    return pos
end

local function decode_scan_string(str, pos)
    pos = pos + 1  -- 跳过开头的引号
    local result = ""
    while pos <= #str do
        local c = str:sub(pos, pos)
        if c == '"' then
            return result, pos + 1
        elseif c == '\\' then
            pos = pos + 1
            local escape = str:sub(pos, pos)
            if escape == 'n' then
                result = result .. '\n'
            elseif escape == 'r' then
                result = result .. '\r'
            elseif escape == 't' then
                result = result .. '\t'
            elseif escape == 'b' then
                result = result .. '\b'
            elseif escape == 'f' then
                result = result .. '\f'
            elseif escape == '"' then
                result = result .. '"'
            elseif escape == '\\' then
                result = result .. '\\'
            elseif escape == '/' then
                result = result .. '/'
            elseif escape == 'u' then
                local hex = str:sub(pos + 1, pos + 4)
                local code = tonumber(hex, 16)
                if code then
                    if code < 128 then
                        result = result .. string.char(code)
                    elseif code < 2048 then
                        result = result .. string.char(192 + math.floor(code / 64), 128 + code % 64)
                    else
                        result = result .. string.char(224 + math.floor(code / 4096), 128 + math.floor(code / 64) % 64, 128 + code % 64)
                    end
                    pos = pos + 4
                end
            end
            pos = pos + 1
        else
            result = result .. c
            pos = pos + 1
        end
    end
    error("Unterminated string")
end

local decode_scan_value  -- 前向声明

local function decode_scan_array(str, pos)
    pos = pos + 1  -- 跳过 [
    local result = {}
    local index = 1
    
    pos = decode_scan_whitespace(str, pos)
    if str:sub(pos, pos) == ']' then
        return result, pos + 1
    end
    
    while pos <= #str do
        local val
        val, pos = decode_scan_value(str, pos)
        result[index] = val
        index = index + 1
        
        pos = decode_scan_whitespace(str, pos)
        local c = str:sub(pos, pos)
        if c == ']' then
            return result, pos + 1
        elseif c == ',' then
            pos = pos + 1
            pos = decode_scan_whitespace(str, pos)
        else
            error("Expected ',' or ']' at position " .. pos)
        end
    end
    error("Unterminated array")
end

local function decode_scan_object(str, pos)
    pos = pos + 1  -- 跳过 {
    local result = {}
    
    pos = decode_scan_whitespace(str, pos)
    if str:sub(pos, pos) == '}' then
        return result, pos + 1
    end
    
    while pos <= #str do
        pos = decode_scan_whitespace(str, pos)
        
        -- 读取键
        if str:sub(pos, pos) ~= '"' then
            error("Expected string key at position " .. pos)
        end
        local key
        key, pos = decode_scan_string(str, pos)
        
        pos = decode_scan_whitespace(str, pos)
        if str:sub(pos, pos) ~= ':' then
            error("Expected ':' at position " .. pos)
        end
        pos = pos + 1
        
        -- 读取值
        pos = decode_scan_whitespace(str, pos)
        local val
        val, pos = decode_scan_value(str, pos)
        result[key] = val
        
        pos = decode_scan_whitespace(str, pos)
        local c = str:sub(pos, pos)
        if c == '}' then
            return result, pos + 1
        elseif c == ',' then
            pos = pos + 1
        else
            error("Expected ',' or '}' at position " .. pos)
        end
    end
    error("Unterminated object")
end

local function decode_scan_number(str, pos)
    local start = pos
    if str:sub(pos, pos) == '-' then
        pos = pos + 1
    end
    while pos <= #str and str:sub(pos, pos):match('[0-9]') do
        pos = pos + 1
    end
    if pos <= #str and str:sub(pos, pos) == '.' then
        pos = pos + 1
        while pos <= #str and str:sub(pos, pos):match('[0-9]') do
            pos = pos + 1
        end
    end
    if pos <= #str and str:sub(pos, pos):match('[eE]') then
        pos = pos + 1
        if pos <= #str and str:sub(pos, pos):match('[+-]') then
            pos = pos + 1
        end
        while pos <= #str and str:sub(pos, pos):match('[0-9]') do
            pos = pos + 1
        end
    end
    local num_str = str:sub(start, pos - 1)
    return tonumber(num_str), pos
end

function decode_scan_value(str, pos)
    pos = decode_scan_whitespace(str, pos)
    local c = str:sub(pos, pos)
    
    if c == '"' then
        return decode_scan_string(str, pos)
    elseif c == '{' then
        return decode_scan_object(str, pos)
    elseif c == '[' then
        return decode_scan_array(str, pos)
    elseif c == 't' then
        if str:sub(pos, pos + 3) == 'true' then
            return true, pos + 4
        end
        error("Invalid value at position " .. pos)
    elseif c == 'f' then
        if str:sub(pos, pos + 4) == 'false' then
            return false, pos + 5
        end
        error("Invalid value at position " .. pos)
    elseif c == 'n' then
        if str:sub(pos, pos + 3) == 'null' then
            return nil, pos + 4
        end
        error("Invalid value at position " .. pos)
    elseif c == '-' or c:match('[0-9]') then
        return decode_scan_number(str, pos)
    else
        error("Invalid value at position " .. pos .. ": " .. c)
    end
end

-- 解码函数
function json.decode(str)
    if type(str) ~= "string" then
        error("Expected string, got " .. type(str))
    end
    if str == "" then
        error("Empty string")
    end
    
    -- 移除 UTF-8 BOM（如果存在）
    if str:sub(1, 3) == "\239\187\191" then
        str = str:sub(4)
    end
    
    -- 移除开头的空白字符（包括换行符等）
    local start_pos = 1
    while start_pos <= #str do
        local c = str:sub(start_pos, start_pos)
        if c == ' ' or c == '\t' or c == '\n' or c == '\r' then
            start_pos = start_pos + 1
        else
            break
        end
    end
    
    if start_pos > #str then
        error("Empty content after trimming")
    end
    
    -- 如果有前导空白，调整字符串
    if start_pos > 1 then
        str = str:sub(start_pos)
    end
    
    local result, pos = decode_scan_value(str, 1)
    pos = decode_scan_whitespace(str, pos)
    if pos <= #str then
        error("Trailing data at position " .. pos)
    end
    return result
end

return json
