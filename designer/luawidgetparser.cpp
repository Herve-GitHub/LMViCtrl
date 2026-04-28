#include "luawidgetparser.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

// ---------------------------------------------------------------------------
// 内部工具函数
// ---------------------------------------------------------------------------

// 从 from 位置开始，找到首个 { 并提取到匹配的 }（跳过字符串与行注释）
QString LuaWidgetParser::extractBlock(const QString &text, int from)
{
    int   depth  = 0;
    int   start  = -1;
    bool  inStr  = false;
    QChar strCh;

    for (int i = from; i < text.length(); ++i) {
        QChar c = text[i];

        // 跳过行注释 --
        if (!inStr && c == '-' && i + 1 < text.length() && text[i + 1] == '-') {
            while (i < text.length() && text[i] != '\n') ++i;
            continue;
        }

        if (inStr) {
            if (c == '\\') { ++i; continue; }   // 转义字符
            if (c == strCh) inStr = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            inStr = true;
            strCh = c;
            continue;
        }
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

// 提取  key = "value"  的值（首次匹配，使用 \b 防止子串误匹配）
QString LuaWidgetParser::extractStringField(const QString &block, const QString &key)
{
    QRegularExpression re(QStringLiteral("\\b") + QRegularExpression::escape(key)
                          + QStringLiteral(R"(\s*=\s*"([^"]*)\")"));
    const auto m = re.match(block);
    return m.hasMatch() ? m.captured(1) : QString{};
}

// ---------------------------------------------------------------------------
// 属性解析
// ---------------------------------------------------------------------------

PropertyMeta LuaWidgetParser::parsePropertyEntry(const QString &entry)
{
    PropertyMeta p;
    p.name        = extractStringField(entry, "name");
    p.type        = extractStringField(entry, "type");
    p.label       = extractStringField(entry, "label");
    p.description = extractStringField(entry, "description");
    p.event       = extractStringField(entry, "event");

    // default 值：优先字符串，其次布尔，再次数值
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

    // min / max
    {
        QRegularExpression rMin(R"(\bmin\s*=\s*(-?[\d.]+))");
        auto m = rMin.match(entry);
        if (m.hasMatch()) p.min = m.captured(1).toDouble();
    }
    {
        QRegularExpression rMax(R"(\bmax\s*=\s*(-?[\d.]+))");
        auto m = rMax.match(entry);
        if (m.hasMatch()) p.max = m.captured(1).toDouble();
    }

    // multiline / lines
    {
        QRegularExpression rMl(R"(\bmultiline\s*=\s*(true|false)\b)");
        auto m = rMl.match(entry);
        if (m.hasMatch()) p.multiline = (m.captured(1) == QLatin1String("true"));
    }
    {
        QRegularExpression rLn(R"(\blines\s*=\s*(\d+))");
        auto m = rLn.match(entry);
        if (m.hasMatch()) p.lines = m.captured(1).toInt();
    }

    return p;
}

QList<PropertyMeta> LuaWidgetParser::parseProperties(const QString &metaBlock)
{
    QList<PropertyMeta> result;

    QRegularExpression re(R"(\bproperties\s*=\s*\{)");
    const auto m = re.match(metaBlock);
    if (!m.hasMatch()) return result;

    // 提取 properties = { ... } 整块
    const int    braceFrom  = m.capturedStart() + m.capturedLength() - 1;
    const QString propsBlock = extractBlock(metaBlock, braceFrom);
    if (propsBlock.isEmpty()) return result;

    // 去掉最外层 { }，逐个提取 { ... } 属性入口
    const QString inner = propsBlock.mid(1, propsBlock.length() - 2);
    int pos = 0;
    while (pos < inner.length()) {
        const int braceIdx = inner.indexOf('{', pos);
        if (braceIdx < 0) break;
        const QString entry = extractBlock(inner, braceIdx);
        if (entry.isEmpty()) break;
        result.append(parsePropertyEntry(entry));
        pos = braceIdx + entry.length();
    }
    return result;
}

// ---------------------------------------------------------------------------
// 事件列表解析
// ---------------------------------------------------------------------------

QStringList LuaWidgetParser::parseEvents(const QString &metaBlock)
{
    QStringList events;

    QRegularExpression re(R"(\bevents\s*=\s*\{)");
    const auto m = re.match(metaBlock);
    if (!m.hasMatch()) return events;

    const int    braceFrom = m.capturedStart() + m.capturedLength() - 1;
    const QString evBlock  = extractBlock(metaBlock, braceFrom);
    if (evBlock.isEmpty()) return events;

    QRegularExpression quoted(R"("([^"]*)\")");
    auto it = quoted.globalMatch(evBlock);
    while (it.hasNext())
        events << it.next().captured(1);

    return events;
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

    // 定位 __widget_meta = {
    QRegularExpression metaDecl(R"(__widget_meta\s*=\s*\{)");
    const auto match = metaDecl.match(content);
    if (!match.hasMatch()) return meta;

    // 提取完整表块（从 { 开始）
    const int    braceFrom = match.capturedStart() + match.capturedLength() - 1;
    const QString metaBlock = extractBlock(content, braceFrom);
    if (metaBlock.isEmpty()) return meta;

    meta.id            = extractStringField(metaBlock, "id");
    meta.name          = extractStringField(metaBlock, "name");
    meta.description   = extractStringField(metaBlock, "description");
    meta.schemaVersion = extractStringField(metaBlock, "schema_version");
    meta.version       = extractStringField(metaBlock, "version");
    meta.properties    = parseProperties(metaBlock);
    meta.events        = parseEvents(metaBlock);

    QRegularExpression hdbRe(R"(\bhas_data_binding\s*=\s*(true|false)\b)");
    const auto hm = hdbRe.match(metaBlock);
    if (hm.hasMatch())
        meta.hasDataBinding = (hm.captured(1) == QLatin1String("true"));

    return meta;
}

QList<WidgetMeta> LuaWidgetParser::parseDirectory(const QString &dirPath)
{
    QList<WidgetMeta> result;
    QDir dir(dirPath);
    if (!dir.exists()) return result;

    const QStringList files = dir.entryList({QStringLiteral("*.lua")}, QDir::Files, QDir::Name);
    for (const QString &fileName : files) {
        WidgetMeta meta = parse(dir.absoluteFilePath(fileName));
        if (!meta.id.isEmpty())
            result.append(std::move(meta));
    }
    return result;
}
