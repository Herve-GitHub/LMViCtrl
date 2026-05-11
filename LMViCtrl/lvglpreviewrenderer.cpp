#include "lvglpreviewrenderer.h"

#include "canvasitem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLibrary>
#include <QMetaType>
#include <QVariant>

namespace {

using RenderFn = int (*)(const char *luaWidgetPath,
                         const char *stateJson,
                         const char *fontPath,
                         int fontSize,
                         int width,
                         int height,
                         unsigned char *outArgb32,
                         int outStride,
                         char *errorBuf,
                         int errorBufSize);

RenderFn resolveRenderer(QString *error)
{
    static QLibrary library;
    static RenderFn fn = nullptr;
    static bool tried = false;

    if (fn) return fn;
    if (tried) return nullptr;
    tried = true;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath(QStringLiteral("LvglLuaBinding")),
        QDir(appDir).absoluteFilePath(QStringLiteral("LvglLuaBinding.dll")),
        QStringLiteral("LvglLuaBinding")
    };

    for (const QString &candidate : candidates) {
        library.setFileName(candidate);
        if (!library.load()) continue;
        fn = reinterpret_cast<RenderFn>(library.resolve("lvgl_lua_render_widget_to_argb32"));
        if (fn) return fn;
        library.unload();
    }

    if (error)
        *error = QStringLiteral("无法加载 LvglLuaBinding 离屏渲染接口");
    return nullptr;
}

QJsonValue variantToJson(const QVariant &value)
{
    switch (value.metaType().id()) {
    case QMetaType::Bool:
        return value.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Double:
    case QMetaType::Float:
        return value.toDouble();
    default:
        return value.toString();
    }
}

QByteArray buildStateJson(const WidgetMeta &meta, const WidgetInstance &inst, int width, int height)
{
    QJsonObject state;
    for (const PropertyMeta &property : meta.properties) {
        if (!property.name.isEmpty())
            state.insert(property.name, variantToJson(property.defaultValue));
    }
    for (auto it = inst.properties.cbegin(); it != inst.properties.cend(); ++it)
        state.insert(it.key(), variantToJson(it.value()));

    state.insert(QStringLiteral("x"), 0);
    state.insert(QStringLiteral("y"), 0);
    state.insert(QStringLiteral("width"), width);
    state.insert(QStringLiteral("height"), height);
    state.insert(QStringLiteral("design_mode"), true);

    return QJsonDocument(state).toJson(QJsonDocument::Compact);
}

} // namespace

QImage LvglPreviewRenderer::renderWidget(const WidgetMeta &meta,
                                         const WidgetInstance &inst,
                                         int width,
                                         int height,
                                         QString *error)
{
    if (width <= 0 || height <= 0 || meta.luaFilePath.isEmpty())
        return {};
    if (!QFileInfo::exists(meta.luaFilePath)) {
        if (error) *error = QStringLiteral("Lua 控件文件不存在: %1").arg(meta.luaFilePath);
        return {};
    }

    RenderFn fn = resolveRenderer(error);
    if (!fn) return {};

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    const QByteArray luaPath = QFile::encodeName(QFileInfo(meta.luaFilePath).absoluteFilePath());
    const QByteArray stateJson = buildStateJson(meta, inst, width, height);
    const QString fontPathString = CanvasItem::projectFontFilePath();
    const QByteArray fontPath = fontPathString.isEmpty()
        ? QByteArray()
        : QFile::encodeName(QFileInfo(fontPathString).absoluteFilePath());
    QByteArray errorBuf(1024, Qt::Uninitialized);
    errorBuf.fill('\0');

    const int rc = fn(luaPath.constData(),
                      stateJson.constData(),
                      fontPath.isEmpty() ? nullptr : fontPath.constData(),
                      CanvasItem::projectFontSize(),
                      width,
                      height,
                      image.bits(),
                      image.bytesPerLine(),
                      errorBuf.data(),
                      errorBuf.size());
    if (rc != 0) {
        if (error)
            *error = QString::fromUtf8(errorBuf.constData()).trimmed();
        return {};
    }
    return image;
}
