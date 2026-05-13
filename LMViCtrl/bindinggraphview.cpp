#include "bindinggraphview.h"

#include <QBrush>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QStyleOptionGraphicsItem>
#include <QVBoxLayout>

namespace {
constexpr qreal kNodeWidth = 240.0;
constexpr qreal kHeaderHeight = 34.0;
constexpr qreal kPortRowHeight = 22.0;
constexpr qreal kPortRadius = 5.0;
constexpr qreal kNodePadding = 10.0;

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
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
    {
        if (change == ItemPositionHasChanged && m_owner) {
            m_owner->persistNodePosition(m_nodeId, pos());
            m_owner->updateEdgesForNode(m_nodeId);
        }
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
};
}

BindingGraphView::BindingGraphView(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void BindingGraphView::setProjectData(ProjectData *project)
{
    m_project = project;
    refreshGraph();
}

void BindingGraphView::setWidgetMetas(const QList<WidgetMeta> &metas)
{
    m_metas = metas;
    refreshGraph();
}

void BindingGraphView::refreshGraph()
{
    if (!m_scene) return;
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
    if (!m_project) return;
    int widgetIndex = 0;
    int dataIndex = 0;
    for (BindingNode &node : m_project->bindingGraph.nodes) {
        const QPointF pos = defaultNodePosition(node.type == QLatin1String("data") ? dataIndex++ : widgetIndex++, node.type);
        node.x = qRound(pos.x());
        node.y = qRound(pos.y());
    }
    refreshGraph();
    emit graphChanged();
    emit statusMessageRequested(tr("绑定图已重新布局"));
}

void BindingGraphView::persistNodePosition(const QString &nodeId, const QPointF &pos)
{
    BindingNode *node = findNode(nodeId);
    if (!node) return;
    const int x = qRound(pos.x());
    const int y = qRound(pos.y());
    if (node->x == x && node->y == y) return;
    node->x = x;
    node->y = y;
    emit graphChanged();
}

void BindingGraphView::updateEdgesForNode(const QString &nodeId)
{
    Q_UNUSED(nodeId)
    rebuildEdges();
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
    root->addWidget(m_view, 1);

    connect(refreshButton, &QPushButton::clicked, this, &BindingGraphView::refreshGraph);
    connect(layoutButton, &QPushButton::clicked, this, &BindingGraphView::autoLayout);
    connect(fitButton, &QPushButton::clicked, this, &BindingGraphView::fitToView);
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
                    false, (*leftRow)++, QColor(QStringLiteral("#27ae60")));
        for (const PropertyMeta &property : meta->properties) {
            if (!property.bindable) continue;
            addPort(nodeItem, nodeId, QStringLiteral("property"), property.name,
                    property.label.isEmpty() ? property.name : property.label,
                    false, (*leftRow)++, QColor(QStringLiteral("#95a5a6")));
        }
        for (const EventDef &event : meta->eventDefs)
            addPort(nodeItem, nodeId, QStringLiteral("event"), event.name,
                    event.label.isEmpty() ? event.name : event.label,
                    true, (*rightRow)++, QColor(QStringLiteral("#3498db")));
    }

    if (*leftRow == 0) {
        addPort(nodeItem, nodeId, QStringLiteral("action"), QStringLiteral("set_property"),
                tr("设置属性"), false, (*leftRow)++, QColor(QStringLiteral("#27ae60")));
    }
    if (*rightRow == 0) {
        addPort(nodeItem, nodeId, QStringLiteral("event"), QStringLiteral("clicked"),
                tr("点击"), true, (*rightRow)++, QColor(QStringLiteral("#3498db")));
    }
}

void BindingGraphView::addDataNodePorts(const DataVariable &variable,
                                        QGraphicsItem *nodeItem,
                                        int *leftRow,
                                        int *rightRow)
{
    const QString nodeId = dataNodeId(variable.id);
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
                false, (*leftRow)++, QColor(QStringLiteral("#e67e22")));

    QStringList events = { QStringLiteral("onChange") };
    if (variable.type == QLatin1String("number"))
        events << QStringLiteral("onAboveLimit") << QStringLiteral("onBelowLimit") << QStringLiteral("onEquals");
    else if (variable.type == QLatin1String("boolean"))
        events << QStringLiteral("onTrue") << QStringLiteral("onFalse");
    else if (variable.type == QLatin1String("list"))
        events << QStringLiteral("onEmpty");

    for (const QString &event : events)
        addPort(nodeItem, nodeId, QStringLiteral("data_get"), event, event,
                true, (*rightRow)++, QColor(QStringLiteral("#8e44ad")));
}

void BindingGraphView::addPort(QGraphicsItem *nodeItem,
                               const QString &nodeId,
                               const QString &kind,
                               const QString &name,
                               const QString &label,
                               bool output,
                               int row,
                               const QColor &color)
{
    const qreal y = kHeaderHeight + 28.0 + row * kPortRowHeight;
    const qreal x = output ? kNodeWidth - 14.0 : 14.0;
    auto *dot = new QGraphicsEllipseItem(x - kPortRadius, y - kPortRadius, kPortRadius * 2.0, kPortRadius * 2.0, nodeItem);
    dot->setPen(QPen(color.lighter(130), 1));
    dot->setBrush(color);
    dot->setToolTip(QStringLiteral("%1.%2").arg(kind, name));

    auto *text = new QGraphicsTextItem(label, nodeItem);
    text->setDefaultTextColor(QColor(QStringLiteral("#d8d8d8")));
    text->setTextWidth(kNodeWidth / 2.0 - 28.0);
    text->setPos(output ? kNodeWidth / 2.0 + 6.0 : 24.0, y - 12.0);

    PortVisual visual;
    visual.key = portKey(nodeId, kind, name);
    visual.nodeId = nodeId;
    visual.portKind = kind;
    visual.portName = name;
    visual.scenePos = dot->mapToScene(dot->boundingRect().center());
    visual.item = dot;
    m_ports.insert(visual.key, visual);
}

void BindingGraphView::rebuildEdges()
{
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

        const QPointF a = source.scenePos;
        const QPointF b = target.scenePos;
        QPainterPath path(a);
        const qreal dx = qMax<qreal>(80.0, qAbs(b.x() - a.x()) * 0.45);
        path.cubicTo(a + QPointF(dx, 0), b - QPointF(dx, 0), b);

        auto *item = new QGraphicsPathItem(path);
        QPen pen(colorForEdgeType(edge.type), edge.enabled ? 2.4 : 1.6);
        if (edge.type == QLatin1String("event_data")) pen.setStyle(Qt::DashLine);
        if (edge.type == QLatin1String("data_action")) pen.setStyle(Qt::DotLine);
        if (edge.type == QLatin1String("property_binding")) pen.setStyle(Qt::DashDotLine);
        if (!edge.enabled) pen.setColor(QColor(QStringLiteral("#686868")));
        item->setPen(pen);
        item->setZValue(-10);
        item->setToolTip(edge.label.isEmpty()
            ? QStringLiteral("%1 -> %2").arg(edge.source.portName, edge.target.portName)
            : edge.label);
        m_scene->addItem(item);
        m_edgeItems.insert(edge.id, item);
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