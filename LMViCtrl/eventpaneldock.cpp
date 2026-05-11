#include "eventpaneldock.h"
#include "eventactiondialog.h"
#include "eventtriggerdialog.h"
#include "canvasitem.h"
#include "canvasscene.h"

#include <QFrame>
#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <QSet>

#include <utility>

EventPanelDock::EventPanelDock(QWidget *parent)
    : QDockWidget(tr("Event"), parent)
{
    setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    setMinimumHeight(130);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_root = new QWidget(m_scroll);
    m_grid = new QGridLayout(m_root);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(12);
    m_grid->setVerticalSpacing(4);
    m_scroll->setWidget(m_root);
    setWidget(m_scroll);

    clearPanel();
}

void EventPanelDock::setCurrentScene(CanvasScene *scene)
{
    if (m_scene == scene) {
        onSceneSelectionChanged();
        return;
    }
    if (m_scene)
        disconnect(m_scene, nullptr, this, nullptr);
    m_scene = scene;
    if (m_scene) {
        connect(m_scene.data(), &QGraphicsScene::selectionChanged,
                this, &EventPanelDock::onSceneSelectionChanged);
        connect(m_scene.data(), &CanvasScene::instanceEventsChanged,
                this, &EventPanelDock::onInstanceEventsChanged);
        connect(m_scene.data(), &CanvasScene::instanceChanged,
                this, &EventPanelDock::onInstanceChanged);
    }
    onSceneSelectionChanged();
}

void EventPanelDock::setProjectData(const ProjectData *project)
{
    m_project = project;
    rebuildPanel();
}

void EventPanelDock::onSceneSelectionChanged()
{
    rebuildPanel();
}

void EventPanelDock::onInstanceEventsChanged(const QString &instanceId)
{
    if (instanceId == m_currentInstanceId)
        rebuildPanel();
}

void EventPanelDock::onInstanceChanged(const QString &instanceId)
{
    if (instanceId == m_currentInstanceId)
        rebuildPanel();
}

void EventPanelDock::clearPanel()
{
    while (m_grid->count() > 0) {
        QLayoutItem *item = m_grid->takeAt(0);
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
    m_currentInstanceId.clear();

    auto *label = new QLabel(tr("请选择单个控件以配置事件"), m_root);
    label->setStyleSheet(QStringLiteral("color:#999; padding:10px;"));
    m_grid->addWidget(label, 0, 0, 1, 3);
}

void EventPanelDock::rebuildPanel()
{
    if (!m_scene) { clearPanel(); return; }

    const auto selected = m_scene->selectedItems();
    if (selected.size() != 1) { clearPanel(); return; }

    auto *ci = qgraphicsitem_cast<CanvasItem *>(selected.first());
    if (!ci) { clearPanel(); return; }

    const WidgetInstance inst = ci->instance();
    if (inst.isGroup) { clearPanel(); return; }
    const WidgetMeta meta = m_scene->widgetMeta(inst.widgetId);

    while (m_grid->count() > 0) {
        QLayoutItem *item = m_grid->takeAt(0);
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }

    m_currentInstanceId = inst.instanceId;

    int row = 0;
    for (const WidgetEventBinding &binding : inst.eventBindings) {
        if (binding.eventName.isEmpty()) continue;
        addEventRow(row++, eventDefForName(meta, binding.eventName), inst);
    }
    addNewTriggerRow(row, inst);

    m_grid->setColumnStretch(2, 1);
    m_grid->setRowStretch(row + 1, 1);
}

void EventPanelDock::addEventRow(int row, const EventDef &eventDef, const WidgetInstance &inst)
{
    const QString targetName = inst.name.isEmpty() ? inst.widgetId : inst.name;
    auto *targetLabel = new QLabel(targetName, m_root);
    targetLabel->setMinimumWidth(120);
    targetLabel->setStyleSheet(QStringLiteral("background:#a88612; color:#ddd; padding:8px;"));

    const QString eventText = eventDef.label.isEmpty() ? eventDef.name : eventDef.label;
    auto *eventLabel = new QLabel(eventText, m_root);
    eventLabel->setMinimumWidth(150);
    eventLabel->setStyleSheet(QStringLiteral("background:#7f8a10; color:#ddd; padding:6px;"));
    if (!eventDef.description.isEmpty())
        eventLabel->setToolTip(eventDef.description);

    auto *actionsWidget = new QWidget(m_root);
    auto *actionsLayout = new QHBoxLayout(actionsWidget);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(6);

    const QList<EventAction> actions = actionsForEvent(inst, eventDef.name);
    for (const EventAction &action : actions) {
        auto *chip = new QPushButton(action.label.isEmpty() ? action.type : action.label, actionsWidget);
        chip->setToolTip(action.targetName.isEmpty() ? action.code : action.targetName);
        chip->setStyleSheet(QStringLiteral("QPushButton { background:#2b7f8d; color:#ddd; border:0; padding:4px 10px; }"));
        connect(chip, &QPushButton::clicked, this, [this, eventName = eventDef.name, action]() {
            editAction(eventName, action);
        });
        actionsLayout->addWidget(chip);

        auto *del = new QToolButton(actionsWidget);
        del->setText(QStringLiteral("x"));
        del->setToolTip(tr("删除动作"));
        del->setStyleSheet(QStringLiteral("QToolButton { background:#6b6b00; color:#f66; border:0; padding:2px 5px; }"));
        connect(del, &QToolButton::clicked, this, [this, eventName = eventDef.name, id = action.id]() {
            deleteAction(eventName, id);
        });
        actionsLayout->addWidget(del);
    }

    auto *add = new QToolButton(actionsWidget);
    add->setText(QStringLiteral("+"));
    add->setToolTip(tr("添加动作"));
    add->setMinimumWidth(90);
    add->setStyleSheet(QStringLiteral("QToolButton { background:#287d89; color:#ccc; border:0; font-weight:bold; padding:4px; }"));
    connect(add, &QToolButton::clicked, this, [this, eventName = eventDef.name]() {
        addAction(eventName);
    });
    actionsLayout->addWidget(add);
    actionsLayout->addStretch();

    m_grid->addWidget(targetLabel, row, 0);
    m_grid->addWidget(eventLabel, row, 1);
    m_grid->addWidget(actionsWidget, row, 2);
}

void EventPanelDock::addNewTriggerRow(int row, const WidgetInstance &inst)
{
    const QString targetName = inst.name.isEmpty() ? inst.widgetId : inst.name;
    auto *targetLabel = new QLabel(targetName, m_root);
    targetLabel->setMinimumWidth(150);
    targetLabel->setStyleSheet(QStringLiteral("background:#a88612; color:#ddd; padding:8px;"));

    auto *add = new QToolButton(m_root);
    add->setText(QStringLiteral("+"));
    add->setToolTip(tr("选择事件"));
    add->setMinimumWidth(150);
    add->setStyleSheet(QStringLiteral("QToolButton { background:#8a9010; color:#ccc; border:0; font-weight:bold; padding:6px; }"));
    connect(add, &QToolButton::clicked, this, [this]() {
        chooseTriggerAndAddAction();
    });

    m_grid->addWidget(targetLabel, row, 0);
    m_grid->addWidget(add, row, 1);
}

QList<EventDef> EventPanelDock::eventDefsForMeta(const WidgetMeta &meta) const
{
    QList<EventDef> defs = meta.eventDefs;
    if (!defs.isEmpty()) return defs;
    for (const QString &name : meta.events) {
        EventDef def;
        def.name = name;
        def.label = name;
        defs.append(def);
    }
    return defs;
}

EventDef EventPanelDock::eventDefForName(const WidgetMeta &meta, const QString &eventName) const
{
    for (const EventDef &def : eventDefsForMeta(meta)) {
        if (def.name == eventName) return def;
    }
    EventDef def;
    def.name = eventName;
    def.label = eventName;
    return def;
}

QStringList EventPanelDock::existingEventNames(const WidgetInstance &inst) const
{
    QStringList names;
    for (const WidgetEventBinding &binding : inst.eventBindings) {
        if (!binding.eventName.isEmpty()) names.append(binding.eventName);
    }
    return names;
}

QList<EventAction> EventPanelDock::actionsForEvent(const WidgetInstance &inst, const QString &eventName) const
{
    for (const WidgetEventBinding &binding : inst.eventBindings) {
        if (binding.eventName == eventName)
            return binding.actions;
    }
    return {};
}

QList<WidgetInstance> EventPanelDock::currentSceneInstances() const
{
    return m_scene ? m_scene->allInstances() : QList<WidgetInstance>{};
}

QList<WidgetMeta> EventPanelDock::currentSceneMetas() const
{
    QList<WidgetMeta> metas;
    if (!m_scene) return metas;

    QSet<QString> seen;
    const QList<WidgetInstance> instances = currentSceneInstances();
    for (const WidgetInstance &instance : instances) {
        if (seen.contains(instance.widgetId)) continue;
        seen.insert(instance.widgetId);
        metas.append(m_scene->widgetMeta(instance.widgetId));
    }
    return metas;
}

void EventPanelDock::chooseTriggerAndAddAction()
{
    if (!m_scene || m_currentInstanceId.isEmpty()) return;
    WidgetInstance inst;
    if (!m_scene->instance(m_currentInstanceId, &inst)) return;

    const WidgetMeta meta = m_scene->widgetMeta(inst.widgetId);
    EventTriggerDialog triggerDialog(meta, existingEventNames(inst), this);
    if (triggerDialog.exec() != QDialog::Accepted) return;

    const EventDef trigger = triggerDialog.selectedEvent();
    if (trigger.name.isEmpty()) return;
    addAction(trigger.name);
}

void EventPanelDock::addAction(const QString &eventName)
{
    if (!m_scene || m_currentInstanceId.isEmpty()) return;
    EventActionDialog dlg(eventName,
                          m_project ? m_project->screens : QList<ScreenData>{},
                          currentSceneInstances(),
                          currentSceneMetas(),
                          this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_scene->addInstanceEventAction(m_currentInstanceId, eventName, dlg.action());
}

void EventPanelDock::editAction(const QString &eventName, const EventAction &action)
{
    if (!m_scene || m_currentInstanceId.isEmpty()) return;
    EventActionDialog dlg(eventName,
                          m_project ? m_project->screens : QList<ScreenData>{},
                          currentSceneInstances(),
                          currentSceneMetas(),
                          this);
    dlg.setAction(action);
    if (dlg.exec() != QDialog::Accepted) return;
    m_scene->updateInstanceEventAction(m_currentInstanceId, eventName, action.id, dlg.action());
}

void EventPanelDock::deleteAction(const QString &eventName, const QString &actionId)
{
    if (!m_scene || m_currentInstanceId.isEmpty()) return;
    m_scene->removeInstanceEventAction(m_currentInstanceId, eventName, actionId);
}
