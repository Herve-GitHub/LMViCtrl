#pragma once
#include <QDockWidget>
#include <QList>
#include <QPoint>
#include "WidgetMeta.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;

// 图页管理器停靠窗口
// 负责：列表展示、新增、删除、重命名、排序、双击打开图页
class ScreenManagerDock : public QDockWidget
{
    Q_OBJECT
public:
    explicit ScreenManagerDock(QWidget *parent = nullptr);

    // 用工程数据刷新列表（整体替换）
    void setScreens(const QList<ScreenData> &screens);

    // 返回当前列表中所有 ScreenData（order 字段已按列表顺序重写）
    QList<ScreenData> screens() const;

signals:
    // 用户请求打开某图页
    void openScreenRequested(const QString &screenId);
    // 列表发生变化（新增/删除/改名/排序），携带最新列表
    void screensChanged(const QList<ScreenData> &screens);
    // 新增/删除请求（由 MainWindow 通过 Undo 命令执行）
    void addScreenRequested(const QString &name);
    void deleteScreenRequested(const QString &screenId);

public slots:
    void onAddScreen();
    void onDeleteScreen();
    void onItemDoubleClicked(QListWidgetItem *item);
    void onItemChanged(QListWidgetItem *item);
    void onShowContextMenu(const QPoint &pos);
    void onRenameScreen();

private:
    void rebuildOrderField();
    void updateButtonState();

    QListWidget  *m_list       = nullptr;
    QPushButton  *m_addBtn     = nullptr;
    QPushButton  *m_deleteBtn  = nullptr;

    bool m_blockSignals = false;
};
