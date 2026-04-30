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
    // 提取从 from 起的第一个 { ... }（处理嵌套与字符串、行注释）
    static QString             extractBlock(const QString &text, int from = 0);
    // 提取  key = "value"  字符串字段
    static QString             extractStringField(const QString &block, const QString &key);
    // 提取  key = true|false  布尔字段
    static bool                extractBoolField(const QString &block, const QString &key, bool *ok = nullptr);
    // 提取  key = number  数字字段
    static double              extractNumberField(const QString &block, const QString &key, bool *ok = nullptr);
    // 提取  key = { ... }  子表块（含外层大括号）
    static QString             extractTableField(const QString &block, const QString &key);

    // 解析一条属性入口 { name=..., type=..., ... }
    static PropertyMeta        parsePropertyEntry(const QString &entry);
    // 解析 { name="x", label="X" } 选项入口
    static PropertyOption      parseOptionEntry(const QString &entry);

    // 把 properties = { ... } 拆成单条记录列表
    static QList<PropertyMeta> parsePropertyList(const QString &metaBlock, const QString &key);
    // events 既支持 {"a","b",...} 也支持 { {name="a",...},... }
    static void                parseEvents(const QString &metaBlock,
                                           QStringList    *names,
                                           QList<EventDef> *defs);
    // bindings 列表
    static QList<BindingDef>   parseBindings(const QString &metaBlock);
    // draw_hints 表（kv 字符串）
    static DrawHints           parseDrawHints(const QString &metaBlock);
    // api 表
    static WidgetApi           parseApi(const QString &metaBlock);
    // size 类子表 { w=..., h=... } -> QSize
    static QSize               parseSize(const QString &block, const QSize &def);
    // tags = { "a", "b" }
    static QStringList         parseStringArray(const QString &block);
};
