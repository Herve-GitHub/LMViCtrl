#pragma once
#include "WidgetMeta.h"

// 从 Lua 文件中解析 __widget_meta 表，转换为 WidgetMeta
class LuaWidgetParser
{
public:
    // 解析单个 Lua 文件
    static WidgetMeta        parse(const QString &filePath);
    // 扫描目录下所有 .lua 文件并解析
    static QList<WidgetMeta> parseDirectory(const QString &dirPath);

private:
    // 从 from 位置开始提取第一个 { ... } 块（处理嵌套与字符串）
    static QString             extractBlock(const QString &text, int from = 0);
    // 提取   key = "value"   的值（首次匹配）
    static QString             extractStringField(const QString &block, const QString &key);
    // 解析 properties = { ... } 子表
    static QList<PropertyMeta> parseProperties(const QString &metaBlock);
    // 解析 events = { ... } 字符串列表
    static QStringList         parseEvents(const QString &metaBlock);
    // 解析单条属性入口 { name=..., type=..., ... }
    static PropertyMeta        parsePropertyEntry(const QString &entry);
};
