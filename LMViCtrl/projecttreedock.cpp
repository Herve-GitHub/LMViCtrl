#include "projecttreedock.h"
#include "canvasitem.h"
#include "canvasscene.h"

#include <QAbstractItemView>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kInstanceIdRole = Qt::UserRole + 1;
constexpr int kNodeTypeRole = Qt::UserRole + 2;
constexpr int kVariableIdRole = Qt::UserRole + 3;

constexpr const char *kNodeScreen = "screen";
constexpr const char *kNodeWidget = "widget";
constexpr const char *kNodeVariablesRoot = "variablesRoot";
constexpr const char *kNodeVariable = "variable";
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
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #2b2b2b; color: #d8d8d8; border: 1px solid #3a3a3a; }"
        "QTreeWidget::item { padding: 3px 4px; }"
        "QTreeWidget::item:selected { background: #1a6fa0; color: white; }"
    ));
    if (m_tree->header())
        m_tree->header()->setSectionResizeMode(QHeaderView::Stretch);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ProjectTreeDock::onCurrentTreeItemChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ProjectTreeDock::showContextMenu);
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

void ProjectTreeDock::setProjectData(const ProjectData *project)
{
    m_project = project;
    refreshTree();
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
        root->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeScreen));

        const QList<WidgetInstance> instances = m_scene->instancesSortedByZ();
        for (const WidgetInstance &inst : instances) {
            if (inst.isGroup)
                continue;
            const QString displayName = inst.name.trimmed().isEmpty()
                ? inst.widgetId
                : inst.name.trimmed();
            auto *item = new QTreeWidgetItem(root);
            item->setText(0, displayName);
            item->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeWidget));
            item->setData(0, kInstanceIdRole, inst.instanceId);
            item->setToolTip(0, displayName);
            if (inst.instanceId == selectedId)
                m_tree->setCurrentItem(item);
            ++count;
        }
        root->setExpanded(true);
    }

    const QList<DataVariable> variables = m_project ? m_project->dataVariables : QList<DataVariable>{};
    auto *variablesRoot = new QTreeWidgetItem(m_tree);
    variablesRoot->setText(0, tr("数据变量"));
    variablesRoot->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeVariablesRoot));
    for (const DataVariable &variable : variables) {
        const QString type = variable.type.isEmpty() ? QStringLiteral("number") : variable.type;
        const QString label = QStringLiteral("%1  <%2>").arg(variable.name, type);
        auto *item = new QTreeWidgetItem(variablesRoot);
        item->setText(0, label);
        item->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeVariable));
        item->setData(0, kVariableIdRole, variable.id);
        item->setToolTip(0, variable.description.isEmpty() ? label : variable.description);
    }
    variablesRoot->setExpanded(true);

    m_updatingTree = false;

    if (m_countLabel) {
        if (m_scene)
            m_countLabel->setText(tr("%1 个元素 / %2 个变量").arg(count).arg(variables.size()));
        else
            m_countLabel->setText(tr("未激活图页"));
    }
}

void ProjectTreeDock::showContextMenu(const QPoint &pos)
{
    if (!m_tree) return;
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    const QString nodeType = item ? item->data(0, kNodeTypeRole).toString() : QString();

    QMenu menu(this);
    QAction *addVariable = nullptr;
    QAction *removeVariable = nullptr;
    if (!item || nodeType == QLatin1String(kNodeVariablesRoot) || nodeType == QLatin1String(kNodeVariable))
        addVariable = menu.addAction(tr("新增数据变量"));
    if (nodeType == QLatin1String(kNodeVariable))
        removeVariable = menu.addAction(tr("删除数据变量"));

    if (menu.actions().isEmpty()) return;
    QAction *chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == addVariable)
        requestAddDataVariable();
    else if (chosen == removeVariable)
        requestRemoveDataVariable(item);
}

void ProjectTreeDock::requestAddDataVariable()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("新增数据变量"),
                                               tr("变量名"), QLineEdit::Normal,
                                               QStringLiteral("$value"), &ok).trimmed();
    if (!ok || name.isEmpty()) return;

    const QStringList types { QStringLiteral("number"), QStringLiteral("string"), QStringLiteral("boolean"), QStringLiteral("list") };
    const QString type = QInputDialog::getItem(this, tr("变量类型"), tr("类型"),
                                               types, 0, false, &ok);
    if (!ok || type.isEmpty()) return;

    emit addDataVariableRequested(name, type);
}

void ProjectTreeDock::requestRemoveDataVariable(QTreeWidgetItem *item)
{
    if (!item) return;
    const QString id = item->data(0, kVariableIdRole).toString();
    if (!id.isEmpty())
        emit removeDataVariableRequested(id);
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