#pragma once
#include <QDockWidget>
#include <QPointer>
#include "WidgetMeta.h"

class CanvasScene;
class QFormLayout;
class QGroupBox;
class QScrollArea;
class QWidget;

// 属性面板停靠窗口
// 第一部分：不可编辑的元数据（id/description/schema_version/version）
// 第二部分：实例属性（name + properties）
class PropertyPanelDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit PropertyPanelDock(QWidget *parent = nullptr);

    // 切换当前关注的画布场景；nullptr 表示无场景（清空面板）
    void setCurrentScene(CanvasScene *scene);

private slots:
    void onSceneSelectionChanged();
    void onInstanceChangedFromScene(const QString &instanceId);

private:
    void clearPanel();
    void buildPanel(const WidgetInstance &inst, const WidgetMeta &meta);
    QWidget *makeEditor(const PropertyMeta &pm, const QVariant &value);

    QPointer<CanvasScene> m_scene;
    QString               m_currentInstanceId;

    // UI
    QWidget     *m_root        = nullptr;
    QScrollArea *m_scroll      = nullptr;
    QGroupBox   *m_metaBox     = nullptr;   // 第一部分
    QGroupBox   *m_propsBox    = nullptr;   // 第二部分
    QFormLayout *m_metaForm    = nullptr;
    QFormLayout *m_propsForm   = nullptr;
};
