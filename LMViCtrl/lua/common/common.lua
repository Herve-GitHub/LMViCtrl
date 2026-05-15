local common = {}

--- 将颜色值转换为数字（24 位 RGB，范围 0x000000 ~ 0xffffff）
--- @param c any 输入可以是：
---   - 数字（直接返回）
---   - "#RRGGBB" / "#RGB" / "0xRRGGBB" 字符串
--- @param fallback number|nil 解析失败时使用的回退值；缺省为 0xffffff
--- @return number 转换后的数字颜色值
function common.colorToNumber(c, fallback)
    fallback = fallback or 0xffffff

    if type(c) == "number" then
        return c
    end

    if type(c) ~= "string" then
        return fallback
    end

    -- #RRGGBB
    if c:match("^#%x%x%x%x%x%x$") then
        return tonumber(c:sub(2), 16) or fallback
    end

    -- #RGB （短写，扩展为 #RRGGBB）
    if c:match("^#%x%x%x$") then
        local r = c:sub(2, 2)
        local g = c:sub(3, 3)
        local b = c:sub(4, 4)
        return tonumber(r .. r .. g .. g .. b .. b, 16) or fallback
    end

    -- 0xRRGGBB / 0XRRGGBB
    if c:match("^0[xX]%x%x%x%x%x%x$") then
        return tonumber(c, 16) or fallback
    end

    -- 纯十六进制（无前缀）"RRGGBB"
    if c:match("^%x%x%x%x%x%x$") then
        return tonumber(c, 16) or fallback
    end

    return fallback
end

--- 将颜色数字拆分为 r, g, b（每通道 0~255）
--- @param n number
--- @return number, number, number
function common.colorToRGB(n)
    n = n or 0
    local r = math.floor(n / 0x10000) % 0x100
    local g = math.floor(n / 0x100)   % 0x100
    local b = n % 0x100
    return r, g, b
end

--- 将 r, g, b（0~255）合并为颜色数字
--- @param r number
--- @param g number
--- @param b number
--- @return number
function common.rgbToNumber(r, g, b)
    return ((r or 0) % 0x100) * 0x10000
         + ((g or 0) % 0x100) * 0x100
         +  (b or 0) % 0x100
end

--- 把颜色数字格式化为 "#RRGGBB"
--- @param n number
--- @return string
function common.colorToHexString(n)
    return string.format("#%06x", (n or 0) % 0x1000000)
end

return common
