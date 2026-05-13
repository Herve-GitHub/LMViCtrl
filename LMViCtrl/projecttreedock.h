#pragma once

#include <QDockWidget>
#include <QPoint>
#include <QPointer>
#include <QString>

#include "widgemeta.h"

class CanvasScene;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;

class ProjectTreeDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProjectTreeDock(QWidget *parent = nullptr);

    void setCurrentScene(CanvasScene *scene);
    void setCurrentScene(CanvasScene *scene, const QString &screenName);
    void setProjectData(const ProjectData *project);

signals:
    void addDataVariableRequested(const QString &name, const QString &type);
    void removeDataVariableRequested(const QString &id);

private slots:
    void refreshTree();
    void syncSelectionFromScene();
    void onCurrentTreeItemChanged();
    void showContextMenu(const QPoint &pos);

private:
    void buildUi();
    void requestAddDataVariable();
    void requestRemoveDataVariable(QTreeWidgetItem *item);

    QPointer<CanvasScene> m_scene;
    const ProjectData *m_project = nullptr;
    QString m_screenName;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_countLabel = nullptr;
    bool m_updatingTree = false;
    bool m_syncingSelection = false;
};
