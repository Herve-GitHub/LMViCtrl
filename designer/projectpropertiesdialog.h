#pragma once
#include <QDialog>
#include "widgemeta.h"

class QLineEdit;
class QSpinBox;
class QComboBox;
class QTextEdit;
class QPushButton;

class ProjectPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    /// @param projectDir 工程目录绝对路径，用于把用户选择的字体文件
    /// 拷贝到工程目录的 fonts/ 子目录中。可为空（此时字体浏览按钮被禁用）。
    explicit ProjectPropertiesDialog(const ProjectData &project,
                                     const QString &projectDir,
                                     QWidget *parent = nullptr);

    // 返回用户编辑后的 ProjectData
    ProjectData projectData() const;

private:
    void buildUi();
    void loadData();
    void onBrowseFont();
    void onClearFont();

    ProjectData m_data;
    QString     m_projectDir;

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

    // 字体配置
    QLineEdit   *m_fontFileEdit   = nullptr;
    QSpinBox    *m_fontSizeSpin   = nullptr;
    QPushButton *m_fontBrowseBtn  = nullptr;
    QPushButton *m_fontClearBtn   = nullptr;
};
