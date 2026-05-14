#pragma once

#include <QHash>
#include <QColor>
#include <QWidget>

#include "widgemeta.h"

class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;
class QPainterPath;
class QUndoStack;

class BindingGraphView : public QWidget
{
    Q_OBJECT

public:
    explicit BindingGraphView(QWidget *parent = nullptr);

    void setProjectData(ProjectData *project);
    void setWidgetMetas(const QList<WidgetMeta> &metas);
    void refreshGraph();
    void fitToView();
    void autoLayout();
    bool hasProject() const { return m_project != nullptr; }
    QUndoStack *undoStack() const { return m_undoStack; }

    void commitNodeMove(const QString &nodeId, const QPointF &oldPos, const QPointF &newPos);
    void updateEdgesForNode(const QString &nodeId);
    void beginConnectionDrag(const QString &portKey, const QPointF &scenePos);
    void updateConnectionDrag(const QPointF &scenePos);
    void finishConnectionDrag(const QPointF &scenePos);
    void cancelConnectionDrag();
    void cmdAddEdge(const BindingEdge &edge);
    void cmdRemoveEdge(const QString &edgeId);
    void cmdSetNodePosition(const QString &nodeId, const QPointF &pos);
    void cmdSetNodePositions(const QHash<QString, QPointF> &positions);

signals:
    void graphChanged();
    void statusMessageRequested(const QString &message);

protected:
    void showEvent(QShowEvent *event) override;

private:
    struct PortVisual {
        QString key;
        QString nodeId;
        QString portKind;
        QString portName;
        QString label;
        QString valueType;
        bool output = false;
        QColor color;
        QPointF scenePos;
        QGraphicsItem *item = nullptr;
    };

    void buildUi();
    void reconcileGraphNodes();
    void rebuildScene();
    void addWidgetNodePorts(const WidgetInstance &instance,
                            const WidgetMeta *meta,
                            QGraphicsItem *nodeItem,
                            int *leftRow,
                            int *rightRow);
    void addDataNodePorts(const DataVariable &variable,
                          QGraphicsItem *nodeItem,
                          int *leftRow,
                          int *rightRow);
    void addPort(QGraphicsItem *nodeItem,
                 const QString &nodeId,
                 const QString &kind,
                 const QString &name,
                 const QString &label,
                 const QString &valueType,
                 bool output,
                 int row,
                 const QColor &color);
    void rebuildEdges();
    void updateStatus();
    void updatePortHighlights(const QString &hoverKey = QString());
    void resetPortHighlights();
    void setPortHighlighted(const PortVisual &port, bool compatible, bool hover);
    QString portAt(const QPointF &scenePos) const;
    bool isCompatibleConnection(const PortVisual &source, const PortVisual &target) const;
    BindingEndpoint endpointFromPort(const PortVisual &port) const;
    QString edgeTypeForPorts(const PortVisual &source, const PortVisual &target) const;
    QString edgeLabelForPorts(const PortVisual &source, const PortVisual &target) const;
    bool edgeExists(const QString &sourceKey, const QString &targetKey) const;
    bool createEdge(const QString &sourceKey, const QString &targetKey);
    QPainterPath connectionPath(const QPointF &source, const QPointF &target) const;

    const WidgetMeta *findWidgetMeta(const QString &widgetId) const;
    const WidgetInstance *findWidgetInstance(const QString &instanceId) const;
    const DataVariable *findDataVariable(const QString &id) const;
    BindingNode *findNode(const QString &nodeId) const;
    QPointF defaultNodePosition(int index, const QString &type) const;
    QString portKey(const BindingEndpoint &endpoint) const;
    QString portKey(const QString &nodeId, const QString &kind, const QString &name) const;

    ProjectData *m_project = nullptr;
    QList<WidgetMeta> m_metas;
    QGraphicsView *m_view = nullptr;
    QGraphicsScene *m_scene = nullptr;
    QLabel *m_statusLabel = nullptr;
    QHash<QString, QGraphicsItem *> m_nodeItems;
    QHash<QString, PortVisual> m_ports;
    QHash<QString, QGraphicsItem *> m_edgeItems;
    QUndoStack *m_undoStack = nullptr;
    QString m_dragSourceKey;
    QGraphicsPathItem *m_dragPathItem = nullptr;
};