#pragma once
#include "WidgetMeta.h"

#include <QJsonObject>
#include <QString>

/**
 * @brief 工程文件读写与编译工具
 *
 * 工程数据 (ProjectData) 以 JSON 文件形式持久化保存，
 * 后续可基于 JSON 编译成可在运行时（LvglLuaBinding）加载的 Lua 脚本。
 */
class ProjectManager
{
public:
    // ---- JSON 序列化 ----
    static QJsonObject toJson  (const ProjectData &project);
    static ProjectData fromJson(const QJsonObject &obj);

    // ---- 文件读写 ----
    static bool saveToFile  (const ProjectData &project, const QString &path,
                             QString *errorMessage = nullptr);
    static bool loadFromFile(ProjectData *project, const QString &path,
                             QString *errorMessage = nullptr);

    // ---- 编译为 Lua 脚本 ----
    // 输出可在 LVGL+Lua 运行时加载的脚本字符串。
    static QString compileToLua(const ProjectData &project);

    // 将 JSON 工程文件直接编译为同名 .lua 文件（写盘）
    static bool compileFileToLua(const QString &jsonPath,
                                 const QString &luaPath,
                                 QString *errorMessage = nullptr);

private:
    static QJsonObject screenToJson  (const ScreenData &screen);
    static ScreenData  screenFromJson(const QJsonObject &obj);
    static QJsonObject instanceToJson(const WidgetInstance &inst);
    static WidgetInstance instanceFromJson(const QJsonObject &obj);
};
