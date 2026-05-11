#pragma once

#include <QDockWidget>
#include <QPointer>
#include <QString>

class CanvasScene;
class QLabel;
class QTreeWidget;

class ProjectTreeDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit ProjectTreeDock(QWidget *parent = nullptr);

    void setCurrentScene(CanvasScene *scene);
    void setCurrentScene(CanvasScene *scene, const QString &screenName);

private slots:
    void refreshTree();
    void syncSelectionFromScene();
    void onCurrentTreeItemChanged();

private:
    void buildUi();

    QPointer<CanvasScene> m_scene;
    QString m_screenName;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_countLabel = nullptr;
    bool m_updatingTree = false;
    bool m_syncingSelection = false;
};
