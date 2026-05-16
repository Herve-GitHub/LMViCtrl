#include "projecttreedock.h"
#include "canvasitem.h"
#include "canvasscene.h"

#include <QAbstractItemView>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {
constexpr int kInstanceIdRole = Qt::UserRole + 1;
constexpr int kNodeTypeRole = Qt::UserRole + 2;
constexpr int kVariableIdRole = Qt::UserRole + 3;
constexpr int kScreenIdRole = Qt::UserRole + 4;
constexpr int kWidgetIdRole = Qt::UserRole + 5;
constexpr int kVariableNameRole = Qt::UserRole + 6;
constexpr int kVariableTypeRole = Qt::UserRole + 7;

constexpr const char *kBindingGraphNodeMime = "application/x-lm-victrl-binding-node";
constexpr const char *kNodeScreensRoot = "screensRoot";
constexpr const char *kNodeScreen = "screen";
constexpr const char *kNodeWidget = "widget";
constexpr const char *kNodeVariablesRoot = "variablesRoot";
constexpr const char *kNodeVariable = "variable";

QList<ScreenData> screensSortedByOrder(QList<ScreenData> screens)
{
    std::stable_sort(screens.begin(), screens.end(), [](const ScreenData &a, const ScreenData &b) {
        return a.order < b.order;
    });
    return screens;
}

QList<WidgetInstance> widgetsSortedByZ(QList<WidgetInstance> widgets)
{
    std::stable_sort(widgets.begin(), widgets.end(), [](const WidgetInstance &a, const WidgetInstance &b) {
        return a.zOrder < b.zOrder;
    });
    return widgets;
}

QString widgetDisplayName(const WidgetInstance &inst)
{
    return inst.name.trimmed().isEmpty() ? inst.widgetId : inst.name.trimmed();
}

class ProjectTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    QStringList mimeTypes() const override
    {
        return { QString::fromLatin1(kBindingGraphNodeMime) };
    }

    QMimeData *mimeData(const QList<QTreeWidgetItem *> &items) const override
    {
        auto *mime = new QMimeData;
        if (items.isEmpty())
            return mime;

        QTreeWidgetItem *item = items.constFirst();
        const QString nodeType = item->data(0, kNodeTypeRole).toString();
        QJsonObject payload;
        if (nodeType == QLatin1String(kNodeWidget)) {
            payload.insert(QStringLiteral("nodeType"), QStringLiteral("widget"));
            payload.insert(QStringLiteral("refId"), item->data(0, kInstanceIdRole).toString());
            payload.insert(QStringLiteral("refName"), item->text(0));
            payload.insert(QStringLiteral("title"), item->text(0));
            payload.insert(QStringLiteral("widgetId"), item->data(0, kWidgetIdRole).toString());
            payload.insert(QStringLiteral("screenId"), item->data(0, kScreenIdRole).toString());
        } else if (nodeType == QLatin1String(kNodeVariable)) {
            payload.insert(QStringLiteral("nodeType"), QStringLiteral("data"));
            payload.insert(QStringLiteral("refId"), item->data(0, kVariableIdRole).toString());
            payload.insert(QStringLiteral("refName"), item->data(0, kVariableNameRole).toString());
            payload.insert(QStringLiteral("title"), item->data(0, kVariableNameRole).toString());
            payload.insert(QStringLiteral("valueType"), item->data(0, kVariableTypeRole).toString());
        }

        if (!payload.isEmpty())
            mime->setData(QString::fromLatin1(kBindingGraphNodeMime), QJsonDocument(payload).toJson(QJsonDocument::Compact));
        return mime;
    }

};
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

    m_tree = new ProjectTreeWidget(container);
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setIndentation(12);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    m_tree->setDefaultDropAction(Qt::CopyAction);
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
    setCurrentScreen(scene, QString(), QString());
}

void ProjectTreeDock::setProjectData(const ProjectData *project)
{
    m_project = project;
    refreshTree();
}

void ProjectTreeDock::setCurrentScene(CanvasScene *scene, const QString &screenName)
{
    setCurrentScreen(scene, QString(), screenName);
}

void ProjectTreeDock::setCurrentScreen(CanvasScene *scene, const QString &screenId, const QString &screenName)
{
    if (m_scene == scene) {
        m_screenId = screenId;
        m_screenName = screenName;
        refreshTree();
        syncSelectionFromScene();
        return;
    }
    if (m_scene)
        disconnect(m_scene, nullptr, this, nullptr);

    m_scene = scene;
    m_screenId = screenId;
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
    int screenCount = 0;

    auto *screensRoot = new QTreeWidgetItem(m_tree);
    screensRoot->setText(0, tr("图页"));
    screensRoot->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeScreensRoot));

    auto appendScreen = [&](const QString &screenId, const QString &screenName, const QList<WidgetInstance> &widgets) {
        const QString rootName = screenName.trimmed().isEmpty()
            ? tr("未命名图页")
            : screenName.trimmed();
        auto *root = new QTreeWidgetItem(screensRoot);
        root->setText(0, rootName);
        root->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeScreen));
        root->setData(0, kScreenIdRole, screenId);
        root->setToolTip(0, rootName);

        QList<WidgetInstance> instances = widgets;
        if (m_scene && !m_screenId.isEmpty() && screenId == m_screenId)
            instances = m_scene->instancesSortedByZ();
        else
            instances = widgetsSortedByZ(instances);

        for (const WidgetInstance &inst : instances) {
            if (inst.isGroup)
                continue;
            const QString displayName = widgetDisplayName(inst);
            auto *item = new QTreeWidgetItem(root);
            item->setText(0, displayName);
            item->setData(0, kNodeTypeRole, QString::fromLatin1(kNodeWidget));
            item->setData(0, kInstanceIdRole, inst.instanceId);
            item->setData(0, kScreenIdRole, screenId);
            item->setData(0, kWidgetIdRole, inst.widgetId);
            item->setToolTip(0, displayName);
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
            if ((screenId.isEmpty() || screenId == m_screenId) && inst.instanceId == selectedId)
                m_tree->setCurrentItem(item);
            ++count;
        }
        root->setExpanded(true);
        ++screenCount;
    };

    if (m_project) {
        const QList<ScreenData> screens = screensSortedByOrder(m_project->screens);
        for (const ScreenData &screen : screens)
            appendScreen(screen.id, screen.name, screen.widgets);
    } else if (m_scene) {
        appendScreen(m_screenId, m_screenName, m_scene->instancesSortedByZ());
    }
    screensRoot->setExpanded(true);

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
        item->setData(0, kVariableNameRole, variable.name);
        item->setData(0, kVariableTypeRole, type);
        item->setToolTip(0, variable.description.isEmpty() ? label : variable.description);
        item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
    }
    variablesRoot->setExpanded(true);

    m_updatingTree = false;

    if (m_countLabel) {
        if (m_project || m_scene)
            m_countLabel->setText(tr("%1 个图页 / %2 个控件 / %3 个变量")
                                      .arg(screenCount).arg(count).arg(variables.size()));
        else
            m_countLabel->setText(tr("未激活图页"));
    }
}

QTreeWidgetItem *ProjectTreeDock::currentScreenItem() const
{
    if (!m_tree) return nullptr;

    QTreeWidgetItem *firstScreen = nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *top = m_tree->topLevelItem(i);
        const int childCount = top ? top->childCount() : 0;
        for (int j = 0; j < childCount; ++j) {
            QTreeWidgetItem *item = top->child(j);
            if (item->data(0, kNodeTypeRole).toString() != QLatin1String(kNodeScreen))
                continue;
            if (!firstScreen)
                firstScreen = item;
            const QString screenId = item->data(0, kScreenIdRole).toString();
            if (!m_screenId.isEmpty() && screenId == m_screenId)
                return item;
            if (m_screenId.isEmpty() && item->text(0) == m_screenName.trimmed())
                return item;
        }
    }
    return firstScreen;
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

    QVariant initialValue;
    if (type == QLatin1String("number")) {
        initialValue = QInputDialog::getDouble(this, tr("初始值"), tr("值"),
                                               0.0, -1000000000000.0, 1000000000000.0,
                                               6, &ok);
        if (!ok) return;
    } else if (type == QLatin1String("string")) {
        initialValue = QInputDialog::getText(this, tr("初始值"), tr("值"),
                                             QLineEdit::Normal, QString(), &ok);
        if (!ok) return;
    } else if (type == QLatin1String("boolean")) {
        const QStringList values { QStringLiteral("false"), QStringLiteral("true") };
        const QString value = QInputDialog::getItem(this, tr("初始值"), tr("值"),
                                                    values, 0, false, &ok);
        if (!ok || value.isEmpty()) return;
        initialValue = (value == QLatin1String("true"));
    }

    emit addDataVariableRequested(name, type, initialValue);
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
    QTreeWidgetItem *root = currentScreenItem();
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
    const QString screenId = item->data(0, kScreenIdRole).toString();
    if (!screenId.isEmpty() && !m_screenId.isEmpty() && screenId != m_screenId) return;

    if (CanvasItem *canvasItem = m_scene->findItem(instanceId)) {
        m_syncingSelection = true;
        m_scene->clearSelection();
        canvasItem->setSelected(true);
        m_syncingSelection = false;
    }
}