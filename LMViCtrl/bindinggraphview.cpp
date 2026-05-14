#include "bindinggraphview.h"

#include <QBrush>
#include <QAction>
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
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStyleOptionGraphicsItem>
#include <QUndoCommand>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QUuid>

namespace {
constexpr qreal kNodeWidth = 240.0;
constexpr qreal kHeaderHeight = 34.0;
constexpr qreal kPortRowHeight = 22.0;
constexpr qreal kPortRadius = 5.0;
constexpr qreal kNodePadding = 10.0;
constexpr qreal kPortHitRadius = 14.0;

QColor colorForNodeType(const QString &type)
{
    if (type == QLatin1String("data")) return QColor(QStringLiteral("#8e44ad"));
    if (type == QLatin1String("screen")) return QColor(QStringLiteral("#607d8b"));
    return QColor(QStringLiteral("#2d6be4"));
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
    return type.isEmpty()
        || type == QLatin1String("any")
        || type == QLatin1String("event")
        || type == QLatin1String("void");
}

bool valueTypesCompatible(const QString &sourceType, const QString &targetType)
{
    if (isWildcardValueType(sourceType) || isWildcardValueType(targetType)) return true;
    return sourceType.compare(targetType, Qt::CaseInsensitive) == 0;
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
        setFlags(QGraphicsItem::ItemIsMovable
                 | QGraphicsItem::ItemIsSelectable
                 | QGraphicsItem::ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override
    {
        m_dragStartPos = pos();
        QGraphicsRectItem::mousePressEvent(event);
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
            painter->setPen(QPen(QColor(QStringLiteral("#ffffff")), 1.5, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(rect().adjusted(2, 2, -2, -2), 6, 6);
        }
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
    } else if (!edgeId.isEmpty()) {
        m_selectedEdgeId = edgeId;
        emit selectedEdgeChanged(edgeId);
    } else if (!m_selectedEdgeId.isEmpty()) {
        m_selectedEdgeId.clear();
        emit selectedEdgeChanged(QString());
    }
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
    const QPointF before(qRound(oldPos.x()), qRound(oldPos.y()));
    const QPointF after(qRound(newPos.x()), qRound(newPos.y()));
    if (before == after) return;
    m_undoStack->push(new MoveBindingNodeCommand(this, nodeId, before, after));
}

void BindingGraphView::updateEdgesForNode(const QString &nodeId)
{
    Q_UNUSED(nodeId)
    rebuildEdges();
}

void BindingGraphView::beginConnectionDrag(const QString &portKey, const QPointF &scenePos)
{
    if (!m_project || !m_ports.contains(portKey)) return;
    const PortVisual source = m_ports.value(portKey);
    if (!source.output) return;

    cancelConnectionDrag();
    m_dragSourceKey = portKey;
    m_dragPathItem = new QGraphicsPathItem(connectionPath(scenePos, scenePos));
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
    m_dragPathItem->setPath(connectionPath(source.scenePos, scenePos));
    updatePortHighlights(portAt(scenePos));
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
    if ((watched == m_view || (m_view && watched == m_view->viewport())) && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
            if (deleteSelectedEdges()) {
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
        "QPushButton { padding:5px 10px; border:1px solid #4a4a4a; background:#333333; color:#dddddd; border-radius:4px; }"
        "QPushButton:hover { background:#3f3f46; }"
        "QLabel { color:#cfcfcf; }"));
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    barLayout->setSpacing(8);

    auto *refreshButton = new QPushButton(tr("刷新"), bar);
    auto *layoutButton = new QPushButton(tr("重新布局"), bar);
    auto *fitButton = new QPushButton(tr("适应窗口"), bar);
    m_statusLabel = new QLabel(bar);

    barLayout->addWidget(refreshButton);
    barLayout->addWidget(layoutButton);
    barLayout->addWidget(fitButton);
    barLayout->addStretch();
    barLayout->addWidget(m_statusLabel);
    root->addWidget(bar);

    m_scene = new QGraphicsScene(this);
    m_scene->setBackgroundBrush(QColor(QStringLiteral("#1e1e1e")));
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    m_view->setDragMode(QGraphicsView::RubberBandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorViewCenter);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->installEventFilter(this);
    m_view->viewport()->installEventFilter(this);
    root->addWidget(m_view, 1);

    connect(refreshButton, &QPushButton::clicked, this, &BindingGraphView::refreshGraph);
    connect(layoutButton, &QPushButton::clicked, this, &BindingGraphView::autoLayout);
    connect(fitButton, &QPushButton::clicked, this, &BindingGraphView::fitToView);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &BindingGraphView::onSceneSelectionChanged);
}

void BindingGraphView::reconcileGraphNodes()
{
    if (!m_project) return;

    QSet<QString> liveIds;
    int nextIndex = m_project->bindingGraph.nodes.size();
    auto ensureNode = [&](const QString &id, const QString &type, const QString &refId,
                          const QString &refName, const QString &title) {
        liveIds.insert(id);
        BindingNode *existing = findNode(id);
        if (existing) {
            existing->type = type;
            existing->refId = refId;
            existing->refName = refName;
            existing->title = title;
            return;
        }
        BindingNode node;
        node.id = id;
        node.type = type;
        node.refId = refId;
        node.refName = refName;
        node.title = title;
        const QPointF pos = defaultNodePosition(nextIndex++, type);
        node.x = qRound(pos.x());
        node.y = qRound(pos.y());
        m_project->bindingGraph.nodes.append(node);
    };

    for (const ScreenData &screen : std::as_const(m_project->screens)) {
        for (const WidgetInstance &instance : screen.widgets) {
            if (instance.isGroup) continue;
            const QString title = instance.name.isEmpty() ? instance.widgetId : instance.name;
            ensureNode(widgetNodeId(instance.instanceId), QStringLiteral("widget"),
                       instance.instanceId, instance.name, title);
        }
    }

    for (const DataVariable &variable : std::as_const(m_project->dataVariables)) {
        const QString title = variable.name.isEmpty() ? variable.id : variable.name;
        ensureNode(dataNodeId(variable.id), QStringLiteral("data"),
                   variable.id, variable.name, title);
    }

    for (int i = m_project->bindingGraph.nodes.size() - 1; i >= 0; --i) {
        if (!liveIds.contains(m_project->bindingGraph.nodes.at(i).id))
            m_project->bindingGraph.nodes.removeAt(i);
    }

    for (int i = m_project->bindingGraph.edges.size() - 1; i >= 0; --i) {
        const BindingEdge &edge = m_project->bindingGraph.edges.at(i);
        if (!liveIds.contains(edge.source.nodeId) || !liveIds.contains(edge.target.nodeId))
            m_project->bindingGraph.edges.removeAt(i);
    }
}

void BindingGraphView::rebuildScene()
{
    m_scene->clear();
    m_nodeItems.clear();
    m_ports.clear();
    m_edgeItems.clear();
    m_dragPathItem = nullptr;
    m_dragSourceKey.clear();
    if (!m_project) return;

    m_scene->setSceneRect(-600, -400, 2200, 1600);

    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        const bool isData = node.type == QLatin1String("data");
        const QColor accent = colorForNodeType(node.type);
        const QRectF rect(0, 0, kNodeWidth, 168);
        auto *nodeItem = new BindingNodeItem(this, node.id, rect);
        nodeItem->setPos(node.x, node.y);
        nodeItem->setPen(QPen(accent, 1.6));
        nodeItem->setBrush(QColor(QStringLiteral("#252526")));
        m_scene->addItem(nodeItem);
        m_nodeItems.insert(node.id, nodeItem);

        auto *header = new QGraphicsRectItem(0, 0, kNodeWidth, kHeaderHeight, nodeItem);
        header->setPen(Qt::NoPen);
        header->setBrush(accent.darker(135));

        auto *title = new QGraphicsTextItem(node.title.isEmpty() ? node.refName : node.title, nodeItem);
        title->setDefaultTextColor(Qt::white);
        title->setTextWidth(kNodeWidth - 24);
        title->setPos(12, 6);

        auto *kind = new QGraphicsTextItem(isData ? tr("数据变量") : tr("控件节点"), nodeItem);
        kind->setDefaultTextColor(QColor(QStringLiteral("#a8a8a8")));
        kind->setPos(12, kHeaderHeight + 4);

        int leftRow = 0;
        int rightRow = 0;
        if (isData) {
            if (const DataVariable *variable = findDataVariable(node.refId))
                addDataNodePorts(*variable, nodeItem, &leftRow, &rightRow);
        } else {
            const WidgetInstance *instance = findWidgetInstance(node.refId);
            if (instance)
                addWidgetNodePorts(*instance, findWidgetMeta(instance->widgetId), nodeItem, &leftRow, &rightRow);
        }

        const int rows = qMax(leftRow, rightRow);
        const qreal height = qMax<qreal>(122.0, kHeaderHeight + 28.0 + rows * kPortRowHeight + kNodePadding);
        nodeItem->setRect(0, 0, kNodeWidth, height);
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
        for (const PropertyMeta &property : meta->properties) {
            if (!property.bindable) continue;
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

    if (*leftRow == 0) {
        addPort(nodeItem, nodeId, QStringLiteral("action"), QStringLiteral("set_property"),
                tr("设置属性"), QStringLiteral("any"),
                false, (*leftRow)++, QColor(QStringLiteral("#27ae60")));
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
    QStringList setters;
    if (variable.type == QLatin1String("boolean"))
        setters = { QStringLiteral("set_true"), QStringLiteral("set_false"), QStringLiteral("toggle") };
    else if (variable.type == QLatin1String("list"))
        setters = { QStringLiteral("push"), QStringLiteral("pop"), QStringLiteral("set_item"), QStringLiteral("clear") };
    else if (variable.type == QLatin1String("string"))
        setters = { QStringLiteral("set"), QStringLiteral("append"), QStringLiteral("format"), QStringLiteral("clear") };
    else
        setters = { QStringLiteral("set"), QStringLiteral("increment"), QStringLiteral("decrement"), QStringLiteral("reset") };

    for (const QString &setter : setters)
        addPort(nodeItem, nodeId, QStringLiteral("data_set"), setter, setter,
                valueType, false, (*leftRow)++, QColor(QStringLiteral("#e67e22")));

    QStringList events = { QStringLiteral("onChange") };
    if (variable.type == QLatin1String("number"))
        events << QStringLiteral("onAboveLimit") << QStringLiteral("onBelowLimit") << QStringLiteral("onEquals");
    else if (variable.type == QLatin1String("boolean"))
        events << QStringLiteral("onTrue") << QStringLiteral("onFalse");
    else if (variable.type == QLatin1String("list"))
        events << QStringLiteral("onEmpty");

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
    const qreal y = kHeaderHeight + 28.0 + row * kPortRowHeight;
    const qreal x = output ? kNodeWidth - 14.0 : 14.0;
    const QString key = portKey(nodeId, kind, name);
    auto *dot = new PortItem(this, key, output,
                             QRectF(x - kPortRadius, y - kPortRadius, kPortRadius * 2.0, kPortRadius * 2.0),
                             nodeItem);
    dot->setPen(QPen(color.lighter(130), 1));
    dot->setBrush(color);
    dot->setToolTip(QStringLiteral("%1.%2").arg(kind, name));

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

    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        const PortVisual source = m_ports.value(portKey(edge.source));
        const PortVisual target = m_ports.value(portKey(edge.target));
        if (!source.item || !target.item) continue;

        auto *item = new EdgeItem(this, edge.id, connectionPath(source.scenePos, target.scenePos));
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
    if (m_scene) {
        for (QGraphicsItem *item : m_scene->selectedItems()) {
            selectedEdgeId = edgeIdForItem(item);
            if (!selectedEdgeId.isEmpty())
                break;
        }
    }
    if (m_selectedEdgeId == selectedEdgeId) return;
    m_selectedEdgeId = selectedEdgeId;
    emit selectedEdgeChanged(m_selectedEdgeId);
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
    if (source.key.isEmpty() || target.key.isEmpty()) return false;
    if (source.key == target.key) return false;
    if (!source.output || target.output) return false;

    const bool sourceOk = source.portKind == QLatin1String("event")
        || source.portKind == QLatin1String("data_get");
    const bool targetOk = target.portKind == QLatin1String("action")
        || target.portKind == QLatin1String("data_set")
        || target.portKind == QLatin1String("property");
    if (!sourceOk || !targetOk) return false;

    if (source.portKind == QLatin1String("event"))
        return true;
    return valueTypesCompatible(source.valueType, target.valueType);
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
    if (!isCompatibleConnection(source, target)) {
        emit statusMessageRequested(tr("绑定失败：端口类型不兼容"));
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