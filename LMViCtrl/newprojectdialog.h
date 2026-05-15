#pragma once

#include <QDialog>
#include <QString>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QLineEdit;
class QToolButton;
class QPushButton;
class QStackedWidget;

/**
 * @brief 新建工程向导对话框
 *
 * 三栏布局（仿 Qt Creator）：
 *   左栏  — 工程类别列表
 *   中栏  — 当前类别下的模板列表
 *   右栏  — 选中模板的详情说明
 *
 * 底部输入工程名称与存储路径，点击"创建"返回 Accepted。
 * 调用方通过 projectName() / projectLocation() / templateId() 获取结果。
 */
class NewProjectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewProjectDialog(QWidget *parent = nullptr);

    /** 用户填写的工程名称 */
    QString projectName()     const;
    /** 用户选择的保存目录 */
    QString projectLocation() const;
    /** 选中的模板 ID（如 "hmi", "button", "template", "svg"） */
    QString templateId()      const;
    /** 选中模板的显示名称 */
    QString templateName()    const;

private slots:
    void onCategoryChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onTemplateChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onBrowseLocation();
    void onProjectNameChanged(const QString &text);
    void validate();

private:
    void buildUi();
    void populateCategories();
    void populateTemplates(const QString &categoryId);
    QString uniqueProjectName(const QString &base) const;

    QListWidget   *m_categoryList  = nullptr;
    QListWidget   *m_templateList  = nullptr;
    QLabel        *m_detailIcon    = nullptr;
    QLabel        *m_detailTitle   = nullptr;
    QLabel        *m_detailDesc    = nullptr;
    QLineEdit     *m_nameEdit      = nullptr;
    QLineEdit     *m_locationEdit  = nullptr;
    QToolButton   *m_browseBtn     = nullptr;
    QPushButton   *m_createBtn     = nullptr;
    QLabel        *m_errorLabel    = nullptr;

    QString        m_templateId;
    QString        m_templateName;
};
