#include "bindinggraphview.h"

#include <algorithm>
#include <QBrush>
#include <QAction>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QPoint>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyleOptionGraphicsItem>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtMath>
#include <QUuid>

namespace {
constexpr qreal kNodeWidth = 240.0;
constexpr qreal kHeaderHeight = 34.0;
constexpr qreal kFirstPortTop = 42.0;
constexpr qreal kSectionLabelHeight = 24.0;
constexpr qreal kPortRowHeight = 22.0;
constexpr qreal kPortRadius = 5.0;
constexpr qreal kNodePadding = 10.0;
constexpr qreal kNodeHalfWidth = kNodeWidth / 2.0;
constexpr qreal kPortHitRadius = 14.0;
constexpr qreal kPortEdgePadding = 2.0;
constexpr qreal kMultiEdgePortSpread = 5.0;
constexpr qreal kMaxMultiEdgePortOffset = 12.0;
constexpr qreal kMinZoom = 0.25;
constexpr qreal kMaxZoom = 3.0;
constexpr qreal kZoomStep = 1.15;
constexpr qreal kGridOriginX = 16.0;
constexpr qreal kGridOriginY = 16.0;
constexpr qreal kGridColumnWidth = 320.0;
constexpr qreal kGridMinRowHeight = 230.0;
constexpr qreal kGridNodeMarginX = 22.0;
constexpr qreal kGridNodeMarginY = 18.0;
constexpr int kGridMinColumns = 6;
constexpr int kGridMinRows = 3;
constexpr const char *kBindingGraphNodeMime = "application/x-lm-victrl-binding-node";

qreal multiEdgeOffset(int index, int total)
{
    if (total <= 1) return 0.0;
    const qreal centeredIndex = index - (total - 1) / 2.0;
    return qBound(-kMaxMultiEdgePortOffset,
                  centeredIndex * kMultiEdgePortSpread,
                  kMaxMultiEdgePortOffset);
}

QPointF portConnectionPoint(const QPointF &center, bool output, int index = 0, int total = 1)
{
    const qreal direction = output ? 1.0 : -1.0;
    return center + QPointF(direction * (kPortRadius + kPortEdgePadding), multiEdgeOffset(index, total));
}

bool containsAnyToken(const QString &text, const QStringList &tokens)
{
    const QString normalized = text.toLower();
    for (const QString &token : tokens) {
        if (normalized.contains(token.toLower()))
            return true;
    }
    return false;
}

QString widgetClassText(const WidgetMeta *meta, const QString &fallback)
{
    if (!meta)
        return fallback;
    return QStringLiteral("%1 %2 %3 %4 %5")
        .arg(meta->id, meta->name, meta->type, meta->category, meta->tags.join(QLatin1Char(' ')));
}

QColor colorForWidgetNode(const WidgetMeta *meta, const QString &fallback)
{
    const QString text = widgetClassText(meta, fallback);

    if (containsAnyToken(text, { QStringLiteral("label"), QStringLiteral("image"), QStringLiteral("img"), QStringLiteral("chart"), QStringLiteral("展示") }))
        return QColor(QStringLiteral("#27AE60"));
    if (containsAnyToken(text, { QStringLiteral("textinput"), QStringLiteral("text_input"), QStringLiteral("textarea"), QStringLiteral("text_area"), QStringLiteral("slider"), QStringLiteral("复合") }))
        return QColor(QStringLiteral("#E67E22"));
    if (containsAnyToken(text, { QStringLiteral("button"), QStringLiteral("switch"), QStringLiteral("checkbox"), QStringLiteral("基础对象"), QStringLiteral("交互控件") }))
        return QColor(QStringLiteral("#2D6BE4"));
    return QColor(QStringLiteral("#008b8b"));
}

QColor colorForNodeType(const QString &type)
{
    if (type == QLatin1String("data")) return QColor(QStringLiteral("#8E44AD"));
    if (type == QLatin1String("screen")) return QColor(QStringLiteral("#607d8b"));
    return QColor(QStringLiteral("#008b8b"));
}

int bindablePropertyCount(const WidgetMeta *meta)
{
    if (!meta) return 0;
    int count = 0;
    for (const PropertyMeta &property : meta->properties) {
        if (property.bindable)
            ++count;
    }
    return count;
}

QStringList dataSettersForType(const QString &type)
{
    if (type == QLatin1String("boolean"))
        return { QStringLiteral("set_true"), QStringLiteral("set_false"), QStringLiteral("toggle") };
    if (type == QLatin1String("list"))
        return { QStringLiteral("push"), QStringLiteral("pop"), QStringLiteral("set_item"), QStringLiteral("clear") };
    if (type == QLatin1String("string"))
        return { QStringLiteral("set"), QStringLiteral("append"), QStringLiteral("format"), QStringLiteral("clear") };
    return { QStringLiteral("set"), QStringLiteral("increment"), QStringLiteral("decrement"), QStringLiteral("reset") };
}

QStringList dataEventsForType(const QString &type)
{
    QStringList events { QStringLiteral("onChange") };
    if (type == QLatin1String("number"))
        events << QStringLiteral("onAboveLimit") << QStringLiteral("onBelowLimit") << QStringLiteral("onEquals");
    else if (type == QLatin1String("boolean"))
        events << QStringLiteral("onTrue") << QStringLiteral("onFalse");
    else if (type == QLatin1String("list"))
        events << QStringLiteral("onEmpty");
    return events;
}

void addSectionLabel(QGraphicsItem *parent, const QString &text, qreal x, qreal y, qreal width)
{
    auto *label = new QGraphicsTextItem(text, parent);
    label->setDefaultTextColor(QColor(QStringLiteral("#b9c0c8")));
    label->setTextWidth(width - 16.0);
    label->setPos(x + 8.0, y + 4.0);
}

qreal portCenterY(int row)
{
    return kHeaderHeight + kFirstPortTop + row * kPortRowHeight;
}

QColor colorForEdgeType(const QString &type)
{
    if (type == QLatin1String("event_data")) return QColor(QStringLiteral("#e67e22"));
    if (type == QLatin1String("data_action")) return QColor(QStringLiteral("#8e44ad"));
    if (type == QLatin1String("property_binding")) return QColor(QStringLiteral("#95a5a6"));
    return QColor(QStringLiteral("#2d6be4"));
}

QString widgetNodeId(const QString &instanceId)
{
    return QStringLiteral("widget:%1").arg(instanceId);
}

QString dataNodeId(const QString &id)
{
    return QStringLiteral("data:%1").arg(id);
}

bool isWildcardValueType(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    return normalized.isEmpty()
        || normalized == QLatin1String("any")
        || normalized == QLatin1String("event")
        || normalized == QLatin1String("void");
}

QString normalizedValueType(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized == QLatin1String("int")
        || normalized == QLatin1String("integer")
        || normalized == QLatin1String("float")
        || normalized == QLatin1String("double")) {
        return QStringLiteral("number");
    }
    if (normalized == QLatin1String("bool"))
        return QStringLiteral("boolean");
    return normalized;
}

QString typeDisplayName(const QString &type)
{
    const QString normalized = normalizedValueType(type);
    if (normalized.isEmpty() || normalized == QLatin1String("any")) return QObject::tr("任意");
    if (normalized == QLatin1String("event")) return QObject::tr("事件");
    if (normalized == QLatin1String("void")) return QObject::tr("无参数");
    if (normalized == QLatin1String("number")) return QObject::tr("数字");
    if (normalized == QLatin1String("string")) return QObject::tr("文本");
    if (normalized == QLatin1String("boolean")) return QObject::tr("布尔");
    if (normalized == QLatin1String("color")) return QObject::tr("颜色");
    if (normalized == QLatin1String("list")) return QObject::tr("列表");
    return type;
}

bool valueTypesCompatible(const QString &sourceType, const QString &targetType)
{
    if (isWildcardValueType(sourceType) || isWildcardValueType(targetType)) return true;
    return normalizedValueType(sourceType) == normalizedValueType(targetType);
}

QString normalizeVariableName(const QString &name)
{
    QString result = name.trimmed();
    if (result.startsWith(QLatin1Char('$')))
        result.remove(0, 1);
    return result;
}

QString extractVariableName(const QString &text)
{
    const QString trimmed = text.trimmed();
    const int dollar = trimmed.indexOf(QLatin1Char('$'));
    if (dollar < 0) return trimmed;
    int end = dollar + 1;
    while (end < trimmed.size()) {
        const QChar ch = trimmed.at(end);
        if (!(ch.isLetterOrNumber() || ch == QLatin1Char('_')))
            break;
        ++end;
    }
    return trimmed.mid(dollar, end - dollar);
}

bool endpointsEqual(const BindingEndpoint &lhs, const BindingEndpoint &rhs)
{
    return lhs.nodeId == rhs.nodeId
        && lhs.nodeType == rhs.nodeType
        && lhs.refId == rhs.refId
        && lhs.refName == rhs.refName
        && lhs.portKind == rhs.portKind
        && lhs.portName == rhs.portName
        && lhs.valueType == rhs.valueType;
}

bool edgesEqual(const BindingEdge &lhs, const BindingEdge &rhs)
{
    return lhs.id == rhs.id
        && lhs.type == rhs.type
        && endpointsEqual(lhs.source, rhs.source)
        && endpointsEqual(lhs.target, rhs.target)
        && lhs.label == rhs.label
        && lhs.executionMode == rhs.executionMode
        && lhs.order == rhs.order
        && lhs.condition == rhs.condition
        && lhs.delayMs == rhs.delayMs
        && lhs.enabled == rhs.enabled
        && lhs.params == rhs.params;
}

class BindingNodeItem : public QGraphicsRectItem
{
public:
    BindingNodeItem(BindingGraphView *owner,
                    const QString &nodeId,
                    const QRectF &rect,
                    QGraphicsItem *parent = nullptr)
        : QGraphicsRectItem(rect, parent)
        , m_owner(owner)
        , m_nodeId(nodeId)
    {
        setFlags(QGraphicsItem::ItemIsSelectable
                 | QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
    }

    QString nodeId() const { return m_nodeId; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsRectItem::mousePressEvent(event);
        m_dragStartPos = pos();
        if (m_owner)
            m_owner->beginNodeMove(m_nodeId);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsRectItem::mouseReleaseEvent(event);
        if (!m_owner) return;
        const QPointF newPos = pos();
        if (qRound(m_dragStartPos.x()) == qRound(newPos.x())
            && qRound(m_dragStartPos.y()) == qRound(newPos.y())) {
            return;
        }
        m_owner->commitNodeMove(m_nodeId, m_dragStartPos, newPos);
    }

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == ItemPositionHasChanged && m_owner)
            m_owner->updateEdgesForNode(m_nodeId);
        return QGraphicsRectItem::itemChange(change, value);
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        QStyleOptionGraphicsItem copy = *option;
        copy.state &= ~QStyle::State_Selected;
        QGraphicsRectItem::paint(painter, &copy, widget);
        if (isSelected()) {
            painter->setPen(QPen(QColor(QStringLiteral("#4AA3FF")), 3.0));
            painter->setBrush(QColor(74, 163, 255, 44));
            painter->drawRoundedRect(rect().adjusted(1.5, 1.5, -1.5, -1.5), 6, 6);
            painter->setPen(QPen(QColor(QStringLiteral("#ffffff")), 1.2, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect().adjusted(5, 5, -5, -5), 5, 5);
        }
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override
    {
        setSelected(true);
        QMenu menu;
        QAction *deleteAction = menu.addAction(QObject::tr("删除节点"));
        QAction *chosen = menu.exec(event->screenPos());
        if (chosen == deleteAction && m_owner)
            m_owner->deleteNode(m_nodeId);
        event->accept();
    }

private:
    BindingGraphView *m_owner = nullptr;
    QString m_nodeId;
    QPointF m_dragStartPos;
};

class PortItem : public QGraphicsEllipseItem
{
public:
    PortItem(BindingGraphView *owner,
             const QString &portKey,
             bool output,
             const QRectF &rect,
             QGraphicsItem *parent = nullptr)
        : QGraphicsEllipseItem(rect, parent)
        , m_owner(owner)
        , m_portKey(portKey)
        , m_output(output)
    {
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(output ? Qt::LeftButton : Qt::NoButton);
        setCursor(output ? Qt::CrossCursor : Qt::ArrowCursor);
        setZValue(4);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (!m_output || event->button() != Qt::LeftButton || !m_owner) {
            QGraphicsEllipseItem::mousePressEvent(event);
            return;
        }
        m_owner->beginConnectionDrag(m_portKey, mapToScene(boundingRect().center()));
        event->accept();
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (m_output && m_owner) {
            m_owner->updateConnectionDrag(event->scenePos());
            event->accept();
            return;
        }
        QGraphicsEllipseItem::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        if (m_output && m_owner) {
            m_owner->finishConnectionDrag(event->scenePos());
            event->accept();
            return;
        }
        QGraphicsEllipseItem::mouseReleaseEvent(event);
    }

private:
    BindingGraphView *m_owner = nullptr;
    QString m_portKey;
    bool m_output = false;
};

class EdgeItem : public QGraphicsPathItem
{
public:
    EdgeItem(BindingGraphView *owner,
             const QString &edgeId,
             const QPainterPath &path,
             QGraphicsItem *parent = nullptr)
        : QGraphicsPathItem(path, parent)
        , m_owner(owner)
        , m_edgeId(edgeId)
    {
        setFlags(QGraphicsItem::ItemIsSelectable);
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
        setAcceptHoverEvents(true);
    }

    QString edgeId() const { return m_edgeId; }

protected:
    QPainterPath shape() const override
    {
        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        stroker.setCapStyle(Qt::RoundCap);
        stroker.setJoinStyle(Qt::RoundJoin);
        return stroker.createStroke(path());
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override
    {
        if (isSelected()) {
            QPen highlight(QColor(QStringLiteral("#f1c40f")), pen().widthF() + 4.0);
            highlight.setStyle(Qt::SolidLine);
            highlight.setCapStyle(Qt::RoundCap);
            painter->setPen(highlight);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(path());
        }
        QGraphicsPathItem::paint(painter, option, widget);
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override
    {
        setSelected(true);
        QMenu menu;
        QAction *deleteAction = menu.addAction(QObject::tr("删除连线"));
        QAction *chosen = menu.exec(event->screenPos());
        if (chosen == deleteAction && m_owner)
            m_owner->deleteEdge(m_edgeId);
        event->accept();
    }

private:
    BindingGraphView *m_owner = nullptr;
    QString m_edgeId;
};

class AddBindingEdgeCommand : public QUndoCommand
{
public:
    AddBindingEdgeCommand(BindingGraphView *view, const BindingEdge &edge)
        : QUndoCommand(QObject::tr("创建绑定 %1").arg(edge.label))
        , m_view(view)
        , m_edge(edge)
    {}

    void redo() override { if (m_view) m_view->cmdInsertEdge(m_edge, -1); }
    void undo() override { if (m_view) m_view->cmdRemoveEdge(m_edge.id); }

private:
    BindingGraphView *m_view = nullptr;
    BindingEdge m_edge;
};

class RemoveBindingEdgeCommand : public QUndoCommand
{
public:
    RemoveBindingEdgeCommand(BindingGraphView *view, const BindingEdge &edge, int index)
        : QUndoCommand(QObject::tr("删除绑定 %1").arg(edge.label))
        , m_view(view)
        , m_edge(edge)
        , m_index(index)
    {}

    void redo() override { if (m_view) m_view->cmdRemoveEdge(m_edge.id); }
    void undo() override { if (m_view) m_view->cmdInsertEdge(m_edge, m_index); }

private:
    BindingGraphView *m_view = nullptr;
    BindingEdge m_edge;
    int m_index = -1;
};

class UpdateBindingEdgeCommand : public QUndoCommand
{
public:
    UpdateBindingEdgeCommand(BindingGraphView *view, const BindingEdge &before, const BindingEdge &after)
        : QUndoCommand(QObject::tr("编辑绑定 %1").arg(after.label))
        , m_view(view)
        , m_before(before)
        , m_after(after)
    {}

    void redo() override { if (m_view) m_view->cmdUpdateEdge(m_after); }
    void undo() override { if (m_view) m_view->cmdUpdateEdge(m_before); }

private:
    BindingGraphView *m_view = nullptr;
    BindingEdge m_before;
    BindingEdge m_after;
};

class MoveBindingNodeCommand : public QUndoCommand
{
public:
    MoveBindingNodeCommand(BindingGraphView *view,
                           const QString &nodeId,
                           const QPointF &before,
                           const QPointF &after)
        : QUndoCommand(QObject::tr("移动绑定节点"))
        , m_view(view)
        , m_nodeId(nodeId)
        , m_before(before)
        , m_after(after)
    {}

    void redo() override { if (m_view) m_view->cmdSetNodePosition(m_nodeId, m_after); }
    void undo() override { if (m_view) m_view->cmdSetNodePosition(m_nodeId, m_before); }

private:
    BindingGraphView *m_view = nullptr;
    QString m_nodeId;
    QPointF m_before;
    QPointF m_after;
};

class SetBindingNodePositionsCommand : public QUndoCommand
{
public:
    SetBindingNodePositionsCommand(BindingGraphView *view,
                                   const QHash<QString, QPointF> &before,
                                   const QHash<QString, QPointF> &after)
        : QUndoCommand(QObject::tr("重新布局绑定图"))
        , m_view(view)
        , m_before(before)
        , m_after(after)
    {}

    void redo() override { if (m_view) m_view->cmdSetNodePositions(m_after); }
    void undo() override { if (m_view) m_view->cmdSetNodePositions(m_before); }

private:
    BindingGraphView *m_view = nullptr;
    QHash<QString, QPointF> m_before;
    QHash<QString, QPointF> m_after;
};

struct RemovedEdgeSnapshot
{
    BindingEdge edge;
    int index = -1;
};

class AddBindingNodeCommand : public QUndoCommand
{
public:
    AddBindingNodeCommand(BindingGraphView *view, const BindingNode &node)
        : QUndoCommand(QObject::tr("添加绑定节点 %1").arg(node.title.isEmpty() ? node.refName : node.title))
        , m_view(view)
        , m_node(node)
    {}

    void redo() override { if (m_view) m_view->cmdInsertNode(m_node, -1); }
    void undo() override { if (m_view) m_view->cmdRemoveNode(m_node.id); }

private:
    BindingGraphView *m_view = nullptr;
    BindingNode m_node;
};

class UpdateBindingNodeCommand : public QUndoCommand
{
public:
    UpdateBindingNodeCommand(BindingGraphView *view, const BindingNode &before, const BindingNode &after)
        : QUndoCommand(QObject::tr("移动绑定节点 %1").arg(after.title.isEmpty() ? after.refName : after.title))
        , m_view(view)
        , m_before(before)
        , m_after(after)
    {}

    void redo() override { if (m_view) m_view->cmdUpdateNode(m_after); }
    void undo() override { if (m_view) m_view->cmdUpdateNode(m_before); }

private:
    BindingGraphView *m_view = nullptr;
    BindingNode m_before;
    BindingNode m_after;
};

class RemoveBindingNodeCommand : public QUndoCommand
{
public:
    RemoveBindingNodeCommand(BindingGraphView *view,
                             const BindingNode &node,
                             int nodeIndex,
                             const QVector<RemovedEdgeSnapshot> &edges)
        : QUndoCommand(QObject::tr("删除绑定节点 %1").arg(node.title.isEmpty() ? node.refName : node.title))
        , m_view(view)
        , m_node(node)
        , m_nodeIndex(nodeIndex)
        , m_edges(edges)
    {}

    void redo() override
    {
        if (m_view)
            m_view->cmdRemoveNode(m_node.id);
    }

    void undo() override
    {
        if (!m_view)
            return;
        m_view->cmdInsertNode(m_node, m_nodeIndex);
        QVector<RemovedEdgeSnapshot> edges = m_edges;
        std::sort(edges.begin(), edges.end(), [](const RemovedEdgeSnapshot &lhs, const RemovedEdgeSnapshot &rhs) {
            return lhs.index < rhs.index;
        });
        for (const RemovedEdgeSnapshot &snapshot : std::as_const(edges))
            m_view->cmdInsertEdge(snapshot.edge, snapshot.index);
    }

private:
    BindingGraphView *m_view = nullptr;
    BindingNode m_node;
    int m_nodeIndex = -1;
    QVector<RemovedEdgeSnapshot> m_edges;
};
}

BindingGraphView::BindingGraphView(QWidget *parent)
    : QWidget(parent)
{
    m_undoStack = new QUndoStack(this);
    buildUi();
}

void BindingGraphView::setProjectData(ProjectData *project)
{
    if (m_project != project && m_undoStack)
        m_undoStack->clear();
    m_project = project;
    refreshGraph();
}

void BindingGraphView::setWidgetMetas(const QList<WidgetMeta> &metas)
{
    m_metas = metas;
    refreshGraph();
}

bool BindingGraphView::deleteSelectedEdges()
{
    if (!m_project || !m_scene || !m_undoStack) return false;

    QList<QString> edgeIds;
    for (QGraphicsItem *item : m_scene->selectedItems()) {
        const QString edgeId = edgeIdForItem(item);
        if (!edgeId.isEmpty() && !edgeIds.contains(edgeId))
            edgeIds.append(edgeId);
    }
    if (edgeIds.isEmpty()) return false;

    if (edgeIds.size() == 1)
        return deleteEdge(edgeIds.constFirst());

    m_undoStack->beginMacro(tr("删除 %1 条绑定").arg(edgeIds.size()));
    for (const QString &edgeId : std::as_const(edgeIds))
        deleteEdge(edgeId);
    m_undoStack->endMacro();
    return true;
}

bool BindingGraphView::deleteSelectedItems()
{
    if (!m_scene || !m_project || !m_undoStack) return false;

    QStringList nodeIds;
    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        if (it.value() && it.value()->isSelected())
            nodeIds.append(it.key());
    }

    if (!nodeIds.isEmpty()) {
        if (nodeIds.size() > 1)
            m_undoStack->beginMacro(tr("删除 %1 个绑定节点").arg(nodeIds.size()));
        for (const QString &nodeId : std::as_const(nodeIds))
            deleteNode(nodeId);
        if (nodeIds.size() > 1)
            m_undoStack->endMacro();
        return true;
    }

    return deleteSelectedEdges();
}

bool BindingGraphView::deleteNode(const QString &nodeId)
{
    if (!m_project || !m_undoStack || nodeId.isEmpty()) return false;

    int nodeIndex = -1;
    BindingNode nodeSnapshot;
    for (int i = 0; i < m_project->bindingGraph.nodes.size(); ++i) {
        const BindingNode &node = m_project->bindingGraph.nodes.at(i);
        if (node.id == nodeId) {
            nodeIndex = i;
            nodeSnapshot = node;
            break;
        }
    }
    if (nodeIndex < 0)
        return false;

    QVector<RemovedEdgeSnapshot> edgeSnapshots;
    for (int i = 0; i < m_project->bindingGraph.edges.size(); ++i) {
        const BindingEdge &edge = m_project->bindingGraph.edges.at(i);
        if (edge.source.nodeId == nodeId || edge.target.nodeId == nodeId)
            edgeSnapshots.append(RemovedEdgeSnapshot{ edge, i });
    }

    m_undoStack->push(new RemoveBindingNodeCommand(this, nodeSnapshot, nodeIndex, edgeSnapshots));
    const QString title = nodeSnapshot.title.isEmpty() ? nodeSnapshot.refName : nodeSnapshot.title;
    emit statusMessageRequested(tr("已删除绑定节点：%1").arg(title.isEmpty() ? nodeId : title));
    return true;
}

bool BindingGraphView::deleteEdge(const QString &edgeId)
{
    if (!m_project || !m_undoStack) return false;
    const int index = edgeIndex(edgeId);
    if (index < 0) return false;
    const BindingEdge edge = m_project->bindingGraph.edges.at(index);
    m_undoStack->push(new RemoveBindingEdgeCommand(this, edge, index));
    emit statusMessageRequested(tr("已删除绑定：%1").arg(edge.label));
    return true;
}

BindingNode BindingGraphView::nodeSnapshot(const QString &nodeId) const
{
    if (BindingNode *node = findNode(nodeId))
        return *node;
    return BindingNode{};
}

BindingEdge BindingGraphView::edgeSnapshot(const QString &edgeId) const
{
    const int index = edgeIndex(edgeId);
    if (!m_project || index < 0) return BindingEdge{};
    return m_project->bindingGraph.edges.at(index);
}

bool BindingGraphView::updateEdge(const BindingEdge &edge)
{
    if (!m_project || !m_undoStack || edge.id.isEmpty()) return false;
    const int index = edgeIndex(edge.id);
    if (index < 0) return false;
    const BindingEdge before = m_project->bindingGraph.edges.at(index);
    if (edgesEqual(before, edge)) return false;
    m_undoStack->push(new UpdateBindingEdgeCommand(this, before, edge));
    emit statusMessageRequested(tr("已更新绑定：%1").arg(edge.label));
    return true;
}

void BindingGraphView::selectEdge(const QString &edgeId)
{
    if (!m_scene) return;
    m_scene->clearSelection();
    if (QGraphicsItem *item = m_edgeItems.value(edgeId, nullptr)) {
        item->setSelected(true);
        m_selectedEdgeId = edgeId;
        emit selectedEdgeChanged(edgeId);
        emit selectedNodeChanged(QString());
    } else if (!edgeId.isEmpty()) {
        m_selectedEdgeId = edgeId;
        emit selectedEdgeChanged(edgeId);
        emit selectedNodeChanged(QString());
    } else if (!m_selectedEdgeId.isEmpty()) {
        m_selectedEdgeId.clear();
        emit selectedEdgeChanged(QString());
        emit selectedNodeChanged(QString());
    } else {
        emit selectedNodeChanged(QString());
    }
}

bool BindingGraphView::quickBindProperty(const QString &instanceId,
                                         const QString &propertyName,
                                         const QString &valueType,
                                         const QString &preferredVariableName)
{
    if (!m_project || instanceId.isEmpty() || propertyName.isEmpty()) return false;

    refreshGraph();

    QStringList candidates;
    const QString preferred = extractVariableName(preferredVariableName);
    if (!preferred.isEmpty())
        candidates << preferred;
    candidates << QStringLiteral("$%1").arg(propertyName) << propertyName;

    const DataVariable *matchedVariable = nullptr;
    for (const QString &candidate : std::as_const(candidates)) {
        const QString normalizedCandidate = normalizeVariableName(candidate);
        for (const DataVariable &variable : std::as_const(m_project->dataVariables)) {
            const QString normalizedName = normalizeVariableName(variable.name);
            const QString normalizedId = normalizeVariableName(variable.id);
            if (normalizedCandidate.compare(normalizedName, Qt::CaseInsensitive) == 0
                || normalizedCandidate.compare(normalizedId, Qt::CaseInsensitive) == 0) {
                matchedVariable = &variable;
                break;
            }
        }
        if (matchedVariable) break;
    }

    if (!matchedVariable) {
        emit statusMessageRequested(tr("快速绑定失败：未找到与 %1 匹配的数据变量").arg(propertyName));
        return false;
    }

    const QString sourceKey = portKey(dataNodeId(matchedVariable->id), QStringLiteral("data_get"), QStringLiteral("onChange"));
    const QString targetKey = portKey(widgetNodeId(instanceId), QStringLiteral("property"), propertyName);
    if (!m_ports.contains(sourceKey) || !m_ports.contains(targetKey)) {
        emit statusMessageRequested(tr("快速绑定失败：未找到可绑定端口"));
        return false;
    }

    const PortVisual source = m_ports.value(sourceKey);
    PortVisual target = m_ports.value(targetKey);
    if (!valueType.isEmpty())
        target.valueType = valueType;
    const CompatibilityResult compatibility = connectionCompatibility(source, target);
    if (!compatibility.ok) {
        emit statusMessageRequested(tr("快速绑定失败：%1").arg(compatibility.message));
        return false;
    }

    if (edgeExists(sourceKey, targetKey)) {
        for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
            if (portKey(edge.source) == sourceKey && portKey(edge.target) == targetKey) {
                selectEdge(edge.id);
                break;
            }
        }
        emit statusMessageRequested(tr("属性绑定已存在"));
        return true;
    }

    const bool created = createEdge(sourceKey, targetKey);
    if (created) {
        for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
            if (portKey(edge.source) == sourceKey && portKey(edge.target) == targetKey) {
                selectEdge(edge.id);
                break;
            }
        }
        emit statusMessageRequested(tr("已快速绑定属性：%1 -> %2")
            .arg(matchedVariable->name.isEmpty() ? matchedVariable->id : matchedVariable->name,
                 propertyName));
    }
    return created;
}

void BindingGraphView::refreshGraph()
{
    if (!m_scene) return;
    cancelConnectionDrag();
    reconcileGraphNodes();
    rebuildScene();
    updateStatus();
}

void BindingGraphView::fitToView()
{
    if (!m_view || !m_scene) return;
    const QRectF bounds = m_scene->itemsBoundingRect().adjusted(-80, -80, 80, 80);
    if (bounds.isValid() && !bounds.isEmpty())
        m_view->fitInView(bounds, Qt::KeepAspectRatio);
}

void BindingGraphView::autoLayout()
{
    if (!m_project || !m_undoStack) return;
    QHash<QString, QPointF> before;
    QHash<QString, QPointF> after;
    int widgetIndex = 0;
    int dataIndex = 0;
    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        before.insert(node.id, QPointF(node.x, node.y));
        const QPointF pos = defaultNodePosition(node.type == QLatin1String("data") ? dataIndex++ : widgetIndex++, node.type);
        after.insert(node.id, pos);
    }

    if (before == after) return;
    m_undoStack->push(new SetBindingNodePositionsCommand(this, before, after));
    emit statusMessageRequested(tr("绑定图已重新布局"));
}

void BindingGraphView::commitNodeMove(const QString &nodeId, const QPointF &oldPos, const QPointF &newPos)
{
    if (!m_undoStack) return;

    if (!m_nodeMoveStartPositions.isEmpty()) {
        QHash<QString, QPointF> before;
        QHash<QString, QPointF> after;

        for (auto it = m_nodeMoveStartPositions.constBegin(); it != m_nodeMoveStartPositions.constEnd(); ++it) {
            QGraphicsItem *item = m_nodeItems.value(it.key(), nullptr);
            if (!item) continue;

            const QPointF oldRounded(qRound(it.value().x()), qRound(it.value().y()));
            const QPointF newRounded(qRound(item->pos().x()), qRound(item->pos().y()));
            before.insert(it.key(), oldRounded);
            after.insert(it.key(), newRounded);
        }

        m_nodeMoveStartPositions.clear();
        if (before == after) return;
        m_undoStack->push(new SetBindingNodePositionsCommand(this, before, after));
        return;
    }

    const QPointF before(qRound(oldPos.x()), qRound(oldPos.y()));
    const QPointF after(qRound(newPos.x()), qRound(newPos.y()));
    if (before == after) return;
    m_undoStack->push(new MoveBindingNodeCommand(this, nodeId, before, after));
}

void BindingGraphView::beginNodeMove(const QString &nodeId)
{
    if (!m_scene || !m_nodeMoveStartPositions.isEmpty()) return;

    for (auto it = m_nodeItems.constBegin(); it != m_nodeItems.constEnd(); ++it) {
        QGraphicsItem *item = it.value();
        if (item && item->isSelected())
            m_nodeMoveStartPositions.insert(it.key(), item->pos());
    }

    if (m_nodeMoveStartPositions.isEmpty()) {
        if (QGraphicsItem *item = m_nodeItems.value(nodeId, nullptr))
            m_nodeMoveStartPositions.insert(nodeId, item->pos());
    }
}

void BindingGraphView::updateEdgesForNode(const QString &nodeId)
{
    Q_UNUSED(nodeId)
    if (!m_project || !m_scene) return;

    for (auto it = m_ports.begin(); it != m_ports.end(); ++it) {
        if (it->item)
            it->scenePos = it->item->mapToScene(it->item->boundingRect().center());
    }

    QHash<QString, int> sourceTotals;
    QHash<QString, int> targetTotals;
    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        const QString sourceKey = portKey(edge.source);
        const QString targetKey = portKey(edge.target);
        if (m_ports.contains(sourceKey) && m_ports.contains(targetKey)) {
            ++sourceTotals[sourceKey];
            ++targetTotals[targetKey];
        }
    }

    QHash<QString, int> sourceIndexes;
    QHash<QString, int> targetIndexes;
    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        const QString sourceKey = portKey(edge.source);
        const QString targetKey = portKey(edge.target);
        const PortVisual source = m_ports.value(sourceKey);
        const PortVisual target = m_ports.value(targetKey);
        if (!source.item || !target.item) continue;

        const int sourceIndex = sourceIndexes.value(sourceKey, 0);
        const int targetIndex = targetIndexes.value(targetKey, 0);
        ++sourceIndexes[sourceKey];
        ++targetIndexes[targetKey];

        auto *edgeItem = dynamic_cast<QGraphicsPathItem *>(m_edgeItems.value(edge.id, nullptr));
        if (!edgeItem) continue;

        const QPointF sourcePoint = portConnectionPoint(source.scenePos, true, sourceIndex, sourceTotals.value(sourceKey, 1));
        const QPointF targetPoint = portConnectionPoint(target.scenePos, false, targetIndex, targetTotals.value(targetKey, 1));
        edgeItem->setPath(connectionPath(sourcePoint, targetPoint));
    }
}

void BindingGraphView::beginConnectionDrag(const QString &portKey, const QPointF &scenePos)
{
    if (!m_project || !m_ports.contains(portKey)) return;
    const PortVisual source = m_ports.value(portKey);
    if (!source.output) return;

    cancelConnectionDrag();
    m_dragSourceKey = portKey;
    const QPointF startPoint = portConnectionPoint(scenePos, true);
    m_dragPathItem = new QGraphicsPathItem(connectionPath(startPoint, startPoint));
    QPen pen(source.color.lighter(125), 2.0, Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    m_dragPathItem->setPen(pen);
    m_dragPathItem->setZValue(30);
    m_scene->addItem(m_dragPathItem);
    updatePortHighlights();
}

void BindingGraphView::updateConnectionDrag(const QPointF &scenePos)
{
    if (m_dragSourceKey.isEmpty() || !m_dragPathItem) return;
    PortVisual source = m_ports.value(m_dragSourceKey);
    if (source.item)
        source.scenePos = source.item->mapToScene(source.item->boundingRect().center());
    m_dragPathItem->setPath(connectionPath(portConnectionPoint(source.scenePos, true), scenePos));
    const QString hoverKey = portAt(scenePos);
    updatePortHighlights(hoverKey);
    if (m_statusLabel) {
        if (hoverKey.isEmpty() || !m_ports.contains(hoverKey)) {
            m_statusLabel->setText(tr("拖拽中：%1 输出 %2").arg(source.label, typeDisplayName(source.valueType)));
        } else {
            m_statusLabel->setText(connectionCompatibility(source, m_ports.value(hoverKey)).message);
        }
    }
}

void BindingGraphView::finishConnectionDrag(const QPointF &scenePos)
{
    const QString sourceKey = m_dragSourceKey;
    const QString targetKey = portAt(scenePos);
    const bool created = createEdge(sourceKey, targetKey);
    cancelConnectionDrag();
    if (created) {
        rebuildEdges();
        updateStatus();
    }
}

void BindingGraphView::cancelConnectionDrag()
{
    if (m_dragPathItem) {
        m_scene->removeItem(m_dragPathItem);
        delete m_dragPathItem;
        m_dragPathItem = nullptr;
    }
    m_dragSourceKey.clear();
    resetPortHighlights();
    updateStatus();
}

void BindingGraphView::cmdInsertNode(const BindingNode &node, int index)
{
    if (!m_project || node.id.isEmpty()) return;
    for (const BindingNode &existing : std::as_const(m_project->bindingGraph.nodes)) {
        if (existing.id == node.id)
            return;
    }

    const int insertIndex = index < 0
        ? m_project->bindingGraph.nodes.size()
        : qBound(0, index, m_project->bindingGraph.nodes.size());
    m_project->bindingGraph.nodes.insert(insertIndex, node);
    m_selectedEdgeId.clear();
    rebuildScene();
    updateStatus();
    if (QGraphicsItem *item = m_nodeItems.value(node.id, nullptr)) {
        m_scene->clearSelection();
        item->setSelected(true);
    }
    emit graphChanged();
    emit selectedEdgeChanged(QString());
    emit selectedNodeChanged(node.id);
}

void BindingGraphView::cmdRemoveNode(const QString &nodeId)
{
    if (!m_project || nodeId.isEmpty()) return;

    for (int i = m_project->bindingGraph.edges.size() - 1; i >= 0; --i) {
        const BindingEdge &edge = m_project->bindingGraph.edges.at(i);
        if (edge.source.nodeId == nodeId || edge.target.nodeId == nodeId)
            m_project->bindingGraph.edges.removeAt(i);
    }

    for (int i = 0; i < m_project->bindingGraph.nodes.size(); ++i) {
        if (m_project->bindingGraph.nodes.at(i).id == nodeId) {
            m_project->bindingGraph.nodes.removeAt(i);
            break;
        }
    }

    m_selectedEdgeId.clear();
    rebuildScene();
    updateStatus();
    emit graphChanged();
    emit selectedEdgeChanged(QString());
    emit selectedNodeChanged(QString());
}

void BindingGraphView::cmdUpdateNode(const BindingNode &node)
{
    if (!m_project || node.id.isEmpty()) return;
    for (BindingNode &existing : m_project->bindingGraph.nodes) {
        if (existing.id == node.id) {
            existing = node;
            break;
        }
    }
    m_selectedEdgeId.clear();
    rebuildScene();
    updateStatus();
    if (QGraphicsItem *item = m_nodeItems.value(node.id, nullptr)) {
        m_scene->clearSelection();
        item->setSelected(true);
    }
    emit graphChanged();
    emit selectedEdgeChanged(QString());
    emit selectedNodeChanged(node.id);
}

void BindingGraphView::cmdAddEdge(const BindingEdge &edge)
{
    cmdInsertEdge(edge, -1);
}

void BindingGraphView::cmdInsertEdge(const BindingEdge &edge, int index)
{
    if (!m_project || edge.id.isEmpty()) return;
    for (const BindingEdge &existing : std::as_const(m_project->bindingGraph.edges)) {
        if (existing.id == edge.id)
            return;
    }
    const int insertIndex = index < 0
        ? m_project->bindingGraph.edges.size()
        : qBound(0, index, m_project->bindingGraph.edges.size());
    m_project->bindingGraph.edges.insert(insertIndex, edge);
    rebuildEdges();
    updateStatus();
    emit graphChanged();
}

void BindingGraphView::cmdRemoveEdge(const QString &edgeId)
{
    if (!m_project || edgeId.isEmpty()) return;
    for (int i = 0; i < m_project->bindingGraph.edges.size(); ++i) {
        if (m_project->bindingGraph.edges.at(i).id == edgeId) {
            m_project->bindingGraph.edges.removeAt(i);
            break;
        }
    }
    if (m_selectedEdgeId == edgeId) {
        m_selectedEdgeId.clear();
        emit selectedEdgeChanged(QString());
    }
    rebuildEdges();
    updateStatus();
    emit graphChanged();
}

void BindingGraphView::cmdUpdateEdge(const BindingEdge &edge)
{
    if (!m_project || edge.id.isEmpty()) return;
    const int index = edgeIndex(edge.id);
    if (index < 0) return;
    m_project->bindingGraph.edges[index] = edge;
    m_selectedEdgeId = edge.id;
    rebuildEdges();
    updateStatus();
    emit graphChanged();
    emit selectedEdgeChanged(edge.id);
}

void BindingGraphView::cmdSetNodePosition(const QString &nodeId, const QPointF &pos)
{
    BindingNode *node = findNode(nodeId);
    if (!node) return;
    const QPointF rounded(qRound(pos.x()), qRound(pos.y()));
    node->x = qRound(rounded.x());
    node->y = qRound(rounded.y());

    if (QGraphicsItem *item = m_nodeItems.value(nodeId, nullptr)) {
        if (item->pos() != rounded)
            item->setPos(rounded);
    }
    rebuildEdges();
    updateStatus();
    emit graphChanged();
}

void BindingGraphView::cmdSetNodePositions(const QHash<QString, QPointF> &positions)
{
    if (!m_project) return;
    for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
        if (BindingNode *node = findNode(it.key())) {
            node->x = qRound(it.value().x());
            node->y = qRound(it.value().y());
        }
        if (QGraphicsItem *item = m_nodeItems.value(it.key(), nullptr)) {
            const QPointF rounded(qRound(it.value().x()), qRound(it.value().y()));
            if (item->pos() != rounded)
                item->setPos(rounded);
        }
    }
    rebuildEdges();
    updateStatus();
    emit graphChanged();
}

bool BindingGraphView::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_view || (m_view && watched == m_view->viewport()))
        && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove)) {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (dragEvent->mimeData()->hasFormat(QLatin1String(kBindingGraphNodeMime))) {
            updateDropHighlights(m_view->mapToScene(dragEvent->position().toPoint()));
            dragEvent->acceptProposedAction();
            return true;
        }
    }

    if ((watched == m_view || (m_view && watched == m_view->viewport())) && event->type() == QEvent::DragLeave) {
        clearDropHighlights();
        event->accept();
        return true;
    }

    if ((watched == m_view || (m_view && watched == m_view->viewport())) && event->type() == QEvent::Drop) {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        if (dropEvent->mimeData()->hasFormat(QLatin1String(kBindingGraphNodeMime))) {
            emit projectSyncRequested();
            const QPointF scenePos = m_view->mapToScene(dropEvent->position().toPoint());
            if (addNodeFromMimeData(dropEvent->mimeData(), scenePos)) {
                clearDropHighlights();
                dropEvent->acceptProposedAction();
                return true;
            }
            clearDropHighlights();
            emit statusMessageRequested(tr("请将节点放到高亮区域"));
            dropEvent->ignore();
            return true;
        }
    }

    if ((watched == m_view || (m_view && watched == m_view->viewport())) && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers().testFlag(Qt::ControlModifier)) {
            const int delta = wheelEvent->angleDelta().y();
            if (delta == 0) {
                event->accept();
                return true;
            }

            const qreal currentZoom = m_view->transform().m11();
            const qreal requestedFactor = delta > 0 ? kZoomStep : (1.0 / kZoomStep);
            const qreal targetZoom = qBound(kMinZoom, currentZoom * requestedFactor, kMaxZoom);
            if (!qFuzzyCompare(currentZoom, targetZoom)) {
                const qreal appliedFactor = targetZoom / currentZoom;
                m_view->scale(appliedFactor, appliedFactor);
                if (m_statusLabel)
                    m_statusLabel->setText(tr("缩放 %1%").arg(qRound(targetZoom * 100.0)));
            }

            event->accept();
            return true;
        }
    }

    if ((watched == m_view || (m_view && watched == m_view->viewport())) && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
            if (deleteSelectedItems()) {
                event->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void BindingGraphView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_scene && !m_scene->items().isEmpty())
        fitToView();
}

void BindingGraphView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("bindingGraphToolbar"));
    bar->setStyleSheet(QStringLiteral(
        "QFrame#bindingGraphToolbar { background:#252526; border-bottom:1px solid #3a3a3a; }"
        "QLabel { color:#cfcfcf; }"));
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    barLayout->setSpacing(8);

    m_statusLabel = new QLabel(bar);

    barLayout->addStretch();
    barLayout->addWidget(m_statusLabel);
    root->addWidget(bar);

    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(QStringLiteral("#1e1e1e")));
    m_view = new QGraphicsView(m_scene, this);
    m_view->setAcceptDrops(true);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setAcceptDrops(true);
    root->addWidget(m_view, 1);

    connect(m_scene, &QGraphicsScene::selectionChanged, this, &BindingGraphView::onSceneSelectionChanged);
}

void BindingGraphView::reconcileGraphNodes()
{
    if (!m_project) return;

    QHash<QString, WidgetInstance> widgetsById;
    QHash<QString, DataVariable> variablesById;

    for (const ScreenData &screen : std::as_const(m_project->screens)) {
        for (const WidgetInstance &instance : screen.widgets) {
            if (instance.isGroup) continue;
            widgetsById.insert(instance.instanceId, instance);
        }
    }

    for (const DataVariable &variable : std::as_const(m_project->dataVariables))
        variablesById.insert(variable.id, variable);

    for (int i = m_project->bindingGraph.nodes.size() - 1; i >= 0; --i) {
        BindingNode &node = m_project->bindingGraph.nodes[i];
        if (node.type == QLatin1String("widget")) {
            if (!widgetsById.contains(node.refId)) {
                m_project->bindingGraph.nodes.removeAt(i);
                continue;
            }
            const WidgetInstance instance = widgetsById.value(node.refId);
            node.refName = instance.name;
            node.title = instance.name.isEmpty() ? instance.widgetId : instance.name;
            node.visualState.insert(QStringLiteral("widgetId"), instance.widgetId);
        } else if (node.type == QLatin1String("data")) {
            if (!variablesById.contains(node.refId)) {
                m_project->bindingGraph.nodes.removeAt(i);
                continue;
            }
            const DataVariable variable = variablesById.value(node.refId);
            node.refName = variable.name;
            node.title = variable.name.isEmpty() ? variable.id : variable.name;
        } else {
            m_project->bindingGraph.nodes.removeAt(i);
        }
    }

    QSet<QString> liveIds;
    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes))
        liveIds.insert(node.id);

    for (int i = m_project->bindingGraph.edges.size() - 1; i >= 0; --i) {
        const BindingEdge &edge = m_project->bindingGraph.edges.at(i);
        if (!liveIds.contains(edge.source.nodeId) || !liveIds.contains(edge.target.nodeId))
            m_project->bindingGraph.edges.removeAt(i);
    }
}

void BindingGraphView::rebuildScene()
{
    m_dropHighlightItems.clear();
    m_scene->clear();
    m_nodeItems.clear();
    m_ports.clear();
    m_edgeItems.clear();
    m_dragPathItem = nullptr;
    m_dragSourceKey.clear();
    if (!m_project) return;

    const QHash<int, qreal> rowHeights = gridRowHeights();
    int maxRow = 0;
    int maxColumn = 0;
    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        maxRow = qMax(maxRow, nodeGridRow(node));
        maxColumn = qMax(maxColumn, nodeGridColumn(node));
    }
    const int gridRows = qMax(kGridMinRows, maxRow + 2);
    const int gridColumns = qMax(kGridMinColumns, maxColumn + 3);
    const QRectF lastCell = gridCellRect(gridRows - 1, gridColumns - 1, rowHeights);
    m_scene->setSceneRect(QRectF(0, 0, lastCell.right() + 32.0, lastCell.bottom() + 32.0));

    for (BindingNode &node : m_project->bindingGraph.nodes) {
        const bool isData = node.type == QLatin1String("data");
        WidgetInstance fallbackInstance;
        const WidgetInstance *instance = nullptr;
        const WidgetMeta *meta = nullptr;
        const DataVariable *variable = nullptr;

        int inputRows = 0;
        int propertyRows = 0;
        int outputRows = 0;
        if (isData) {
            variable = findDataVariable(node.refId);
            if (variable) {
                inputRows = dataSettersForType(variable->type).size();
                outputRows = dataEventsForType(variable->type).size();
            }
        } else {
            instance = findWidgetInstance(node.refId);
            if (!instance) {
                fallbackInstance.instanceId = node.refId;
                fallbackInstance.name = node.refName;
                fallbackInstance.widgetId = node.visualState.value(QStringLiteral("widgetId")).toString();
                if (!fallbackInstance.widgetId.isEmpty())
                    instance = &fallbackInstance;
            }
            if (instance) {
                meta = findWidgetMeta(instance->widgetId);
                if (meta) {
                    inputRows = meta->actions.size();
                    propertyRows = bindablePropertyCount(meta);
                    outputRows = meta->eventDefs.size();
                }
            }
        }
        if (outputRows == 0)
            outputRows = 1;

        const QColor accent = isData
            ? colorForNodeType(node.type)
            : colorForWidgetNode(meta, QStringLiteral("%1 %2").arg(node.title, node.refName));
        const bool showInputSection = inputRows > 0;
        const bool showPropertySection = propertyRows > 0;
        const bool showOutputSection = outputRows > 0;
        const int propertyTitleRows = (showInputSection && showPropertySection) ? 1 : 0;
        const int leftRows = inputRows + propertyTitleRows + propertyRows;
        const int rows = qMax(leftRows, outputRows);
        const qreal height = qMax<qreal>(122.0, kHeaderHeight + kFirstPortTop + rows * kPortRowHeight + kNodePadding);
        const QRectF rect(0, 0, kNodeWidth, height);
        const QRectF cellRect = gridCellRect(nodeGridRow(node), nodeGridColumn(node), rowHeights);
        node.x = qRound(cellRect.left() + kGridNodeMarginX);
        node.y = qRound(cellRect.top() + kGridNodeMarginY);
        auto *nodeItem = new BindingNodeItem(this, node.id, rect);
        nodeItem->setPos(node.x, node.y);
        nodeItem->setPen(QPen(accent, 1.8));
        nodeItem->setBrush(QColor(QStringLiteral("#252526")));
        m_scene->addItem(nodeItem);
        m_nodeItems.insert(node.id, nodeItem);

        auto *header = new QGraphicsRectItem(0, 0, kNodeWidth, kHeaderHeight, nodeItem);
        header->setPen(Qt::NoPen);
        header->setBrush(accent);

        const qreal bodyTop = kHeaderHeight;
        const qreal propertyTop = showInputSection
            ? portCenterY(inputRows) - kPortRowHeight / 2.0
            : bodyTop;
        if (showInputSection) {
            auto *inputBackground = new QGraphicsRectItem(0, bodyTop,
                                                          kNodeHalfWidth,
                                                          showPropertySection ? propertyTop - bodyTop : height - bodyTop,
                                                          nodeItem);
            inputBackground->setPen(Qt::NoPen);
            inputBackground->setBrush(QColor(QStringLiteral("#30363D")));
            addSectionLabel(nodeItem, isData ? tr("输入动作") : tr("输入事件"),
                            0, bodyTop + 4.0, kNodeHalfWidth);
        }

        if (showOutputSection) {
            auto *outputBackground = new QGraphicsRectItem(kNodeHalfWidth, bodyTop,
                                                           kNodeHalfWidth,
                                                           height - bodyTop,
                                                           nodeItem);
            outputBackground->setPen(Qt::NoPen);
            outputBackground->setBrush(QColor(QStringLiteral("#2B3036")));
            addSectionLabel(nodeItem, isData ? tr("输出数据") : tr("输出事件"),
                            kNodeHalfWidth, bodyTop + 4.0, kNodeHalfWidth);
        }

        if (showPropertySection) {
            auto *propertyBackground = new QGraphicsRectItem(0, propertyTop,
                                                             kNodeHalfWidth,
                                                             height - propertyTop,
                                                             nodeItem);
            propertyBackground->setPen(Qt::NoPen);
            propertyBackground->setBrush(QColor(QStringLiteral("#252A30")));
            addSectionLabel(nodeItem, tr("属性设置"), 0, propertyTop, kNodeHalfWidth);
        }

        auto *title = new QGraphicsTextItem(node.title.isEmpty() ? node.refName : node.title, nodeItem);
        title->setDefaultTextColor(Qt::white);
        title->setTextWidth(kNodeWidth - 24);
        title->setPos(12, 6);

        int leftRow = 0;
        int rightRow = 0;
        if (isData) {
            if (variable)
                addDataNodePorts(*variable, nodeItem, &leftRow, &rightRow);
        } else {
            if (instance)
                addWidgetNodePorts(*instance, meta, nodeItem, &leftRow, &rightRow);
        }
    }

    rebuildEdges();
}

void BindingGraphView::addWidgetNodePorts(const WidgetInstance &instance,
                                          const WidgetMeta *meta,
                                          QGraphicsItem *nodeItem,
                                          int *leftRow,
                                          int *rightRow)
{
    const QString nodeId = widgetNodeId(instance.instanceId);
    if (meta) {
        for (const ActionDef &action : meta->actions)
            addPort(nodeItem, nodeId, QStringLiteral("action"), action.name,
                    action.label.isEmpty() ? action.name : action.label,
                    action.valueType.isEmpty() ? QStringLiteral("any") : action.valueType,
                    false, (*leftRow)++, QColor(QStringLiteral("#27ae60")));
        const bool hasInputPorts = *leftRow > 0;
        bool hasPropertyPorts = false;
        for (const PropertyMeta &property : meta->properties) {
            if (!property.bindable) continue;
            if (!hasPropertyPorts && hasInputPorts)
                ++(*leftRow);
            hasPropertyPorts = true;
            addPort(nodeItem, nodeId, QStringLiteral("property"), property.name,
                    property.label.isEmpty() ? property.name : property.label,
                    property.type.isEmpty() ? QStringLiteral("any") : property.type,
                    false, (*leftRow)++, QColor(QStringLiteral("#95a5a6")));
        }
        for (const EventDef &event : meta->eventDefs)
            addPort(nodeItem, nodeId, QStringLiteral("event"), event.name,
                    event.label.isEmpty() ? event.name : event.label,
                    QStringLiteral("event"),
                    true, (*rightRow)++, QColor(QStringLiteral("#3498db")));
    }

    if (*rightRow == 0) {
        addPort(nodeItem, nodeId, QStringLiteral("event"), QStringLiteral("clicked"),
                tr("点击"), QStringLiteral("event"),
                true, (*rightRow)++, QColor(QStringLiteral("#3498db")));
    }
}

void BindingGraphView::addDataNodePorts(const DataVariable &variable,
                                        QGraphicsItem *nodeItem,
                                        int *leftRow,
                                        int *rightRow)
{
    const QString nodeId = dataNodeId(variable.id);
    const QString valueType = variable.type.isEmpty() ? QStringLiteral("any") : variable.type;
    const QStringList setters = dataSettersForType(variable.type);

    for (const QString &setter : setters)
        addPort(nodeItem, nodeId, QStringLiteral("data_set"), setter, setter,
                valueType, false, (*leftRow)++, QColor(QStringLiteral("#e67e22")));

    const QStringList events = dataEventsForType(variable.type);

    for (const QString &event : events)
        addPort(nodeItem, nodeId, QStringLiteral("data_get"), event, event,
                valueType, true, (*rightRow)++, QColor(QStringLiteral("#8e44ad")));
}

void BindingGraphView::addPort(QGraphicsItem *nodeItem,
                               const QString &nodeId,
                               const QString &kind,
                               const QString &name,
                               const QString &label,
                               const QString &valueType,
                               bool output,
                               int row,
                               const QColor &color)
{
    const qreal y = kHeaderHeight + kFirstPortTop + row * kPortRowHeight;
    const qreal x = output ? kNodeWidth - 14.0 : 14.0;
    const QString key = portKey(nodeId, kind, name);
    auto *dot = new PortItem(this, key, output,
                             QRectF(x - kPortRadius, y - kPortRadius, kPortRadius * 2.0, kPortRadius * 2.0),
                             nodeItem);
    dot->setPen(QPen(color.lighter(130), 1));
    dot->setBrush(color);
    dot->setToolTip(QStringLiteral("%1.%2\n%3: %4")
        .arg(kind, name, tr("类型"), typeDisplayName(valueType)));

    auto *text = new QGraphicsTextItem(label, nodeItem);
    text->setDefaultTextColor(QColor(QStringLiteral("#d8d8d8")));
    text->setTextWidth(kNodeWidth / 2.0 - 28.0);
    text->setPos(output ? kNodeWidth / 2.0 + 6.0 : 24.0, y - 12.0);

    PortVisual visual;
    visual.key = key;
    visual.nodeId = nodeId;
    visual.portKind = kind;
    visual.portName = name;
    visual.label = label;
    visual.valueType = valueType;
    visual.output = output;
    visual.color = color;
    visual.scenePos = dot->mapToScene(dot->boundingRect().center());
    visual.item = dot;
    m_ports.insert(visual.key, visual);
}

void BindingGraphView::rebuildEdges()
{
    const QString edgeToRestore = m_selectedEdgeId;
    QSignalBlocker sceneBlocker(m_scene);
    for (QGraphicsItem *item : std::as_const(m_edgeItems)) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_edgeItems.clear();
    if (!m_project) return;

    for (auto it = m_ports.begin(); it != m_ports.end(); ++it) {
        if (it->item)
            it->scenePos = it->item->mapToScene(it->item->boundingRect().center());
    }

    QHash<QString, int> sourceTotals;
    QHash<QString, int> targetTotals;
    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        const QString sourceKey = portKey(edge.source);
        const QString targetKey = portKey(edge.target);
        if (m_ports.contains(sourceKey) && m_ports.contains(targetKey)) {
            ++sourceTotals[sourceKey];
            ++targetTotals[targetKey];
        }
    }

    QHash<QString, int> sourceIndexes;
    QHash<QString, int> targetIndexes;

    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        const QString sourceKey = portKey(edge.source);
        const QString targetKey = portKey(edge.target);
        const PortVisual source = m_ports.value(sourceKey);
        const PortVisual target = m_ports.value(targetKey);
        if (!source.item || !target.item) continue;

        const int sourceIndex = sourceIndexes.value(sourceKey, 0);
        const int targetIndex = targetIndexes.value(targetKey, 0);
        ++sourceIndexes[sourceKey];
        ++targetIndexes[targetKey];
        const QPointF sourcePoint = portConnectionPoint(source.scenePos, true, sourceIndex, sourceTotals.value(sourceKey, 1));
        const QPointF targetPoint = portConnectionPoint(target.scenePos, false, targetIndex, targetTotals.value(targetKey, 1));

        auto *item = new EdgeItem(this, edge.id, connectionPath(sourcePoint, targetPoint));
        QPen pen(colorForEdgeType(edge.type), edge.enabled ? 2.4 : 1.6);
        if (edge.type == QLatin1String("event_data")) pen.setStyle(Qt::DashLine);
        if (edge.type == QLatin1String("data_action")) pen.setStyle(Qt::DotLine);
        if (edge.type == QLatin1String("property_binding")) pen.setStyle(Qt::DashDotLine);
        if (!edge.enabled) pen.setColor(QColor(QStringLiteral("#686868")));
        item->setPen(pen);
        item->setZValue(3);
        item->setToolTip(edge.label.isEmpty()
            ? QStringLiteral("%1 -> %2").arg(edge.source.portName, edge.target.portName)
            : edge.label);
        m_scene->addItem(item);
        m_edgeItems.insert(edge.id, item);
    }

    m_selectedEdgeId = edgeToRestore;
    if (!m_selectedEdgeId.isEmpty()) {
        if (QGraphicsItem *selected = m_edgeItems.value(m_selectedEdgeId, nullptr))
            selected->setSelected(true);
    }
}

void BindingGraphView::updateStatus()
{
    if (!m_statusLabel) return;
    if (!m_project) {
        m_statusLabel->setText(tr("未打开工程"));
        return;
    }
    m_statusLabel->setText(tr("节点 %1 · 连线 %2")
        .arg(m_project->bindingGraph.nodes.size())
        .arg(m_project->bindingGraph.edges.size()));
}

void BindingGraphView::onSceneSelectionChanged()
{
    QString selectedEdgeId;
    QString selectedNodeId;
    if (m_scene) {
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            selectedEdgeId = edgeIdForItem(item);
            if (!selectedEdgeId.isEmpty())
                break;
            if (auto *nodeItem = dynamic_cast<BindingNodeItem *>(item))
                selectedNodeId = nodeItem->nodeId();
        }
    }
    if (!selectedEdgeId.isEmpty())
        selectedNodeId.clear();
    if (m_selectedEdgeId == selectedEdgeId) {
        emit selectedNodeChanged(selectedNodeId);
        return;
    }
    m_selectedEdgeId = selectedEdgeId;
    emit selectedEdgeChanged(m_selectedEdgeId);
    emit selectedNodeChanged(selectedNodeId);
}

void BindingGraphView::updatePortHighlights(const QString &hoverKey)
{
    if (m_dragSourceKey.isEmpty() || !m_ports.contains(m_dragSourceKey)) {
        resetPortHighlights();
        return;
    }

    const PortVisual source = m_ports.value(m_dragSourceKey);
    for (const PortVisual &port : std::as_const(m_ports)) {
        if (port.key == source.key) {
            setPortHighlighted(port, true, false);
            continue;
        }
        if (port.output) {
            setPortHighlighted(port, false, false);
            continue;
        }
        const bool compatible = isCompatibleConnection(source, port);
        setPortHighlighted(port, compatible, port.key == hoverKey);
    }
}

void BindingGraphView::resetPortHighlights()
{
    for (const PortVisual &port : std::as_const(m_ports))
        setPortHighlighted(port, false, false);
}

void BindingGraphView::setPortHighlighted(const PortVisual &port, bool compatible, bool hover)
{
    auto *ellipse = dynamic_cast<QGraphicsEllipseItem *>(port.item);
    if (!ellipse) return;

    QString tooltip = QStringLiteral("%1.%2\n%3: %4")
        .arg(port.portKind, port.portName, tr("类型"), typeDisplayName(port.valueType));
    if (!m_dragSourceKey.isEmpty() && m_ports.contains(m_dragSourceKey) && !port.output)
        tooltip += QStringLiteral("\n%1").arg(connectionCompatibility(m_ports.value(m_dragSourceKey), port).message);
    ellipse->setToolTip(tooltip);

    if (hover && compatible) {
        ellipse->setPen(QPen(QColor(QStringLiteral("#f1c40f")), 3.0));
        ellipse->setBrush(QColor(QStringLiteral("#f1c40f")));
    } else if (compatible) {
        ellipse->setPen(QPen(port.color.lighter(170), 2.4));
        ellipse->setBrush(port.color.lighter(130));
    } else if (!port.output && !m_dragSourceKey.isEmpty()) {
        ellipse->setPen(QPen(QColor(QStringLiteral("#555555")), 1.0));
        ellipse->setBrush(QColor(QStringLiteral("#3a3a3a")));
    } else {
        ellipse->setPen(QPen(port.color.lighter(130), 1.0));
        ellipse->setBrush(port.color);
    }
}

QString BindingGraphView::portAt(const QPointF &scenePos) const
{
    QString bestKey;
    qreal bestDistance = kPortHitRadius;
    for (const PortVisual &port : m_ports) {
        if (!port.item) continue;
        const QPointF center = port.item->mapToScene(port.item->boundingRect().center());
        const qreal distance = QLineF(center, scenePos).length();
        if (distance <= bestDistance) {
            bestDistance = distance;
            bestKey = port.key;
        }
    }
    return bestKey;
}

bool BindingGraphView::isCompatibleConnection(const PortVisual &source, const PortVisual &target) const
{
    return connectionCompatibility(source, target).ok;
}

BindingGraphView::CompatibilityResult BindingGraphView::connectionCompatibility(const PortVisual &source, const PortVisual &target) const
{
    CompatibilityResult result;
    if (source.key.isEmpty() || target.key.isEmpty()) {
        result.message = tr("端口不存在");
        return result;
    }
    if (source.key == target.key) {
        result.message = tr("不能连接到同一个端口");
        return result;
    }
    if (!source.output) {
        result.message = tr("源端口必须是输出端口");
        return result;
    }
    if (target.output) {
        result.message = tr("目标端口必须是输入端口");
        return result;
    }

    const bool sourceOk = source.portKind == QLatin1String("event")
        || source.portKind == QLatin1String("data_get");
    const bool targetOk = target.portKind == QLatin1String("action")
        || target.portKind == QLatin1String("data_set")
        || target.portKind == QLatin1String("property");
    if (!sourceOk || !targetOk) {
        result.message = tr("端口方向不兼容：%1 -> %2").arg(source.portKind, target.portKind);
        return result;
    }

    if (target.portKind == QLatin1String("property") && source.portKind != QLatin1String("data_get")) {
        result.message = tr("属性绑定需要数据输出端口，当前源端口是 %1").arg(source.portKind);
        return result;
    }

    if (source.portKind == QLatin1String("event")) {
        result.ok = true;
        result.message = tr("兼容：事件可触发目标动作或写入目标");
        return result;
    }

    if (valueTypesCompatible(source.valueType, target.valueType)) {
        result.ok = true;
        result.message = tr("兼容：%1 -> %2")
            .arg(typeDisplayName(source.valueType), typeDisplayName(target.valueType));
        return result;
    }

    result.message = tr("类型不兼容：源为 %1，目标需要 %2")
        .arg(typeDisplayName(source.valueType), typeDisplayName(target.valueType));
    return result;
}

BindingEndpoint BindingGraphView::endpointFromPort(const PortVisual &port) const
{
    BindingEndpoint endpoint;
    endpoint.nodeId = port.nodeId;
    endpoint.portKind = port.portKind;
    endpoint.portName = port.portName;
    endpoint.valueType = port.valueType;

    if (BindingNode *node = findNode(port.nodeId)) {
        endpoint.nodeType = node->type;
        endpoint.refId = node->refId;
        endpoint.refName = node->refName;
    }
    return endpoint;
}

QString BindingGraphView::edgeTypeForPorts(const PortVisual &source, const PortVisual &target) const
{
    if (target.portKind == QLatin1String("property"))
        return QStringLiteral("property_binding");
    if (target.portKind == QLatin1String("data_set"))
        return QStringLiteral("event_data");
    if (source.portKind == QLatin1String("data_get"))
        return QStringLiteral("data_action");
    return QStringLiteral("event_action");
}

QString BindingGraphView::edgeLabelForPorts(const PortVisual &source, const PortVisual &target) const
{
    const BindingEndpoint sourceEndpoint = endpointFromPort(source);
    const BindingEndpoint targetEndpoint = endpointFromPort(target);
    const QString sourceName = sourceEndpoint.refName.isEmpty() ? source.nodeId : sourceEndpoint.refName;
    const QString targetName = targetEndpoint.refName.isEmpty() ? target.nodeId : targetEndpoint.refName;
    return QStringLiteral("%1.%2 -> %3.%4")
        .arg(sourceName, source.portName, targetName, target.portName);
}

bool BindingGraphView::edgeExists(const QString &sourceKey, const QString &targetKey) const
{
    if (!m_project) return false;
    for (const BindingEdge &edge : m_project->bindingGraph.edges) {
        if (portKey(edge.source) == sourceKey && portKey(edge.target) == targetKey)
            return true;
    }
    return false;
}

int BindingGraphView::edgeIndex(const QString &edgeId) const
{
    if (!m_project) return -1;
    for (int i = 0; i < m_project->bindingGraph.edges.size(); ++i) {
        if (m_project->bindingGraph.edges.at(i).id == edgeId)
            return i;
    }
    return -1;
}

QString BindingGraphView::edgeIdForItem(QGraphicsItem *item) const
{
    if (!item) return QString();
    if (auto *edgeItem = dynamic_cast<EdgeItem *>(item))
        return edgeItem->edgeId();
    for (auto it = m_edgeItems.constBegin(); it != m_edgeItems.constEnd(); ++it) {
        if (it.value() == item)
            return it.key();
    }
    return QString();
}

bool BindingGraphView::createEdge(const QString &sourceKey, const QString &targetKey)
{
    if (!m_project || sourceKey.isEmpty() || targetKey.isEmpty()) return false;
    if (!m_ports.contains(sourceKey) || !m_ports.contains(targetKey)) return false;

    const PortVisual source = m_ports.value(sourceKey);
    const PortVisual target = m_ports.value(targetKey);
    const CompatibilityResult compatibility = connectionCompatibility(source, target);
    if (!compatibility.ok) {
        emit statusMessageRequested(tr("绑定失败：%1").arg(compatibility.message));
        return false;
    }
    if (edgeExists(sourceKey, targetKey)) {
        emit statusMessageRequested(tr("绑定已存在"));
        return false;
    }

    BindingEdge edge;
    edge.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    edge.type = edgeTypeForPorts(source, target);
    edge.source = endpointFromPort(source);
    edge.target = endpointFromPort(target);
    edge.label = edgeLabelForPorts(source, target);
    edge.executionMode = QStringLiteral("sequence");
    edge.order = 0;
    for (const BindingEdge &existing : std::as_const(m_project->bindingGraph.edges)) {
        if (portKey(existing.source) == sourceKey)
            edge.order = qMax(edge.order, existing.order + 1);
    }
    edge.enabled = true;
    if (m_undoStack)
        m_undoStack->push(new AddBindingEdgeCommand(this, edge));
    else
        cmdAddEdge(edge);
    emit statusMessageRequested(tr("已创建绑定：%1").arg(edge.label));
    return true;
}


bool BindingGraphView::addNodeFromMimeData(const QMimeData *mimeData, const QPointF &scenePos)
{
    if (!m_project || !m_undoStack || !mimeData || !mimeData->hasFormat(QLatin1String(kBindingGraphNodeMime)))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(mimeData->data(QLatin1String(kBindingGraphNodeMime)));
    if (!doc.isObject())
        return false;

    const QJsonObject payload = doc.object();
    const QString type = payload.value(QStringLiteral("nodeType")).toString();
    const QString refId = payload.value(QStringLiteral("refId")).toString();
    if (type.isEmpty() || refId.isEmpty())
        return false;

    const QString nodeId = type == QLatin1String("widget")
        ? widgetNodeId(refId)
        : (type == QLatin1String("data") ? dataNodeId(refId) : QString());
    if (nodeId.isEmpty())
        return false;

    const QPoint cell = gridCellFromScenePos(scenePos);
    if (!isAllowedDropCell(cell, nodeId))
        return false;

    BindingNode node;
    node.id = nodeId;
    node.type = type;
    node.refId = refId;
    node.refName = payload.value(QStringLiteral("refName")).toString();
    node.title = payload.value(QStringLiteral("title")).toString(node.refName);

    if (type == QLatin1String("widget")) {
        node.visualState.insert(QStringLiteral("widgetId"), payload.value(QStringLiteral("widgetId")).toString());
    } else if (type == QLatin1String("data")) {
        node.visualState.insert(QStringLiteral("valueType"), payload.value(QStringLiteral("valueType")).toString());
    } else {
        return false;
    }

    if (node.title.isEmpty())
        node.title = node.refId;

    if (BindingNode *existing = findNode(node.id)) {
        BindingNode after = *existing;
        after.refName = node.refName;
        after.title = node.title;
        if (type == QLatin1String("widget"))
            after.visualState.insert(QStringLiteral("widgetId"), node.visualState.value(QStringLiteral("widgetId")));
        else if (type == QLatin1String("data"))
            after.visualState.insert(QStringLiteral("valueType"), node.visualState.value(QStringLiteral("valueType")));
        applyGridPlacement(after, cell);
        m_undoStack->push(new UpdateBindingNodeCommand(this, *existing, after));
    } else {
        applyGridPlacement(node, cell);
        m_undoStack->push(new AddBindingNodeCommand(this, node));
    }

    emit statusMessageRequested(tr("已添加绑定节点：%1").arg(node.title));
    return true;
}

int BindingGraphView::nodeGridRow(const BindingNode &node) const
{
    return qMax(0, node.visualState.value(QStringLiteral("gridRow"), 0).toInt());
}

int BindingGraphView::nodeGridColumn(const BindingNode &node) const
{
    return qMax(0, node.visualState.value(QStringLiteral("gridCol"), 0).toInt());
}

qreal BindingGraphView::nodeVisualHeight(const BindingNode &node) const
{
    int inputRows = 0;
    int propertyRows = 0;
    int outputRows = 0;

    if (node.type == QLatin1String("data")) {
        const DataVariable *variable = findDataVariable(node.refId);
        const QString type = variable ? variable->type : node.visualState.value(QStringLiteral("valueType")).toString();
        inputRows = dataSettersForType(type).size();
        outputRows = dataEventsForType(type).size();
    } else {
        const WidgetInstance *instance = findWidgetInstance(node.refId);
        const QString widgetId = instance ? instance->widgetId : node.visualState.value(QStringLiteral("widgetId")).toString();
        const WidgetMeta *meta = findWidgetMeta(widgetId);
        if (meta) {
            inputRows = meta->actions.size();
            propertyRows = bindablePropertyCount(meta);
            outputRows = meta->eventDefs.size();
        }
    }

    if (outputRows == 0)
        outputRows = 1;
    const bool showInputSection = inputRows > 0;
    const bool showPropertySection = propertyRows > 0;
    const int propertyTitleRows = (showInputSection && showPropertySection) ? 1 : 0;
    const int rows = qMax(inputRows + propertyTitleRows + propertyRows, outputRows);
    return qMax<qreal>(122.0, kHeaderHeight + kFirstPortTop + rows * kPortRowHeight + kNodePadding);
}

QHash<int, qreal> BindingGraphView::gridRowHeights() const
{
    QHash<int, qreal> rowHeights;
    if (!m_project)
        return rowHeights;

    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        const int row = nodeGridRow(node);
        const qreal height = qMax(kGridMinRowHeight, nodeVisualHeight(node) + kGridNodeMarginY * 2.0);
        rowHeights.insert(row, qMax(rowHeights.value(row, kGridMinRowHeight), height));
    }
    return rowHeights;
}

QRectF BindingGraphView::gridCellRect(int row, int column, const QHash<int, qreal> &rowHeights) const
{
    qreal y = kGridOriginY;
    for (int i = 0; i < row; ++i)
        y += rowHeights.value(i, kGridMinRowHeight);
    return QRectF(kGridOriginX + column * kGridColumnWidth,
                  y,
                  kGridColumnWidth,
                  rowHeights.value(row, kGridMinRowHeight));
}

QRectF BindingGraphView::gridCellRect(int row, int column) const
{
    return gridCellRect(row, column, gridRowHeights());
}

QPoint BindingGraphView::gridCellFromScenePos(const QPointF &scenePos) const
{
    if (scenePos.x() < kGridOriginX || scenePos.y() < kGridOriginY)
        return QPoint(-1, -1);

    const int column = qFloor((scenePos.x() - kGridOriginX) / kGridColumnWidth);
    const QHash<int, qreal> rowHeights = gridRowHeights();
    qreal y = kGridOriginY;
    const int maxRows = qMax(kGridMinRows, rowHeights.isEmpty() ? 1 : (*std::max_element(rowHeights.keyBegin(), rowHeights.keyEnd()) + 2));
    for (int row = 0; row <= maxRows; ++row) {
        const qreal height = rowHeights.value(row, kGridMinRowHeight);
        if (scenePos.y() >= y && scenePos.y() < y + height)
            return QPoint(column, row);
        y += height;
    }
    return QPoint(column, qMax(0, maxRows));
}

BindingNode *BindingGraphView::nodeAtGridCell(int row, int column, const QString &ignoreNodeId) const
{
    if (!m_project)
        return nullptr;
    for (BindingNode &node : m_project->bindingGraph.nodes) {
        if (!ignoreNodeId.isEmpty() && node.id == ignoreNodeId)
            continue;
        if (nodeGridRow(node) == row && nodeGridColumn(node) == column)
            return &node;
    }
    return nullptr;
}

QVector<QPoint> BindingGraphView::allowedDropCells(const QString &ignoreNodeId) const
{
    QVector<QPoint> cells;
    if (!m_project)
        return cells;

    const bool effectivelyEmpty = std::none_of(m_project->bindingGraph.nodes.cbegin(),
                                               m_project->bindingGraph.nodes.cend(),
                                               [&](const BindingNode &node) { return node.id != ignoreNodeId; });
    if (effectivelyEmpty) {
        cells.append(QPoint(0, 0));
        return cells;
    }

    auto addCell = [&](int row, int column) {
        if (row < 0 || column < 0)
            return;
        const QPoint cell(column, row);
        if (!cells.contains(cell) && !nodeAtGridCell(row, column, ignoreNodeId))
            cells.append(cell);
    };

    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        if (node.id == ignoreNodeId)
            continue;
        const int row = nodeGridRow(node);
        const int column = nodeGridColumn(node);
        const bool entry = node.visualState.value(QStringLiteral("entry"), false).toBool();
        addCell(row, column + 1);
        if (!entry)
            addCell(row + 1, column);
    }
    return cells;
}

bool BindingGraphView::isAllowedDropCell(const QPoint &cell, const QString &ignoreNodeId) const
{
    if (cell.x() < 0 || cell.y() < 0)
        return false;
    return allowedDropCells(ignoreNodeId).contains(cell);
}

void BindingGraphView::applyGridPlacement(BindingNode &node, const QPoint &cell)
{
    const QHash<int, qreal> rowHeights = gridRowHeights();
    const QRectF rect = gridCellRect(cell.y(), cell.x(), rowHeights);
    node.visualState.insert(QStringLiteral("gridRow"), cell.y());
    node.visualState.insert(QStringLiteral("gridCol"), cell.x());
    node.visualState.insert(QStringLiteral("entry"), cell == QPoint(0, 0));

    QString previousNodeId;
    if (cell == QPoint(0, 0)) {
        previousNodeId.clear();
    } else if (BindingNode *left = nodeAtGridCell(cell.y(), cell.x() - 1, node.id)) {
        previousNodeId = left->id;
    } else if (BindingNode *entry = nodeAtGridCell(0, 0, node.id)) {
        previousNodeId = entry->id;
    }
    node.visualState.insert(QStringLiteral("prevNodeId"), previousNodeId);
    node.visualState.insert(QStringLiteral("branchRow"), cell.y());
    node.x = qRound(rect.left() + kGridNodeMarginX);
    node.y = qRound(rect.top() + kGridNodeMarginY);
}

void BindingGraphView::drawGrid(const QHash<int, qreal> &rowHeights, int rows, int columns)
{
    QPen gridPen(QColor(255, 255, 255, 58), 1.0, Qt::DashLine);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            auto *cell = new QGraphicsRectItem(gridCellRect(row, column, rowHeights));
            cell->setPen(gridPen);
            cell->setBrush(Qt::NoBrush);
            cell->setZValue(-20.0);
            m_scene->addItem(cell);
        }
    }
}

void BindingGraphView::updateDropHighlights(const QPointF &scenePos)
{
    if (!m_scene)
        return;

    clearDropHighlights();
    const QVector<QPoint> cells = allowedDropCells();
    m_hoverDropCell = gridCellFromScenePos(scenePos);

    for (const QPoint &cell : cells) {
        QRectF rect = gridCellRect(cell.y(), cell.x()).adjusted(5, 5, -5, -5);
        auto *highlight = new QGraphicsRectItem(rect);
        const bool hover = cell == m_hoverDropCell;
        highlight->setPen(QPen(hover ? QColor(QStringLiteral("#4AA3FF")) : QColor(QStringLiteral("#27AE60")), hover ? 2.0 : 1.4));
        highlight->setBrush(hover ? QColor(74, 163, 255, 72) : QColor(39, 174, 96, 48));
        highlight->setZValue(-5.0);
        m_scene->addItem(highlight);
        m_dropHighlightItems.append(highlight);
    }
}

void BindingGraphView::clearDropHighlights()
{
    if (m_scene) {
        for (QGraphicsItem *item : std::as_const(m_dropHighlightItems))
            m_scene->removeItem(item);
    }
    qDeleteAll(m_dropHighlightItems);
    m_dropHighlightItems.clear();
    m_hoverDropCell = QPoint(-1, -1);
}

QPainterPath BindingGraphView::connectionPath(const QPointF &source, const QPointF &target) const
{
    QPainterPath path(source);
    const qreal dx = qMax<qreal>(80.0, qAbs(target.x() - source.x()) * 0.45);
    path.cubicTo(source + QPointF(dx, 0), target - QPointF(dx, 0), target);
    return path;
}

const WidgetMeta *BindingGraphView::findWidgetMeta(const QString &widgetId) const
{
    for (const WidgetMeta &meta : m_metas) {
        if (meta.id == widgetId || meta.type == widgetId)
            return &meta;
    }
    return nullptr;
}

const WidgetInstance *BindingGraphView::findWidgetInstance(const QString &instanceId) const
{
    if (!m_project) return nullptr;
    for (const ScreenData &screen : m_project->screens) {
        for (const WidgetInstance &instance : screen.widgets) {
            if (instance.instanceId == instanceId)
                return &instance;
        }
    }
    return nullptr;
}

const DataVariable *BindingGraphView::findDataVariable(const QString &id) const
{
    if (!m_project) return nullptr;
    for (const DataVariable &variable : m_project->dataVariables) {
        if (variable.id == id)
            return &variable;
    }
    return nullptr;
}

BindingNode *BindingGraphView::findNode(const QString &nodeId) const
{
    if (!m_project) return nullptr;
    for (BindingNode &node : m_project->bindingGraph.nodes) {
        if (node.id == nodeId)
            return &node;
    }
    return nullptr;
}

QPointF BindingGraphView::defaultNodePosition(int index, const QString &type) const
{
    const bool isData = type == QLatin1String("data");
    const int column = isData ? 1 : 0;
    const int row = index;
    return QPointF(80 + column * 360, 80 + row * 210);
}

QString BindingGraphView::portKey(const BindingEndpoint &endpoint) const
{
    return portKey(endpoint.nodeId, endpoint.portKind, endpoint.portName);
}

QString BindingGraphView::portKey(const QString &nodeId, const QString &kind, const QString &name) const
{
    return QStringLiteral("%1|%2|%3").arg(nodeId, kind, name);
}