#pragma once
#include <QDockWidget>
#include <QList>
#include "WidgetMeta.h"

class QListWidget;
class QLineEdit;
class QLabel;

// 组件工具箱停靠窗口
// 从指定目录加载 Lua widget 脚本，展示为可拖拽列表
class WidgetToolbox : public QDockWidget
{
    Q_OBJECT

public:
    explicit WidgetToolbox(QWidget *parent = nullptr);

    // 从目录加载所有 .lua 脚本中的 __widget_meta
    void loadFromDirectory(const QString &dirPath);

    // 已加载的 WidgetMeta 列表
    const QList<WidgetMeta> &widgetMetas() const { return m_metas; }

private slots:
    void onFilterChanged(const QString &text);

private:
    void buildUi();
    void populateList(const QString &filter = {});

    QListWidget       *m_listWidget = nullptr;
    QLineEdit         *m_searchEdit = nullptr;
    QLabel            *m_countLabel = nullptr;
    QList<WidgetMeta>  m_metas;
};
