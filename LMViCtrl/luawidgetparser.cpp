#include "luawidgetparser.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

// ---------------------------------------------------------------------------
// 通用块/字段提取
// ---------------------------------------------------------------------------

QString LuaWidgetParser::extractBlock(const QString &text, int from)
{
    int   depth  = 0;
    int   start  = -1;
    bool  inStr  = false;
    QChar strCh;

    for (int i = from; i < text.length(); ++i) {
        QChar c = text[i];

        // 行注释 --
        if (!inStr && c == '-' && i + 1 < text.length() && text[i + 1] == '-') {
            while (i < text.length() && text[i] != '\n') ++i;
            continue;
        }

        if (inStr) {
            if (c == '\\') { ++i; continue; }
            if (c == strCh) inStr = false;
            continue;
        }
        if (c == '"' || c == '\'') { inStr = true; strCh = c; continue; }

        if (c == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start >= 0)
                return text.mid(start, i - start + 1);
        }
    }
    return {};
}

QString LuaWidgetParser::extractStringField(const QString &block, const QString &key)
{
    // 支持单/双引号
    QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(key)
                          + QStringLiteral(R"(\s*=\s*(?:"([^"]*)\"|'([^']*)'))"));
    const auto m = re.match(block);
    if (!m.hasMatch()) return {};
    return m.captured(1).isNull() ? m.captured(2) : m.captured(1);
}

bool LuaWidgetParser::extractBoolField(const QString &block, const QString &key, bool *ok)
{
    QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(key)
                          + QStringLiteral(R"(\s*=\s*(true|false)\b)"));
    const auto m = re.match(block);
    if (ok) *ok = m.hasMatch();
    return m.hasMatch() && m.captured(1) == QLatin1String("true");
}

double LuaWidgetParser::extractNumberField(const QString &block, const QString &key, bool *ok)
{
    QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(key)
                          + QStringLiteral(R"(\s*=\s*(-?[\d.]+))"));
    const auto m = re.match(block);
    if (ok) *ok = m.hasMatch();
    return m.hasMatch() ? m.captured(1).toDouble() : 0.0;
}

QString LuaWidgetParser::extractTableField(const QString &block, const QString &key)
{
    QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(key)
                          + QStringLiteral(R"(\s*=\s*\{)"));
    const auto m = re.match(block);
    if (!m.hasMatch()) return {};
    const int braceFrom = m.capturedStart() + m.capturedLength() - 1;
    return extractBlock(block, braceFrom);
}

// ---------------------------------------------------------------------------
// 选项 / 属性入口
// ---------------------------------------------------------------------------

PropertyOption LuaWidgetParser::parseOptionEntry(const QString &entry)
{
    PropertyOption opt;
    opt.value = extractStringField(entry, "value");
    opt.label = extractStringField(entry, "label");
    return opt;
}

PropertyMeta LuaWidgetParser::parsePropertyEntry(const QString &entry)
{
    PropertyMeta p;
    p.name        = extractStringField(entry, "name");
    p.type        = extractStringField(entry, "type");
    p.label       = extractStringField(entry, "label");
    p.description = extractStringField(entry, "description");
    p.event       = extractStringField(entry, "event");
    p.group       = extractStringField(entry, "group");
    p.unit        = extractStringField(entry, "unit");
    p.language    = extractStringField(entry, "language");
    p.snippet     = extractStringField(entry, "snippet");

    // default
    {
        QRegularExpression rStr(R"(\bdefault\s*=\s*"([^"]*)\")");
        auto m = rStr.match(entry);
        if (m.hasMatch()) {
            p.defaultValue = m.captured(1);
        } else {
            QRegularExpression rBool(R"(\bdefault\s*=\s*(true|false)\b)");
            m = rBool.match(entry);
            if (m.hasMatch()) {
                p.defaultValue = (m.captured(1) == QLatin1String("true"));
            } else {
                QRegularExpression rNum(R"(\bdefault\s*=\s*(-?[\d.]+))");
                m = rNum.match(entry);
                if (m.hasMatch())
                    p.defaultValue = m.captured(1).toDouble();
            }
        }
    }

    bool hasFlag = false;
    {
        bool ok = false;
        const double v = extractNumberField(entry, "min", &ok);
        if (ok) p.min = v;
        ok = false;
        const double v2 = extractNumberField(entry, "max", &ok);
        if (ok) p.max = v2;
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "multiline", &ok);
        if (ok) p.multiline = b;
    }
    {
        bool ok = false;
        const double n = extractNumberField(entry, "lines", &ok);
        if (ok) p.lines = int(n);
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "bindable", &ok);
        if (ok) p.bindable = b;
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "advanced", &ok);
        if (ok) p.advanced = b;
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "readonly", &ok);
        if (ok) p.readOnly = b;
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "hidden", &ok);
        if (ok) p.hidden = b;
    }
    {
        bool ok = false;
        const bool b = extractBoolField(entry, "required", &ok);
        if (ok) p.required = b;
    }
    Q_UNUSED(hasFlag);

    // options = { {value=..., label=...}, ... }
    const QString optsBlock = extractTableField(entry, "options");
    if (!optsBlock.isEmpty()) {
        const QString inner = optsBlock.mid(1, optsBlock.length() - 2);
        int pos = 0;
        while (pos < inner.length()) {
            const int b = inner.indexOf('{', pos);
            if (b < 0) break;
            const QString item = extractBlock(inner, b);
            if (item.isEmpty()) break;
            p.options.append(parseOptionEntry(item));
            pos = b + item.length();
        }
        // 兼容纯字符串列表 options = {"a","b"}
        if (p.options.isEmpty()) {
            QRegularExpression rq(R"("([^"]*)\")");
            auto it = rq.globalMatch(optsBlock);
            while (it.hasNext()) {
                PropertyOption o;
                o.value = it.next().captured(1);
                p.options.append(o);
            }
        }
    }

    return p;
}

// ---------------------------------------------------------------------------
// properties / event_properties 共用解析
// ---------------------------------------------------------------------------

QList<PropertyMeta> LuaWidgetParser::parsePropertyList(const QString &metaBlock,
                                                       const QString &key)
{
    QList<PropertyMeta> result;
    const QString block = extractTableField(metaBlock, key);
    if (block.isEmpty()) return result;

    const QString inner = block.mid(1, block.length() - 2);
    int pos = 0;
    while (pos < inner.length()) {
        const int b = inner.indexOf('{', pos);
        if (b < 0) break;
        const QString entry = extractBlock(inner, b);
        if (entry.isEmpty()) break;
        result.append(parsePropertyEntry(entry));
        pos = b + entry.length();
    }
    return result;
}

// ---------------------------------------------------------------------------
// events (兼容字符串数组与对象数组)
// ---------------------------------------------------------------------------

QList<EventParamDef> LuaWidgetParser::parseEventParams(const QString &eventEntry)
{
    QList<EventParamDef> params;
    const QString block = extractTableField(eventEntry, "params");
    if (block.isEmpty()) return params;

    const QString inner = block.mid(1, block.length() - 2);
    int pos = 0;
    while (pos < inner.length()) {
        const int b = inner.indexOf('{', pos);
        if (b < 0) break;
        const QString entry = extractBlock(inner, b);
        if (entry.isEmpty()) break;

        EventParamDef param;
        param.name = extractStringField(entry, "name");
        param.label = extractStringField(entry, "label");
        param.type = extractStringField(entry, "type");
        param.description = extractStringField(entry, "description");
        if (!param.name.isEmpty())
            params.append(param);
        pos = b + entry.length();
    }

    if (!params.isEmpty()) return params;

    QRegularExpression rq(R"("([^"]*)\")");
    auto it = rq.globalMatch(block);
    while (it.hasNext()) {
        EventParamDef param;
        param.name = it.next().captured(1);
        if (!param.name.isEmpty())
            params.append(param);
    }
    return params;
}

void LuaWidgetParser::parseEvents(const QString  &metaBlock,
                                  QStringList    *names,
                                  QList<EventDef> *defs)
{
    const QString block = extractTableField(metaBlock, "events");
    if (block.isEmpty()) return;

    // 优先尝试对象数组
    const QString inner = block.mid(1, block.length() - 2);
    int pos = 0;
    bool foundObj = false;
    while (pos < inner.length()) {
        const int b = inner.indexOf('{', pos);
        if (b < 0) break;
        const QString entry = extractBlock(inner, b);
        if (entry.isEmpty()) break;
        foundObj = true;

        EventDef d;
        d.name        = extractStringField(entry, "name");
        d.label       = extractStringField(entry, "label");
        d.description = extractStringField(entry, "description");
        d.params      = parseEventParams(entry);
        if (!d.name.isEmpty()) {
            if (names) names->append(d.name);
            if (defs)  defs->append(d);
        }
        pos = b + entry.length();
    }
    if (foundObj) return;

    // 退化：纯字符串数组 { "a", "b", "c" }
    QRegularExpression rq(R"("([^"]*)\")");
    auto it = rq.globalMatch(block);
    while (it.hasNext()) {
        const QString s = it.next().captured(1);
        if (names) names->append(s);
        if (defs) {
            EventDef d; d.name = s; defs->append(d);
        }
    }
}

// ---------------------------------------------------------------------------
// actions / bindings / draw_hints / api / size / tags
// ---------------------------------------------------------------------------

QList<ActionDef> LuaWidgetParser::parseActions(const QString &metaBlock)
{
    QList<ActionDef> out;
    const QString block = extractTableField(metaBlock, "actions");
    if (block.isEmpty()) return out;

    const QString inner = block.mid(1, block.length() - 2);
    int pos = 0;
    while (pos < inner.length()) {
        const int b = inner.indexOf('{', pos);
        if (b < 0) break;
        const QString entry = extractBlock(inner, b);
        if (entry.isEmpty()) break;

        ActionDef d;
        d.name        = extractStringField(entry, "name");
        d.label       = extractStringField(entry, "label");
        d.description = extractStringField(entry, "description");
        d.kind        = extractStringField(entry, "kind");
        d.property    = extractStringField(entry, "property");
        d.method      = extractStringField(entry, "method");
        d.valueType   = extractStringField(entry, "value_type");
        d.defaultValue = extractStringField(entry, "default_value");
        if (d.kind.isEmpty())
            d.kind = d.property.isEmpty() ? QStringLiteral("call_method") : QStringLiteral("set_property");
        if (d.method.isEmpty() && d.kind == QLatin1String("call_method"))
            d.method = d.name;
        if (d.valueType.isEmpty())
            d.valueType = QStringLiteral("any");
        if (!d.name.isEmpty()) out.append(d);
        pos = b + entry.length();
    }
    return out;
}

QList<BindingDef> LuaWidgetParser::parseBindings(const QString &metaBlock)
{
    QList<BindingDef> out;
    const QString block = extractTableField(metaBlock, "bindings");
    if (block.isEmpty()) return out;

    const QString inner = block.mid(1, block.length() - 2);
    int pos = 0;
    while (pos < inner.length()) {
        const int b = inner.indexOf('{', pos);
        if (b < 0) break;
        const QString entry = extractBlock(inner, b);
        if (entry.isEmpty()) break;
        BindingDef d;
        d.name      = extractStringField(entry, "name");
        d.target    = extractStringField(entry, "target");
        d.direction = extractStringField(entry, "direction");
        if (!d.name.isEmpty()) out.append(d);
        pos = b + entry.length();
    }
    return out;
}

QVariantMap LuaWidgetParser::parseDrawHints(const QString &metaBlock)
{
    QVariantMap out;
    const QString block = extractTableField(metaBlock, "draw_hints");
    if (block.isEmpty()) return out;

    // 解析所有 key = "value" 与 key = number / true|false
    QRegularExpression rStr(R"((\w+)\s*=\s*"([^"]*)\")");
    auto it = rStr.globalMatch(block);
    while (it.hasNext()) {
        auto m = it.next();
        out.insert(m.captured(1), m.captured(2));
    }
    QRegularExpression rNum(R"((\w+)\s*=\s*(-?[\d.]+)\b)");
    auto it2 = rNum.globalMatch(block);
    while (it2.hasNext()) {
        auto m = it2.next();
        if (!out.contains(m.captured(1)))
            out.insert(m.captured(1), m.captured(2).toDouble());
    }
    QRegularExpression rBool(R"((\w+)\s*=\s*(true|false)\b)");
    auto it3 = rBool.globalMatch(block);
    while (it3.hasNext()) {
        auto m = it3.next();
        if (!out.contains(m.captured(1)))
            out.insert(m.captured(1), m.captured(2) == QLatin1String("true"));
    }
    return out;
}

WidgetApi LuaWidgetParser::parseApi(const QString &metaBlock)
{
    WidgetApi a;
    const QString block = extractTableField(metaBlock, "api");
    if (block.isEmpty()) return a;

    auto get = [&block](const QString &k) {
        bool ok = false;
        const bool v = extractBoolField(block, k, &ok);
        return ok && v;
    };
    a.hasDraw            = get("has_draw");
    a.hasOn              = get("has_on");
    a.hasGetProperty     = get("has_get_property");
    a.hasSetProperty     = get("has_set_property");
    a.hasGetProperties   = get("has_get_properties");
    a.hasApplyProperties = get("has_apply_properties");
    a.hasToState         = get("has_to_state");
    a.hasDestroy         = get("has_destroy");
    return a;
}

QSize LuaWidgetParser::parseSize(const QString &block, const QSize &def)
{
    if (block.isEmpty()) return def;
    bool okW = false, okH = false;
    const double w = extractNumberField(block, "w", &okW);
    const double h = extractNumberField(block, "h", &okH);
    return QSize(okW ? int(w) : def.width(), okH ? int(h) : def.height());
}

QStringList LuaWidgetParser::parseStringArray(const QString &block)
{
    QStringList out;
    if (block.isEmpty()) return out;
    QRegularExpression rq(R"("([^"]*)\")");
    auto it = rq.globalMatch(block);
    while (it.hasNext()) out << it.next().captured(1);
    return out;
}

// ---------------------------------------------------------------------------
// 公共接口
// ---------------------------------------------------------------------------

WidgetMeta LuaWidgetParser::parse(const QString &filePath)
{
    WidgetMeta meta;
    meta.luaFilePath = filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return meta;
    const QString content = QTextStream(&file).readAll();
    file.close();

    QRegularExpression metaDecl(R"(__widget_meta\s*=\s*\{)");
    const auto match = metaDecl.match(content);
    if (!match.hasMatch()) return meta;

    const int    braceFrom = match.capturedStart() + match.capturedLength() - 1;
    const QString metaBlock = extractBlock(content, braceFrom);
    if (metaBlock.isEmpty()) return meta;

    // 标识
    meta.id            = extractStringField(metaBlock, "id");
    meta.name          = extractStringField(metaBlock, "name");
    meta.description   = extractStringField(metaBlock, "description");
    meta.category      = extractStringField(metaBlock, "category");
    meta.icon          = extractStringField(metaBlock, "icon");
    meta.previewImage  = extractStringField(metaBlock, "preview_image");
    meta.author        = extractStringField(metaBlock, "author");
    meta.schemaVersion = extractStringField(metaBlock, "schema_version");
    meta.version       = extractStringField(metaBlock, "version");

    // 渲染策略
    meta.type       = extractStringField(metaBlock, "type");
    meta.renderMode = extractStringField(metaBlock, "render_mode");
    if (meta.renderMode.isEmpty())
        meta.renderMode = QStringLiteral("builtin");

    // tags
    meta.tags = parseStringArray(extractTableField(metaBlock, "tags"));

    // 尺寸
    meta.defaultSize = parseSize(extractTableField(metaBlock, "default_size"), meta.defaultSize);
    meta.minSize     = parseSize(extractTableField(metaBlock, "min_size"),     meta.minSize);
    meta.maxSize     = parseSize(extractTableField(metaBlock, "max_size"),     meta.maxSize);
    {
        bool ok = false;
        bool b = extractBoolField(metaBlock, "resizable", &ok);
        if (ok) meta.resizable = b;
        ok = false;
        b = extractBoolField(metaBlock, "rotatable", &ok);
        if (ok) meta.rotatable = b;
        ok = false;
        b = extractBoolField(metaBlock, "has_data_binding", &ok);
        if (ok) meta.hasDataBinding = b;
    }

    // 属性 / 事件 / 绑定 / 提示 / API
    meta.properties      = parsePropertyList(metaBlock, "properties");
    meta.actions         = parseActions(metaBlock);
    meta.eventProperties = parsePropertyList(metaBlock, "event_properties");
    parseEvents(metaBlock, &meta.events, &meta.eventDefs);
    meta.bindings        = parseBindings(metaBlock);
    meta.drawHints       = parseDrawHints(metaBlock);
    meta.api             = parseApi(metaBlock);

    // bindings 非空时自动标记
    if (!meta.bindings.isEmpty())
        meta.hasDataBinding = true;

    return meta;
}

QList<WidgetMeta> LuaWidgetParser::parseDirectory(const QString &dirPath)
{
    QList<WidgetMeta> result;
    QDir rootDir(dirPath);
    if (!rootDir.exists()) return result;

    // 1) 顶层 .lua 文件（按文件名排序）
    const QStringList topFiles = rootDir.entryList(
        {QStringLiteral("*.lua")}, QDir::Files, QDir::Name);
    for (const QString &fileName : topFiles) {
        WidgetMeta meta = parse(rootDir.absoluteFilePath(fileName));
        if (!meta.id.isEmpty())
            result.append(std::move(meta));
    }

    // 2) 递归扫描子目录（用于自定义控件分组等）
    //    - 子目录名作为默认 category（脚本未显式声明 category 时使用）
    //    - 内置目录名 -> 中文显示名 的映射
    static const QHash<QString, QString> kFolderCategoryMap = {
        { QStringLiteral("custom"), QStringLiteral("自定义控件") },
    };

    const QStringList subDirs = rootDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &subName : subDirs) {
        const QString subPath = rootDir.absoluteFilePath(subName);
        const QString fallbackCategory =
            kFolderCategoryMap.value(subName.toLower(), subName);

        QDirIterator it(subPath,
                        {QStringLiteral("*.lua")},
                        QDir::Files,
                        QDirIterator::Subdirectories);
        QStringList files;
        while (it.hasNext()) files << it.next();
        std::sort(files.begin(), files.end());

        for (const QString &filePath : std::as_const(files)) {
            WidgetMeta meta = parse(filePath);
            if (meta.id.isEmpty()) continue;
            if (meta.category.isEmpty())
                meta.category = fallbackCategory;
            result.append(std::move(meta));
        }
    }

    return result;
}
