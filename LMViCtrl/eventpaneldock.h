#pragma once

#include <QDockWidget>
#include <QPointer>
#include "widgemeta.h"

class CanvasScene;
class QGridLayout;
class QScrollArea;
class QWidget;

class EventPanelDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit EventPanelDock(QWidget *parent = nullptr);

    void setCurrentScene(CanvasScene *scene);
    void setProjectData(const ProjectData *project);

private slots:
    void onSceneSelectionChanged();
    void onInstanceEventsChanged(const QString &instanceId);
    void onInstanceChanged(const QString &instanceId);

private:
    void clearPanel();
    void rebuildPanel();
    void addEventRow(int row, const EventDef &eventDef, const WidgetInstance &inst);
    void addNewTriggerRow(int row, const WidgetInstance &inst);
    QList<EventDef> eventDefsForMeta(const WidgetMeta &meta) const;
    EventDef eventDefForName(const WidgetMeta &meta, const QString &eventName) const;
    QStringList existingEventNames(const WidgetInstance &inst) const;
    QList<EventAction> actionsForEvent(const WidgetInstance &inst, const QString &eventName) const;
    QString executionModeForEvent(const WidgetInstance &inst, const QString &eventName) const;
    QList<WidgetInstance> currentSceneInstances() const;
    QList<WidgetMeta> currentSceneMetas() const;
    void chooseTriggerAndAddAction();
    void addAction(const QString &eventName);
    void editAction(const QString &eventName, const EventAction &action);
    void deleteAction(const QString &eventName, const QString &actionId);
    void setEventExecutionMode(const QString &eventName, const QString &mode);
    void moveAction(const QString &eventName, const QString &actionId, int delta);

    QPointer<CanvasScene> m_scene;
    const ProjectData *m_project = nullptr;
    QString m_currentInstanceId;

    QWidget *m_root = nullptr;
    QScrollArea *m_scroll = nullptr;
    QGridLayout *m_grid = nullptr;
};
