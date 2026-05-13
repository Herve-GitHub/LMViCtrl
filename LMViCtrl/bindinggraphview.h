#pragma once

#include <QHash>
#include <QWidget>

#include "widgemeta.h"

class QGraphicsItem;
class QGraphicsScene;
class QGraphicsView;
class QLabel;

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

    void persistNodePosition(const QString &nodeId, const QPointF &pos);
    void updateEdgesForNode(const QString &nodeId);

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
                 bool output,
                 int row,
                 const QColor &color);
    void rebuildEdges();
    void updateStatus();

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
};