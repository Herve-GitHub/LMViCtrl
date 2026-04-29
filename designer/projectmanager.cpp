#include "projectmanager.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>

// ===========================================================================
// JSON 序列化
// ===========================================================================
QJsonObject ProjectManager::instanceToJson(const WidgetInstance &inst)
{
    QJsonObject o;
    o["instanceId"] = inst.instanceId;
    o["widgetId"]   = inst.widgetId;
    o["name"]       = inst.name;
    o["zOrder"]     = inst.zOrder;
    o["x"]          = inst.x;
    o["y"]          = inst.y;
    o["width"]      = inst.width;
    o["height"]     = inst.height;
    o["locked"]     = inst.locked;
    o["visible"]    = inst.visible;
    o["properties"] = QJsonObject::fromVariantMap(inst.properties);
    return o;
}

WidgetInstance ProjectManager::instanceFromJson(const QJsonObject &o)
{
    WidgetInstance inst;
    inst.instanceId = o.value("instanceId").toString();
    inst.widgetId   = o.value("widgetId").toString();
    inst.name       = o.value("name").toString();
    inst.zOrder     = o.value("zOrder").toInt(0);
    inst.x          = o.value("x").toInt(0);
    inst.y          = o.value("y").toInt(0);
    inst.width      = o.value("width").toInt(100);
    inst.height     = o.value("height").toInt(100);
    inst.locked     = o.value("locked").toBool(false);
    inst.visible    = o.value("visible").toBool(true);
    inst.properties = o.value("properties").toObject().toVariantMap();
    return inst;
}

QJsonObject ProjectManager::screenToJson(const ScreenData &s)
{
    QJsonObject o;
    o["id"]      = s.id;
    o["name"]    = s.name;
    o["order"]   = s.order;
    o["bgColor"] = s.bgColor;
    QJsonArray arr;
    for (const auto &w : s.widgets) arr.append(instanceToJson(w));
    o["widgets"] = arr;
    return o;
}

ScreenData ProjectManager::screenFromJson(const QJsonObject &o)
{
    ScreenData s;
    s.id      = o.value("id").toString();
    s.name    = o.value("name").toString();
    s.order   = o.value("order").toInt(0);
    s.bgColor = o.value("bgColor").toString("#000000");
    const QJsonArray arr = o.value("widgets").toArray();
    for (const auto &v : arr) s.widgets.append(instanceFromJson(v.toObject()));
    return s;
}

QJsonObject ProjectManager::toJson(const ProjectData &p)
{
    QJsonObject root;
    root["id"]            = p.id;
    root["schemaVersion"] = p.schemaVersion;
    root["name"]          = p.name;
    root["description"]   = p.description;
    root["author"]        = p.author;
    root["createdAt"]     = p.createdAt;
    root["updatedAt"]     = p.updatedAt;

    QJsonObject target;
    target["width"]      = p.target.width;
    target["height"]     = p.target.height;
    target["colorDepth"] = p.target.colorDepth;
    target["platform"]   = p.target.platform;
    root["target"]       = target;

    QJsonObject res;
    res["imagePath"] = p.resources.imagePath;
    res["linuxPath"] = p.resources.linuxPath;
    root["resources"] = res;

    QJsonArray screens;
    for (const auto &s : p.screens) screens.append(screenToJson(s));
    root["screens"] = screens;
    return root;
}

ProjectData ProjectManager::fromJson(const QJsonObject &o)
{
    ProjectData p;
    p.id            = o.value("id").toString();
    p.schemaVersion = o.value("schemaVersion").toString("1.0");
    p.name          = o.value("name").toString("NewProject");
    p.description   = o.value("description").toString();
    p.author        = o.value("author").toString();
    p.createdAt     = o.value("createdAt").toString();
    p.updatedAt     = o.value("updatedAt").toString();

    const QJsonObject t = o.value("target").toObject();
    p.target.width      = t.value("width").toInt(1024);
    p.target.height     = t.value("height").toInt(768);
    p.target.colorDepth = t.value("colorDepth").toInt(16);
    p.target.platform   = t.value("platform").toString("linux");

    const QJsonObject r = o.value("resources").toObject();
    p.resources.imagePath = r.value("imagePath").toString("./image/");
    p.resources.linuxPath = r.value("linuxPath").toString("/root/image/");

    const QJsonArray arr = o.value("screens").toArray();
    for (const auto &v : arr) p.screens.append(screenFromJson(v.toObject()));
    return p;
}

// ===========================================================================
// 文件读写
// ===========================================================================
bool ProjectManager::saveToFile(const ProjectData &project, const QString &path,
                                QString *errorMessage)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    const QJsonDocument doc(toJson(project));
    f.write(doc.toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    return true;
}

bool ProjectManager::loadFromFile(ProjectData *project, const QString &path,
                                  QString *errorMessage)
{
    if (!project) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage) *errorMessage = err.errorString();
        return false;
    }
    *project = fromJson(doc.object());
    return true;
}

// ===========================================================================
// 编译为 Lua
// ===========================================================================
static QString luaQuote(const QString &s)
{
    QString r = s;
    r.replace('\\', "\\\\");
    r.replace('"',  "\\\"");
    r.replace('\n', "\\n");
    r.replace('\r', "\\r");
    return '"' + r + '"';
}

static QString variantToLua(const QVariant &v)
{
    switch (v.typeId()) {
    case QMetaType::Bool:    return v.toBool() ? "true" : "false";
    case QMetaType::Int:
    case QMetaType::LongLong:
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return QString::number(v.toLongLong());
    case QMetaType::Double:
    case QMetaType::Float:
        return QString::number(v.toDouble(), 'g', 12);
    default:
        if (v.canConvert<QString>()) return luaQuote(v.toString());
        return "nil";
    }
}

QString ProjectManager::compileToLua(const ProjectData &p)
{
    QString out;
    QTextStream s(&out);
    s.setEncoding(QStringConverter::Utf8);

    s << "-- Auto-generated by QtLvglDesigner\n";
    s << "-- Project: " << p.name << "\n";
    s << "-- Generated: "
      << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";

    s << "local project = {}\n";
    s << "project.name        = " << luaQuote(p.name)        << "\n";
    s << "project.description = " << luaQuote(p.description) << "\n";
    s << "project.author      = " << luaQuote(p.author)      << "\n";
    s << "project.target = { "
      << "width = "      << p.target.width  << ", "
      << "height = "     << p.target.height << ", "
      << "colorDepth = " << p.target.colorDepth << ", "
      << "platform = "   << luaQuote(p.target.platform)
      << " }\n";
    s << "project.resources = { "
      << "imagePath = " << luaQuote(p.resources.imagePath) << ", "
      << "linuxPath = " << luaQuote(p.resources.linuxPath)
      << " }\n\n";

    s << "project.screens = {\n";
    for (const ScreenData &scr : p.screens) {
        s << "  {\n";
        s << "    id      = " << luaQuote(scr.id)      << ",\n";
        s << "    name    = " << luaQuote(scr.name)    << ",\n";
        s << "    order   = " << scr.order             << ",\n";
        s << "    bgColor = " << luaQuote(scr.bgColor) << ",\n";
        s << "    widgets = {\n";
        for (const WidgetInstance &w : scr.widgets) {
            s << "      {\n";
            s << "        instanceId = " << luaQuote(w.instanceId) << ",\n";
            s << "        widgetId   = " << luaQuote(w.widgetId)   << ",\n";
            s << "        zOrder     = " << w.zOrder               << ",\n";
            s << "        x          = " << w.x                    << ",\n";
            s << "        y          = " << w.y                    << ",\n";
            s << "        width      = " << w.width                << ",\n";
            s << "        height     = " << w.height               << ",\n";
            s << "        locked     = " << (w.locked  ? "true" : "false") << ",\n";
            s << "        visible    = " << (w.visible ? "true" : "false") << ",\n";
            s << "        properties = {";
            bool first = true;
            for (auto it = w.properties.constBegin(); it != w.properties.constEnd(); ++it) {
                if (!first) s << ",";
                s << " [" << luaQuote(it.key()) << "] = " << variantToLua(it.value());
                first = false;
            }
            s << " },\n";
            s << "      },\n";
        }
        s << "    },\n";
        s << "  },\n";
    }
    s << "}\n\n";
    s << "return project\n";
    return out;
}

bool ProjectManager::compileFileToLua(const QString &jsonPath,
                                      const QString &luaPath,
                                      QString *errorMessage)
{
    ProjectData p;
    if (!loadFromFile(&p, jsonPath, errorMessage)) return false;

    QSaveFile f(luaPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    f.write(compileToLua(p).toUtf8());
    if (!f.commit()) {
        if (errorMessage) *errorMessage = f.errorString();
        return false;
    }
    return true;
}
