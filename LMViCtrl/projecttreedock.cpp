#include "projecttreedock.h"
#include "canvasitem.h"
#include "canvasscene.h"

#include <QAbstractItemView>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kInstanceIdRole = Qt::UserRole + 1;
}

ProjectTreeDock::ProjectTreeDock(QWidget *parent)
    : QDockWidget(tr("工程树"), parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(180);
    buildUi();
}

void ProjectTreeDock::buildUi()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_tree = new QTreeWidget(container);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setIndentation(12);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #2b2b2b; color: #d8d8d8; border: 1px solid #3a3a3a; }"
        "QTreeWidget::item { padding: 3px 4px; }"
        "QTreeWidget::item:selected { background: #1a6fa0; color: white; }"
    ));
    if (m_tree->header())
        m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ProjectTreeDock::onCurrentTreeItemChanged);
    layout->addWidget(m_tree, 1);

    m_countLabel = new QLabel(container);
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_countLabel->setStyleSheet(QStringLiteral("color: #707070; font-size: 9pt;"));
    layout->addWidget(m_countLabel);

    setWidget(container);
    refreshTree();
}

void ProjectTreeDock::setCurrentScene(CanvasScene *scene)
{
    setCurrentScene(scene, QString());
}

void ProjectTreeDock::setCurrentScene(CanvasScene *scene, const QString &screenName)
{
    if (m_scene == scene) {
        m_screenName = screenName;
        refreshTree();
        syncSelectionFromScene();
        return;
    }
    if (m_scene)
        disconnect(m_scene, nullptr, this, nullptr);

    m_scene = scene;
    m_screenName = screenName;
    if (m_scene) {
        connect(m_scene.data(), &CanvasScene::sceneItemsChanged,
                this, &ProjectTreeDock::refreshTree);
        connect(m_scene.data(), &CanvasScene::instanceChanged,
                this, &ProjectTreeDock::refreshTree);
        connect(m_scene.data(), &QGraphicsScene::selectionChanged,
                this, &ProjectTreeDock::syncSelectionFromScene);
    }
    refreshTree();
    syncSelectionFromScene();
}

void ProjectTreeDock::refreshTree()
{
    if (!m_tree) return;

    QString selectedId;
    if (m_scene) {
        const auto selected = m_scene->selectedItems();
        if (selected.size() == 1) {
            if (auto *item = qgraphicsitem_cast<CanvasItem *>(selected.first()))
                selectedId = item->instance().instanceId;
        }
    }

    m_updatingTree = true;
    m_tree->clear();

    int count = 0;
    if (m_scene) {
        const QString rootName = m_screenName.trimmed().isEmpty()
            ? tr("图页")
            : m_screenName.trimmed();
        auto *root = new QTreeWidgetItem(m_tree);
        root->setText(0, rootName);

        const QList<WidgetInstance> instances = m_scene->instancesSortedByZ();
        for (const WidgetInstance &inst : instances) {
            if (inst.isGroup)
                continue;
            const QString displayName = inst.name.trimmed().isEmpty()
                ? inst.widgetId
                : inst.name.trimmed();
            auto *item = new QTreeWidgetItem(root);
            item->setText(0, displayName);
            item->setData(0, kInstanceIdRole, inst.instanceId);
            item->setToolTip(0, displayName);
            if (inst.instanceId == selectedId)
                m_tree->setCurrentItem(item);
            ++count;
        }
        root->setExpanded(true);
    }

    m_updatingTree = false;

    if (m_countLabel) {
        if (m_scene)
            m_countLabel->setText(tr("%1 个元素").arg(count));
        else
            m_countLabel->setText(tr("未激活图页"));
    }
}


void ProjectTreeDock::syncSelectionFromScene()
{
    if (!m_tree || !m_scene || m_updatingTree || m_syncingSelection) return;

    QString selectedId;
    const auto selected = m_scene->selectedItems();
    if (selected.size() == 1) {
        if (auto *item = qgraphicsitem_cast<CanvasItem *>(selected.first()))
            selectedId = item->instance().instanceId;
    }

    m_syncingSelection = true;
    m_tree->clearSelection();
    QTreeWidgetItem *root = m_tree->topLevelItem(0);
    const int childCount = root ? root->childCount() : 0;
    for (int i = 0; i < childCount; ++i) {
        QTreeWidgetItem *treeItem = root->child(i);
        if (treeItem->data(0, kInstanceIdRole).toString() == selectedId) {
            m_tree->setCurrentItem(treeItem);
            treeItem->setSelected(true);
            break;
        }
    }
    m_syncingSelection = false;
}

void ProjectTreeDock::onCurrentTreeItemChanged()
{
    if (!m_scene || m_updatingTree || m_syncingSelection) return;

    QTreeWidgetItem *item = m_tree->currentItem();
    const QString instanceId = item ? item->data(0, kInstanceIdRole).toString() : QString();
    if (instanceId.isEmpty()) return;

    if (CanvasItem *canvasItem = m_scene->findItem(instanceId)) {
        m_syncingSelection = true;
        m_scene->clearSelection();
        canvasItem->setSelected(true);
        m_syncingSelection = false;
    }
}