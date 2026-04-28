#include "screenmanagerdock.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

ScreenManagerDock::ScreenManagerDock(QWidget *parent)
    : QDockWidget(tr("图页管理"), parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    auto *container = new QWidget(this);
    auto *vlay      = new QVBoxLayout(container);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);

    m_list = new QListWidget(container);
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    vlay->addWidget(m_list);

    auto *btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("新增"), container);
    m_deleteBtn = new QPushButton(tr("删除"), container);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_deleteBtn);
    btnRow->addStretch();
    vlay->addLayout(btnRow);

    setWidget(container);

    connect(m_addBtn,    &QPushButton::clicked,
            this, &ScreenManagerDock::onAddScreen);
    connect(m_deleteBtn, &QPushButton::clicked,
            this, &ScreenManagerDock::onDeleteScreen);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, &ScreenManagerDock::onItemDoubleClicked);
    connect(m_list, &QListWidget::itemChanged,
            this, &ScreenManagerDock::onItemChanged);
    // 拖拽排序后触发
    connect(m_list->model(), &QAbstractItemModel::rowsMoved,
            this, [this]() {
                rebuildOrderField();
                emit screensChanged(screens());
            });
}

void ScreenManagerDock::setScreens(const QList<ScreenData> &screenList)
{
    m_blockSignals = true;
    m_list->clear();
    for (const ScreenData &s : screenList) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1. %2").arg(s.order + 1).arg(s.name));
        item->setData(Qt::UserRole,     s.id);
        item->setData(Qt::UserRole + 1, s.name);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_list->addItem(item);
    }
    m_blockSignals = false;
}

QList<ScreenData> ScreenManagerDock::screens() const
{
    // 从列表重建（顺序以列表为准），但只有 id/name/order 由我们维护
    // 调用方负责把 widgets 填回去
    QList<ScreenData> result;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        ScreenData s;
        s.id    = item->data(Qt::UserRole).toString();
        s.name  = item->data(Qt::UserRole + 1).toString();
        s.order = i;
        result.append(s);
    }
    return result;
}

void ScreenManagerDock::onAddScreen()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("新增图页"), tr("图页名称："),
        QLineEdit::Normal,
        QStringLiteral("Screen%1").arg(m_list->count() + 1), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int order  = m_list->count();

    auto *item = new QListWidgetItem(
        QStringLiteral("%1. %2").arg(order + 1).arg(name.trimmed()));
    item->setData(Qt::UserRole,     id);
    item->setData(Qt::UserRole + 1, name.trimmed());
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_list->addItem(item);
    m_list->setCurrentItem(item);

    rebuildOrderField();
    emit screensChanged(screens());
    emit openScreenRequested(id);
}

void ScreenManagerDock::onDeleteScreen()
{
    QListWidgetItem *item = m_list->currentItem();
    if (!item) return;
    if (m_list->count() <= 1) {
        QMessageBox::information(this, tr("提示"), tr("至少保留一个图页。"));
        return;
    }
    const QString id = item->data(Qt::UserRole).toString();
    delete m_list->takeItem(m_list->row(item));
    rebuildOrderField();
    emit screensChanged(screens());
    // 通知主窗口关闭对应 tab（用 openScreenRequested 携带空字符串表示删除）
    emit openScreenRequested(QStringLiteral("__delete__:") + id);
}

void ScreenManagerDock::onItemDoubleClicked(QListWidgetItem *item)
{
    emit openScreenRequested(item->data(Qt::UserRole).toString());
}

void ScreenManagerDock::onItemChanged(QListWidgetItem *item)
{
    if (m_blockSignals) return;
    // 用户直接编辑了条目文本，解析新名称
    const QString text = item->text();
    // 格式为 "N. Name"，取 ". " 后面部分
    const int dotIdx = text.indexOf(QLatin1String(". "));
    const QString newName = (dotIdx >= 0) ? text.mid(dotIdx + 2).trimmed() : text.trimmed();
    item->setData(Qt::UserRole + 1, newName);
    rebuildOrderField();
    emit screensChanged(screens());
}

void ScreenManagerDock::rebuildOrderField()
{
    m_blockSignals = true;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const QString name = item->data(Qt::UserRole + 1).toString();
        item->setText(QStringLiteral("%1. %2").arg(i + 1).arg(name));
    }
    m_blockSignals = false;
}
