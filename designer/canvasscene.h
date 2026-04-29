#pragma once
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QSet>
#include <QSize>
#include <functional>
#include "WidgetMeta.h"

class QUndoStack;
class CanvasItem;
class QGraphicsRectItem;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);

    // 画布尺寸（与 ProjectData::target 绑定）
    void  setCanvasSize(int width, int height);
    QSize canvasSize() const { return m_canvasSize; }

    QUndoStack *undoStack() const { return m_undoStack; }

    // 注册 WidgetMeta（用于名称/颜色查找）
    void setWidgetMetas(const QList<WidgetMeta> &metas);

    // 通过 widgetId 取得 meta（属性面板使用）
    WidgetMeta widgetMeta(const QString &widgetId) const { return m_metaMap.value(widgetId); }

    // 名字生成器：提供一个候选基础名，回调返回工程内唯一名
    using NameGenerator = std::function<QString(const QString &baseName)>;
    void setNameGenerator(NameGenerator g) { m_nameGen = std::move(g); }

    // 公共编辑操作（均入 undo 栈）
    void deleteSelected();
    void copySelected();
    void pasteClipboard();

    // 所有实例（用于序列化）
    QList<WidgetInstance> allInstances() const;

    // 清空所有 widget 实例（保留背景），同时清空撤销栈与剪贴板
    void clearAllItems();

    // 用一组实例替换当前画布内容（用于工程加载，不入撤销栈）
    void loadInstances(const QList<WidgetInstance> &instances);

    // --- 由 undo 命令直接调用（不入栈）---
    void doAddItem   (const WidgetInstance &inst);
    void doRemoveItem(const QString &instanceId);
    void doSetGeometry(const QString &instanceId, const QRectF &rect);
    void doSetPositions(const QList<QPair<QString, QPointF>> &moves);

    // 属性面板编辑（不入栈，立刻生效）
    void setInstanceName    (const QString &instanceId, const QString &name);
    void setInstanceProperty(const QString &instanceId, const QString &key, const QVariant &value);

    // 通过 instanceId 取实例数据（属性面板使用）
    bool instance(const QString &instanceId, WidgetInstance *out) const;

    CanvasItem *findItem(const QString &instanceId) const;

signals:
    // 实例的某属性 / 名字 被外部修改后通知（属性面板用于回填）
    void instanceChanged(const QString &instanceId);

protected:
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent (QGraphicsSceneDragDropEvent *event) override;
    void dropEvent     (QGraphicsSceneDragDropEvent *event) override;
    void keyPressEvent (QKeyEvent *event) override;
    void mousePressEvent  (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private slots:
    void onResizeCommitted(const QString &instanceId,
                           const QRectF  &before,
                           const QRectF  &after);

private:
    CanvasItem *makeItem(const WidgetInstance &inst);

    QUndoStack        *m_undoStack = nullptr;
    QGraphicsRectItem *m_bgItem   = nullptr;
    QSize              m_canvasSize{1024, 768};

    QHash<QString, WidgetMeta> m_metaMap;         // widgetId → WidgetMeta

    // 移动 undo 追踪
    QHash<QString, QPointF> m_pressPositions;     // instanceId → pos at press
    QSet<QString>           m_resizingIds;         // 本次交互中完成 resize 的 items

    // 内部剪贴板
    QList<WidgetInstance> m_clipboard;
    int                   m_pasteCount = 0;

    NameGenerator m_nameGen;
};
