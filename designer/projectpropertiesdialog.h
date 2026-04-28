#pragma once
#include <QDialog>
#include "WidgetMeta.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QTextEdit;

class ProjectPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProjectPropertiesDialog(const ProjectData &project, QWidget *parent = nullptr);

    // 返回用户编辑后的 ProjectData
    ProjectData projectData() const;

private:
    void buildUi();
    void loadData();

    ProjectData m_data;

    // 基本信息
    QLineEdit  *m_nameEdit        = nullptr;
    QTextEdit  *m_descEdit        = nullptr;
    QLineEdit  *m_authorEdit      = nullptr;

    // 目标配置
    QSpinBox   *m_widthSpin       = nullptr;
    QSpinBox   *m_heightSpin      = nullptr;
    QComboBox  *m_colorDepthCombo = nullptr;
    QComboBox  *m_platformCombo   = nullptr;

    // 资源配置
    QLineEdit  *m_imagePathEdit   = nullptr;
    QLineEdit  *m_linuxPathEdit   = nullptr;
};
