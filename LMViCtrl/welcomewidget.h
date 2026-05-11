#pragma once

#include <QWidget>
#include <QStringList>

class QListWidget;
class QLabel;

/**
 * @brief 欢迎界面
 *
 * 在未打开任何工程时作为中央区域显示，提供：
 *  - 软件简介
 *  - 新建工程 / 打开工程 快捷入口
 *  - 最近工程列表（双击直接打开）
 */
class WelcomeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WelcomeWidget(QWidget *parent = nullptr);

    /** 刷新最近工程列表 */
    void setRecentProjects(const QStringList &paths);

signals:
    void newProjectRequested();
    void openProjectRequested();
    void openRecentRequested(const QString &path);

private slots:
    void onRecentItemDoubleClicked();

private:
    void buildUi();
    QListWidget *m_recentList = nullptr;
};
