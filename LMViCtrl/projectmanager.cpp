#include "projectmanager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTextStream>

// ===========================================================================
// JSON 序列化
// ===========================================================================
namespace {

QJsonObject eventActionToJson(const EventAction &action)
{
    QJsonObject o;
    o["id"] = action.id;
    o["type"] = action.type;
    o["label"] = action.label;
    o["targetId"] = action.targetId;
    o["targetName"] = action.targetName;
    o["method"] = action.method;
    o["params"] = QJsonObject::fromVariantMap(action.params);
    o["code"] = action.code;
    o["enabled"] = action.enabled;
    return o;
}

EventAction eventActionFromJson(const QJsonObject &o)
{
    EventAction action;
    action.id = o.value("id").toString();
    action.type = o.value("type").toString();
    action.label = o.value("label").toString();
    action.targetId = o.value("targetId").toString();
    action.targetName = o.value("targetName").toString();
    action.method = o.value("method").toString();
    action.params = o.value("params").toObject().toVariantMap();
    action.code = o.value("code").toString();
    action.enabled = o.value("enabled").toBool(true);
    return action;
}

QJsonObject eventBindingsToJson(const QList<WidgetEventBinding> &bindings)
{
    QJsonObject events;
    for (const WidgetEventBinding &binding : bindings) {
        if (binding.eventName.isEmpty()) continue;
        QJsonArray actions;
        for (const EventAction &action : binding.actions)
            actions.append(eventActionToJson(action));
        if (!actions.isEmpty())
            events.insert(binding.eventName, actions);
    }
    return events;
}

QList<WidgetEventBinding> eventBindingsFromJson(const QJsonObject &events)
{
    QList<WidgetEventBinding> bindings;
    for (auto it = events.constBegin(); it != events.constEnd(); ++it) {
        WidgetEventBinding binding;
        binding.eventName = it.key();
        const QJsonArray actions = it.value().toArray();
        for (const QJsonValue &value : actions) {
            if (value.isObject())
                binding.actions.append(eventActionFromJson(value.toObject()));
        }
        if (!binding.eventName.isEmpty() && !binding.actions.isEmpty())
            bindings.append(binding);
    }
    return bindings;
}

} // namespace

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
    o["events"]     = eventBindingsToJson(inst.eventBindings);
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
    inst.eventBindings = eventBindingsFromJson(o.value("events").toObject());
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

    QJsonObject font;
    font["file"] = p.font.file;
    font["size"] = p.font.size;
    root["font"] = font;

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

    const QJsonObject fo = o.value("font").toObject();
    p.font.file = fo.value("file").toString();
    p.font.size = fo.value("size").toInt(16);

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

static QString variantMapToLua(const QVariantMap &map)
{
    QString out = QStringLiteral("{");
    bool first = true;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (!first) out += QStringLiteral(",");
        out += QStringLiteral(" [") + luaQuote(it.key()) + QStringLiteral("] = ")
             + variantToLua(it.value());
        first = false;
    }
    out += QStringLiteral(" }");
    return out;
}

static QString eventBindingsToLua(const QList<WidgetEventBinding> &bindings, int indent)
{
    if (bindings.isEmpty()) return QStringLiteral("{}");
    const QString pad(indent, QLatin1Char(' '));
    const QString childPad(indent + 2, QLatin1Char(' '));
    const QString actionPad(indent + 4, QLatin1Char(' '));
    QString out = QStringLiteral("{\n");
    QTextStream s(&out);
    for (const WidgetEventBinding &binding : bindings) {
        if (binding.eventName.isEmpty() || binding.actions.isEmpty()) continue;
        s << childPad << "[" << luaQuote(binding.eventName) << "] = {\n";
        for (const EventAction &action : binding.actions) {
            s << actionPad << "{\n";
            s << actionPad << "  id = " << luaQuote(action.id) << ",\n";
            s << actionPad << "  type = " << luaQuote(action.type) << ",\n";
            s << actionPad << "  label = " << luaQuote(action.label) << ",\n";
            s << actionPad << "  targetId = " << luaQuote(action.targetId) << ",\n";
            s << actionPad << "  targetName = " << luaQuote(action.targetName) << ",\n";
            s << actionPad << "  method = " << luaQuote(action.method) << ",\n";
            s << actionPad << "  params = " << variantMapToLua(action.params) << ",\n";
            s << actionPad << "  code = " << luaQuote(action.code) << ",\n";
            s << actionPad << "  enabled = " << (action.enabled ? "true" : "false") << ",\n";
            s << actionPad << "},\n";
        }
        s << childPad << "},\n";
    }
    s << pad << "}";
    return out;
}

QString ProjectManager::compileToLua(const ProjectData &p)
{
    QString out;
    QTextStream s(&out);
    s.setEncoding(QStringConverter::Utf8);

    s << "-- Auto-generated by LMViCtrl\n";
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
      << " }\n";
    s << "project.font = { "
      << "file = " << luaQuote(p.font.file) << ", "
      << "size = " << p.font.size
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
            s << "        name       = " << luaQuote(w.name)       << ",\n";
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
            s << "        events     = " << eventBindingsToLua(w.eventBindings, 8) << ",\n";
            s << "      },\n";
        }
        s << "    },\n";
        s << "  },\n";
    }
    s << "}\n\n";
    // 页面跳转动作模块：按钮等控件的事件代码可直接调用 PageNavigation.*
    s << "local ok_nav, page_navigation = pcall(require, \"common.page_navigation\")\n";
    s << "if ok_nav and page_navigation then\n";
    s << "    PageNavigation = page_navigation\n";
    s << "    _G.PageNavigation = page_navigation\n";
    s << "    project.page_navigation = page_navigation\n";
    s << "else\n";
    s << "    print(\"[project] warning: failed to load common.page_navigation: \" .. tostring(page_navigation))\n";
    s << "end\n\n";

    // 引导仿真：调用部署在工程目录中的 runtime.lua，根据 project 表实例化 LVGL 界面
    s << "local ok, runtime = pcall(require, \"runtime\")\n";
    s << "if ok and runtime and type(runtime.run) == \"function\" then\n";
    s << "    runtime.run(project)\n";
    s << "end\n\n";
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

// ===========================================================================
// 工程目录布局 / 运行时部署
// ===========================================================================
QString ProjectManager::projectJsonFileName(const QString &projectName)
{
    return projectName + QStringLiteral(".qlvgl.json");
}

QString ProjectManager::projectLuaFileName(const QString &projectName)
{
    return projectName + QStringLiteral(".lua");
}

namespace {

// 递归拷贝目录内容（含子目录）到 dstDir。
// overwrite=true 时覆盖同名文件；false 时跳过已存在文件。
bool copyDirRecursive(const QString &srcDir, const QString &dstDir,
                      bool overwrite, QString *errorMessage)
{
    QDir src(srcDir);
    if (!src.exists()) {
        if (errorMessage)
            *errorMessage = QObject::tr("源目录不存在：%1").arg(srcDir);
        return false;
    }
    QDir().mkpath(dstDir);

    const QFileInfoList entries = src.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString dstPath = dstDir + QLatin1Char('/') + fi.fileName();
        if (fi.isDir()) {
            if (!copyDirRecursive(fi.absoluteFilePath(), dstPath,
                                  overwrite, errorMessage))
                return false;
        } else {
            if (QFile::exists(dstPath)) {
                if (!overwrite) continue;
                QFile::remove(dstPath);
            }
            if (!QFile::copy(fi.absoluteFilePath(), dstPath)) {
                if (errorMessage)
                    *errorMessage = QObject::tr("拷贝失败：%1 -> %2")
                                        .arg(fi.absoluteFilePath(), dstPath);
                return false;
            }
        }
    }
    return true;
}

void collectWidgetLuaFiles(const QString &dirPath,
                           const QString &prefix,
                           QStringList *names)
{
    QDir dir(dirPath);

    const QFileInfoList files = dir.entryInfoList(
        QStringList{QStringLiteral("*.lua")},
        QDir::Files,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &fi : files) {
        if (fi.fileName().compare(QStringLiteral("manifest.lua"), Qt::CaseInsensitive) == 0)
            continue;
        const QString baseName = fi.completeBaseName();
        names->append(prefix.isEmpty()
                          ? baseName
                          : prefix + QLatin1Char('/') + baseName);
    }

    const QFileInfoList dirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo &subdir : dirs) {
        const QString childPrefix = prefix.isEmpty()
                                      ? subdir.fileName()
                                      : prefix + QLatin1Char('/') + subdir.fileName();
        collectWidgetLuaFiles(subdir.absoluteFilePath(), childPrefix, names);
    }
}

bool writeWidgetManifest(const QString &projectDir, QString *errorMessage)
{
    const QString widgetsDir = projectDir + QStringLiteral("/widgets");
    if (!QDir(widgetsDir).exists()) {
        if (errorMessage)
            *errorMessage = QObject::tr("widgets 目录不存在：%1").arg(widgetsDir);
        return false;
    }

    QStringList names;
    collectWidgetLuaFiles(widgetsDir, QString(), &names);

    QSaveFile file(projectDir + QStringLiteral("/manifest.lua"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QObject::tr("创建 manifest.lua 失败：%1").arg(file.errorString());
        return false;
    }

    QTextStream s(&file);
    s.setEncoding(QStringConverter::Utf8);
    s << "return {\n";
    for (const QString &name : names)
        s << "    " << luaQuote(name) << ",\n";
    s << "}\n";

    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = QObject::tr("写入 manifest.lua 失败：%1").arg(file.errorString());
        return false;
    }
    return true;
}

// 在多个候选位置中找到 LMViCtrl 自带的 lua 资源根目录
QString locateRuntimeLuaRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/lua"),
        appDir + QStringLiteral("/../lua"),
        appDir + QStringLiteral("/../../lua"),
        appDir + QStringLiteral("/../../LMViCtrl/lua"),
        appDir + QStringLiteral("/../../../LMViCtrl/lua"),
    };
    for (const QString &c : candidates) {
        const QFileInfo fi(c);
        if (fi.exists() && fi.isDir())
            return fi.absoluteFilePath();
    }
    return QString();
}

}  // namespace

bool ProjectManager::deployRuntime(const QString &projectDir,
                                   bool overwriteWidgets,
                                   QString *errorMessage)
{
    if (projectDir.isEmpty() || !QDir(projectDir).exists()) {
        if (errorMessage)
            *errorMessage = QObject::tr("工程目录无效：%1").arg(projectDir);
        return false;
    }

    const QString srcRoot = locateRuntimeLuaRoot();
    if (srcRoot.isEmpty()) {
        if (errorMessage)
            *errorMessage = QObject::tr(
                "未找到 LMViCtrl 的 lua 运行时目录（应位于可执行文件旁的 lua/）。");
        return false;
    }

    // 1. 顶层 .lua 文件（如 runtime.lua）始终覆盖，保持与 LMViCtrl 同步
    QDir srcDir(srcRoot);
    const QFileInfoList topFiles = srcDir.entryInfoList(
        QStringList{QStringLiteral("*.lua")}, QDir::Files);
    for (const QFileInfo &fi : topFiles) {
        const QString dst = projectDir + QLatin1Char('/') + fi.fileName();
        if (QFile::exists(dst)) QFile::remove(dst);
        if (!QFile::copy(fi.absoluteFilePath(), dst)) {
            if (errorMessage)
                *errorMessage = QObject::tr("拷贝运行时失败：%1").arg(fi.fileName());
            return false;
        }
    }

    // 2. common/widgets 子目录：默认不覆盖既有文件，避免覆盖用户的本地修改
    const QStringList subdirs = { QStringLiteral("common"), QStringLiteral("widgets") };
    for (const QString &sub : subdirs) {
        const QString src = srcRoot     + QLatin1Char('/') + sub;
        const QString dst = projectDir  + QLatin1Char('/') + sub;
        if (!QDir(src).exists()) continue;
        if (!copyDirRecursive(src, dst, overwriteWidgets, errorMessage))
            return false;
    }

    // 3. 顶层 manifest.lua：由工程目录 widgets/ 下的实际 lua 文件生成
    if (!writeWidgetManifest(projectDir, errorMessage))
        return false;

    return true;
}
