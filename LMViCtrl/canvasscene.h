#pragma once
#include <QGraphicsScene>
#include <QHash>
#include <QList>
#include <QColor>
#include <QSet>
#include <QSize>
#include <functional>
#include "widgemeta.h"

class QUndoStack;
class CanvasItem;
class QGraphicsRectItem;
class QGraphicsSceneContextMenuEvent;

class CanvasScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit CanvasScene(QObject *parent = nullptr);

    // 画布尺寸（与 ProjectData::target 绑定）
    void  setCanvasSize(int width, int height);
    QSize canvasSize() const { return m_canvasSize; }

    // 画布属性（与 ScreenData 绑定）
    void   setCanvasBackgroundColor(const QColor &color);
    QColor canvasBackgroundColor() const { return m_canvasBgColor; }

    QUndoStack *undoStack() const { return m_undoStack; }

    // 注册 WidgetMeta（用于名称/颜色查找）
    void setWidgetMetas(const QList<WidgetMeta> &metas);

    // 通过 widgetId 取得 meta（属性面板使用）
    WidgetMeta widgetMeta(const QString &widgetId) const { return m_metaMap.value(widgetId); }

    // 名字生成器：提供一个候选基础名，回调返回工程内唯一名
    using NameGenerator = std::function<QString(const QString &baseName)>;
    void setNameGenerator(NameGenerator g) { m_nameGen = std::move(g); }

    // 公共编辑操作（均入 undo 栈）
    enum class AlignMode { Left, Right, Top, Bottom, Center };
    enum class ZOrderMode { BringToFront, SendToBack, BringForward, SendBackward };

    void deleteSelected();
    void copySelected();
    void pasteClipboard();
    QList<WidgetInstance> selectedInstances() const;
    void pasteInstances(const QList<WidgetInstance> &instances, int pasteCount);
    void alignSelected(AlignMode mode);
    void changeSelectedZOrder(ZOrderMode mode);
    void groupSelected();
    void ungroupSelected();

    // 所有实例（用于序列化）
    QList<WidgetInstance> allInstances() const;
    QList<WidgetInstance> instancesSortedByZ() const;

    // 清空所有 widget 实例（保留背景），同时清空撤销栈与剪贴板
    void clearAllItems();

    // 用一组实例替换当前画布内容（用于工程加载，不入撤销栈）
    void loadInstances(const QList<WidgetInstance> &instances);

    // --- 由 undo 命令直接调用（不入栈）---
    void doAddItem   (const WidgetInstance &inst);
    void doRemoveItem(const QString &instanceId);
    void doSetGeometry(const QString &instanceId, const QRectF &rect);
    void doSetPositions(const QList<QPair<QString, QPointF>> &moves);
    void doSetZOrders(const QList<QPair<QString, int>> &orders);
    void doSetEventBindings(const QString &instanceId,
                            const QList<WidgetEventBinding> &bindings);
    void doLoadInstances(const QList<WidgetInstance> &instances);

    // 属性面板编辑（不入栈，立刻生效）
    void setInstanceName    (const QString &instanceId, const QString &name);
    void setInstanceProperty(const QString &instanceId, const QString &key, const QVariant &value);
    QList<WidgetEventBinding> instanceEventBindings(const QString &instanceId) const;
    void setInstanceEventBindings(const QString &instanceId,
                                  const QList<WidgetEventBinding> &bindings);
    void addInstanceEventAction(const QString &instanceId,
                                const QString &eventName,
                                const EventAction &action);
    void updateInstanceEventAction(const QString &instanceId,
                                   const QString &eventName,
                                   const QString &actionId,
                                   const EventAction &action);
    void removeInstanceEventAction(const QString &instanceId,
                                   const QString &eventName,
                                   const QString &actionId);

    // 通过 instanceId 取实例数据（属性面板使用）
    bool instance(const QString &instanceId, WidgetInstance *out) const;

    CanvasItem *findItem(const QString &instanceId) const;

signals:
    // 实例的某属性 / 名字 被外部修改后通知（属性面板用于回填）
    void instanceChanged(const QString &instanceId);

    void sceneItemsChanged();

    void instanceEventsChanged(const QString &instanceId);

    void cutRequested();
    void copyRequested();
    void pasteRequested();
    void deleteRequested();
    void bringToFrontRequested();
    void sendToBackRequested();
    void bringForwardRequested();
    void sendBackwardRequested();

    // 画布上拖动/缩放过程中的实时几何变化（只含 x/y/width/height，
    // 属性面板可仅刷新占位编辑器而不重建）
    void instanceGeometryChanged(const QString &instanceId);

    // 画布自身属性变化（背景色 / 尺寸）
    void canvasChanged();

    // 用户操作日志
    void operationLogged(const QString &message);

protected:
    void dragEnterEvent(QGraphicsSceneDragDropEvent *event) override;
    void dragMoveEvent (QGraphicsSceneDragDropEvent *event) override;
    void dropEvent     (QGraphicsSceneDragDropEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void keyPressEvent (QKeyEvent *event) override;
    void mousePressEvent  (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private slots:
    void onResizeCommitted(const QString &instanceId,
                           const QRectF  &before,
                           const QRectF  &after);

private:
    CanvasItem *makeItem(const WidgetInstance &inst);
    int nextZOrder() const;
    QList<CanvasItem *> canvasItemsSortedByZ() const;
    QList<WidgetInstance> normalizedZOrders(QList<WidgetInstance> instances) const;
    void expandSelectionToMoveGroups();
    QList<WidgetInstance> instancesWithDescendants(const QList<WidgetInstance> &roots) const;
    QString uniqueGroupName() const;

    QUndoStack        *m_undoStack = nullptr;
    QGraphicsRectItem *m_bgItem   = nullptr;
    QSize              m_canvasSize{1024, 768};
    QColor             m_canvasBgColor{"#000000"};
    bool               m_suppressOperationLog = false;
    bool               m_adjustingGroupSelection = false;

    QHash<QString, WidgetMeta> m_metaMap;         // widgetId → WidgetMeta

    // 移动 undo 追踪
    QHash<QString, QPointF> m_pressPositions;     // instanceId → pos at press
    QSet<QString>           m_resizingIds;         // 本次交互中完成 resize 的 items

    // 内部剪贴板
    QList<WidgetInstance> m_clipboard;
    int                   m_pasteCount = 0;

    NameGenerator m_nameGen;
};
