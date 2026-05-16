/**
 * @file mainwindow_actions.cpp
 * @brief 菜单栏所有 Action 的实现函数
 *
 * 每个函数对应一个菜单项，在此文件中集中编写具体业务逻辑。
 * 当前均为空实现（接口占位），后续按需填充。
 */

#include "mainwindow.h"
#include "canvasscene.h"
#include "canvasitem.h"
#include "newprojectdialog.h"
#include "projectmanager.h"
#include "projectpropertiesdialog.h"
#include "propertypaneldock.h"
#include "projecttreedock.h"
#include "bindinggraphview.h"
#include "bindingdetaildock.h"
#include "screentab.h"
#include "screenmanagerdock.h"
#include "welcomewidget.h"
#include "widgettoolbox.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTranslator>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

namespace {
constexpr const char *kProjectFileSuffix = ".victrl";
constexpr const char *kProjectFilter =
    "LMViCtrl Project (*.victrl)";
constexpr const char *kWidgetInstancesMimeType =
    "application/x-qtlvgl-LMViCtrl-widget-instances";

QString ensureProjectFileSuffix(const QString &path)
{
    if (path.endsWith(QString::fromLatin1(kProjectFileSuffix), Qt::CaseInsensitive))
        return path;
    return path + QString::fromLatin1(kProjectFileSuffix);
}

QJsonObject widgetInstanceToJson(const WidgetInstance &inst)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("instanceId"), inst.instanceId);
    obj.insert(QStringLiteral("widgetId"), inst.widgetId);
    obj.insert(QStringLiteral("name"), inst.name);
    obj.insert(QStringLiteral("parentId"), inst.parentId);
    obj.insert(QStringLiteral("isGroup"), inst.isGroup);
    obj.insert(QStringLiteral("zOrder"), inst.zOrder);
    obj.insert(QStringLiteral("x"), inst.x);
    obj.insert(QStringLiteral("y"), inst.y);
    obj.insert(QStringLiteral("width"), inst.width);
    obj.insert(QStringLiteral("height"), inst.height);
    obj.insert(QStringLiteral("locked"), inst.locked);
    obj.insert(QStringLiteral("visible"), inst.visible);
    obj.insert(QStringLiteral("properties"), QJsonObject::fromVariantMap(inst.properties));
    QJsonObject events;
    for (const WidgetEventBinding &binding : inst.eventBindings) {
        QJsonArray actions;
        for (const EventAction &action : binding.actions) {
            QJsonObject item;
            item.insert(QStringLiteral("id"), action.id);
            item.insert(QStringLiteral("type"), action.type);
            item.insert(QStringLiteral("label"), action.label);
            item.insert(QStringLiteral("targetId"), action.targetId);
            item.insert(QStringLiteral("targetName"), action.targetName);
            item.insert(QStringLiteral("method"), action.method);
            item.insert(QStringLiteral("params"), QJsonObject::fromVariantMap(action.params));
            item.insert(QStringLiteral("code"), action.code);
            item.insert(QStringLiteral("condition"), action.condition);
            item.insert(QStringLiteral("delayMs"), action.delayMs);
            item.insert(QStringLiteral("enabled"), action.enabled);
            actions.append(item);
        }
        QJsonObject eventObject;
        eventObject.insert(QStringLiteral("executionMode"), binding.executionMode.isEmpty()
            ? QStringLiteral("sequence")
            : binding.executionMode);
        eventObject.insert(QStringLiteral("actions"), actions);
        events.insert(binding.eventName, eventObject);
    }
    obj.insert(QStringLiteral("events"), events);
    return obj;
}

WidgetInstance widgetInstanceFromJson(const QJsonObject &obj)
{
    WidgetInstance inst;
    inst.instanceId = obj.value(QStringLiteral("instanceId")).toString();
    inst.widgetId   = obj.value(QStringLiteral("widgetId")).toString();
    inst.name       = obj.value(QStringLiteral("name")).toString();
    inst.parentId   = obj.value(QStringLiteral("parentId")).toString();
    inst.isGroup    = obj.value(QStringLiteral("isGroup")).toBool(false);
    inst.zOrder     = obj.value(QStringLiteral("zOrder")).toInt(0);
    inst.x          = obj.value(QStringLiteral("x")).toInt(0);
    inst.y          = obj.value(QStringLiteral("y")).toInt(0);
    inst.width      = obj.value(QStringLiteral("width")).toInt(100);
    inst.height     = obj.value(QStringLiteral("height")).toInt(100);
    inst.locked     = obj.value(QStringLiteral("locked")).toBool(false);
    inst.visible    = obj.value(QStringLiteral("visible")).toBool(true);
    inst.properties = obj.value(QStringLiteral("properties")).toObject().toVariantMap();
    const QJsonObject events = obj.value(QStringLiteral("events")).toObject();
    for (auto it = events.constBegin(); it != events.constEnd(); ++it) {
        WidgetEventBinding binding;
        binding.eventName = it.key();
        QJsonArray actions;
        if (it.value().isArray()) {
            actions = it.value().toArray();
            binding.executionMode = QStringLiteral("sequence");
        } else {
            const QJsonObject eventObject = it.value().toObject();
            binding.executionMode = eventObject.value(QStringLiteral("executionMode")).toString(QStringLiteral("sequence"));
            actions = eventObject.value(QStringLiteral("actions")).toArray();
        }
        for (const QJsonValue &value : actions) {
            if (!value.isObject()) continue;
            const QJsonObject item = value.toObject();
            EventAction action;
            action.id = item.value(QStringLiteral("id")).toString();
            action.type = item.value(QStringLiteral("type")).toString();
            action.label = item.value(QStringLiteral("label")).toString();
            action.targetId = item.value(QStringLiteral("targetId")).toString();
            action.targetName = item.value(QStringLiteral("targetName")).toString();
            action.method = item.value(QStringLiteral("method")).toString();
            action.params = item.value(QStringLiteral("params")).toObject().toVariantMap();
            action.code = item.value(QStringLiteral("code")).toString();
            action.condition = item.value(QStringLiteral("condition")).toString();
            action.delayMs = item.value(QStringLiteral("delayMs")).toInt(0);
            action.enabled = item.value(QStringLiteral("enabled")).toBool(true);
            binding.actions.append(action);
        }
        if (!binding.actions.isEmpty())
            inst.eventBindings.append(binding);
    }
    return inst;
}

QByteArray encodeWidgetClipboard(const QList<WidgetInstance> &instances)
{
    QJsonArray widgets;
    for (const WidgetInstance &inst : instances)
        widgets.append(widgetInstanceToJson(inst));

    QJsonObject root;
    root.insert(QStringLiteral("schema"), QStringLiteral("qtlvgl-LMViCtrl-widget-instances"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("widgets"), widgets);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

QList<WidgetInstance> decodeWidgetClipboard(const QByteArray &data)
{
    QList<WidgetInstance> instances;

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return instances;

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schema")).toString()
        != QLatin1String("qtlvgl-LMViCtrl-widget-instances")) {
        return instances;
    }

    const QJsonArray widgets = root.value(QStringLiteral("widgets")).toArray();
    for (const QJsonValue &value : widgets) {
        if (!value.isObject()) continue;
        WidgetInstance inst = widgetInstanceFromJson(value.toObject());
        if (!inst.widgetId.isEmpty())
            instances.append(inst);
    }
    return instances;
}

void setSystemWidgetClipboard(const QList<WidgetInstance> &instances)
{
    if (instances.isEmpty()) return;

    auto *mime = new QMimeData;
    mime->setData(QLatin1String(kWidgetInstancesMimeType),
                  encodeWidgetClipboard(instances));
    QApplication::clipboard()->setMimeData(mime);
}

QList<WidgetInstance> systemWidgetClipboard()
{
    const QMimeData *mime = QApplication::clipboard()->mimeData();
    if (!mime || !mime->hasFormat(QLatin1String(kWidgetInstancesMimeType)))
        return {};
    return decodeWidgetClipboard(mime->data(QLatin1String(kWidgetInstancesMimeType)));
}

QVariant defaultValueForDataVariableType(const QString &type)
{
    if (type == QLatin1String("string")) return QString();
    if (type == QLatin1String("boolean")) return false;
    if (type == QLatin1String("list")) return QVariantList{};
    return 0.0;
}

QString normalizedDataVariableName(const QString &name)
{
    QString normalized = name.trimmed();
    if (normalized.isEmpty())
        normalized = QStringLiteral("value");
    if (!normalized.startsWith(QLatin1Char('$')))
        normalized.prepend(QLatin1Char('$'));
    return normalized;
}

// ------------------------------------------------------------
// 图页 Undo 命令
// ------------------------------------------------------------
class AddScreenCommand : public QUndoCommand
{
public:
    AddScreenCommand(MainWindow *mw, const ScreenData &data)
        : QUndoCommand(QObject::tr("新增图页 %1").arg(data.name))
        , m_mw(mw)
        , m_data(data)
        , m_order(data.order)
    {}

    void redo() override { m_mw->cmdAddScreen(m_data, m_order); }
    void undo() override { m_mw->cmdRemoveScreen(m_data.id); }

private:
    MainWindow *m_mw;
    ScreenData  m_data;
    int         m_order;
};

class RemoveScreenCommand : public QUndoCommand
{
public:
    RemoveScreenCommand(MainWindow *mw, const QString &screenId)
        : QUndoCommand(QObject::tr("删除图页"))
        , m_mw(mw)
        , m_id(screenId)
    {}

    void redo() override
    {
        // 每次 redo 时重新快照，确保恢复最新画布内容
        m_data  = m_mw->snapshotScreen(m_id);
        m_order = m_data.order;
        setText(QObject::tr("删除图页 %1").arg(m_data.name));
        m_mw->cmdRemoveScreen(m_id);
    }
    void undo() override
    {
        m_mw->cmdAddScreen(m_data, m_order);
    }

private:
    MainWindow *m_mw;
    QString     m_id;
    ScreenData  m_data;
    int         m_order   = 0;
};

class AddDataVariableCommand : public QUndoCommand
{
public:
    AddDataVariableCommand(MainWindow *mw, const DataVariable &variable, int order)
        : QUndoCommand(QObject::tr("新增数据变量 %1").arg(variable.name))
        , m_mw(mw)
        , m_variable(variable)
        , m_order(order)
    {}

    void redo() override { m_mw->cmdAddDataVariable(m_variable, m_order); }
    void undo() override { m_mw->cmdRemoveDataVariable(m_variable.id); }

private:
    MainWindow *m_mw;
    DataVariable m_variable;
    int m_order = 0;
};

class RemoveDataVariableCommand : public QUndoCommand
{
public:
    RemoveDataVariableCommand(MainWindow *mw, const QString &id)
        : QUndoCommand(QObject::tr("删除数据变量"))
        , m_mw(mw)
        , m_id(id)
    {}

    void redo() override
    {
        m_variable = m_mw->snapshotDataVariable(m_id);
        m_order = m_mw->dataVariableOrder(m_id);
        setText(QObject::tr("删除数据变量 %1").arg(m_variable.name));
        m_mw->cmdRemoveDataVariable(m_id);
    }

    void undo() override { m_mw->cmdAddDataVariable(m_variable, m_order); }

private:
    MainWindow *m_mw;
    QString m_id;
    DataVariable m_variable;
    int m_order = 0;
};
}  // namespace

// ============================================================
// 工程辅助函数
// ============================================================

void MainWindow::resetProject()
{
    // 关闭所有已打开的图页 tab
    if (m_tabWidget) {
        m_tabWidget->clear();
        m_openTabs.clear();
    }
    resetUndoChains();
    m_widgetClipboard.clear();
    m_widgetPasteCount = 0;

    m_project = ProjectData{};
    m_project.id        = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_project.createdAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_project.updatedAt = m_project.createdAt;

    ScreenData s;
    s.id    = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name  = QStringLiteral("Screen1");
    s.order = 0;
    m_project.screens.append(s);

    m_projectFilePath.clear();

    if (m_screenManager)
        m_screenManager->setScreens(m_project.screens);

    // 默认打开第一个图页
    openScreenTab(s.id);
    openBindingGraphTab();
}

void MainWindow::syncSceneToProject()
{
    // 将所有已打开的图页同步回 m_project
    for (ScreenData &screen : m_project.screens) {
        if (ScreenTab *tab = m_openTabs.value(screen.id, nullptr))
            tab->syncToScreen(screen);
    }
    refreshBindingGraphTab();
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void MainWindow::applyProjectToTabs()
{
    if (!m_tabWidget) return;
    m_tabWidget->clear();
    m_openTabs.clear();
    resetUndoChains();
    m_widgetClipboard.clear();
    m_widgetPasteCount = 0;

    // 为旧工程/缺失 name 的实例补一个唯一名
    ensureInstanceNamesAssigned();

    // 应用工程字体（影响所有画布元素的文字绘制）
    {
        const QString projDir = m_projectFilePath.isEmpty()
            ? QString()
            : QFileInfo(m_projectFilePath).absolutePath();
        CanvasItem::setProjectFont(m_project.font.file,
                                   m_project.font.size,
                                   projDir);
    }

    if (m_screenManager)
        m_screenManager->setScreens(m_project.screens);

    // 开启所有图页
    for (const ScreenData &s : std::as_const(m_project.screens))
        openScreenTab(s.id);
    openBindingGraphTab();
}

void MainWindow::openBindingGraphTab()
{
    if (!m_tabWidget || !m_bindingGraphView) return;
    m_bindingGraphView->setProjectData(&m_project);
    m_bindingGraphView->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
    if (m_bindingDetailPanel) {
        m_bindingDetailPanel->setProjectData(&m_project);
        m_bindingDetailPanel->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
        m_bindingDetailPanel->setGraphView(m_bindingGraphView);
        m_bindingDetailPanel->show();
        m_bindingDetailPanel->raise();
    }
    registerUndoStack(m_bindingGraphView->undoStack());

    const int existing = m_tabWidget->indexOf(m_bindingGraphView);
    if (existing >= 0) {
        m_tabWidget->setCurrentIndex(existing);
        m_bindingGraphView->refreshGraph();
        return;
    }

    m_tabWidget->addTab(m_bindingGraphView, tr("绑定模式"));
    m_tabWidget->setCurrentWidget(m_bindingGraphView);
    m_bindingGraphView->refreshGraph();
}

void MainWindow::refreshBindingGraphTab()
{
    if (!m_bindingGraphView) return;
    m_bindingGraphView->setProjectData(m_projectOpen ? &m_project : nullptr);
    m_bindingGraphView->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
    if (m_bindingDetailPanel) {
        m_bindingDetailPanel->setProjectData(m_projectOpen ? &m_project : nullptr);
        m_bindingDetailPanel->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
        m_bindingDetailPanel->setGraphView(m_bindingGraphView);
    }
    m_bindingGraphView->refreshGraph();
}

void MainWindow::openScreenTab(const QString &screenId)
{
    // 已开着则激活
    if (ScreenTab *existing = m_openTabs.value(screenId, nullptr)) {
        m_tabWidget->setCurrentWidget(existing);
        return;
    }

    // 找到对应 ScreenData
    ScreenData *sd = nullptr;
    for (ScreenData &s : m_project.screens) {
        if (s.id == screenId) { sd = &s; break; }
    }
    if (!sd) return;

    const QSize cvSize(m_project.target.width, m_project.target.height);
    const QList<WidgetMeta> metas = m_widgetToolbox ? m_widgetToolbox->widgetMetas()
                                                    : QList<WidgetMeta>{};
    auto *tab = new ScreenTab(*sd, cvSize, metas, m_tabWidget);

    // 安装名字生成器（拖入新组件时使用，确保工程内唯一）
    installSceneNameGenerator(tab->scene());
    connectSceneLogging(tab->scene(), screenId);

    // 注册该图页的 undo 栈到链式调度
    if (tab->scene())
        registerUndoStack(tab->scene()->undoStack(), screenId);

    if (CanvasScene *scene = tab->scene()) {
        connect(scene, &CanvasScene::cutRequested, this, &MainWindow::onCut);
        connect(scene, &CanvasScene::copyRequested, this, &MainWindow::onCopy);
        connect(scene, &CanvasScene::pasteRequested, this, &MainWindow::onPaste);
        connect(scene, &CanvasScene::deleteRequested, this, &MainWindow::onDelete);
        connect(scene, &CanvasScene::bringToFrontRequested, this, &MainWindow::onBringToFront);
        connect(scene, &CanvasScene::sendToBackRequested, this, &MainWindow::onSendToBack);
        connect(scene, &CanvasScene::bringForwardRequested, this, &MainWindow::onBringForward);
        connect(scene, &CanvasScene::sendBackwardRequested, this, &MainWindow::onSendBackward);
    }

    const QString tabTitle = QStringLiteral("%1. %2").arg(sd->order + 1).arg(sd->name);
    m_tabWidget->addTab(tab, tabTitle);
    m_tabWidget->setCurrentWidget(tab);
    m_openTabs.insert(screenId, tab);

    if (m_propertyPanel)
        m_propertyPanel->setCurrentScene(tab->scene());
    if (m_projectTree)
        m_projectTree->setCurrentScreen(tab->scene(), sd->id, sd->name);
}

void MainWindow::closeScreenTab(const QString &screenId)
{
    ScreenTab* tab = m_openTabs.value(screenId, nullptr);
    if (!tab) return;

    // 先同步数据
    for (ScreenData& s : m_project.screens) {
        if (s.id == screenId) { tab->syncToScreen(s); break; }
    }

    // 在 tab 销毁前，主动清理 undo stack 的注册信息，
    // 避免 destroyed 信号回调时 m_stackLastIndex 处于不确定状态
    if (CanvasScene* scene = tab->scene()) {
        if (QUndoStack* stack = scene->undoStack()) {
            QObject::disconnect(stack, nullptr, this, nullptr); // 断开所有到 this 的连接
            m_stackLastIndex.remove(stack);
            m_stackScreenIds.remove(stack);
            m_undoChain.removeAll(stack);
            m_redoChain.removeAll(stack);
        }
    }

    const int idx = m_tabWidget->indexOf(tab);
    if (idx >= 0) m_tabWidget->removeTab(idx);
    m_openTabs.remove(screenId);
    tab->deleteLater();
}

void MainWindow::refreshTabWidgetMetas()
{
    const QList<WidgetMeta> metas = m_widgetToolbox ? m_widgetToolbox->widgetMetas()
                                                    : QList<WidgetMeta>{};
    for (ScreenTab *tab : std::as_const(m_openTabs))
        tab->scene()->setWidgetMetas(metas);
    if (m_bindingGraphView)
        m_bindingGraphView->setWidgetMetas(metas);
    if (m_bindingDetailPanel)
        m_bindingDetailPanel->setWidgetMetas(metas);
}

ScreenTab *MainWindow::currentScreenTab() const
{
    if (!m_tabWidget) return nullptr;
    return qobject_cast<ScreenTab *>(m_tabWidget->currentWidget());
}

CanvasScene *MainWindow::currentScene() const
{
    ScreenTab *tab = currentScreenTab();
    return tab ? tab->scene() : nullptr;
}

QString MainWindow::currentScreenName() const
{
    ScreenTab *tab = currentScreenTab();
    if (!tab) return QString();
    const QString id = tab->screenId();
    for (const ScreenData &screen : m_project.screens) {
        if (screen.id == id)
            return screen.name;
    }
    return QString();
}

QString MainWindow::currentScreenId() const
{
    ScreenTab *tab = currentScreenTab();
    return tab ? tab->screenId() : QString();
}

// 图页管理器信号
void MainWindow::onScreenManagerOpenRequested(const QString &screenId)
{
    if (screenId.startsWith(QLatin1String("__delete__:"))) {
        const QString id = screenId.mid(11);
        closeScreenTab(id);
        // 删除后若没有打开的 tab，自动打开第一个
        if (m_tabWidget && m_tabWidget->count() == 0 && !m_project.screens.isEmpty())
            openScreenTab(m_project.screens.first().id);
        return;
    }
    openScreenTab(screenId);
}

void MainWindow::onScreensChanged(const QList<ScreenData> &updatedScreens)
{
    // 先同步当前所有已打开的图页数据
    syncSceneToProject();

    // 找到被删除的图页，关闭对应 tab
    QSet<QString> newIds;
    for (const ScreenData &s : updatedScreens) newIds.insert(s.id);
    const QList<QString> oldIds = m_openTabs.keys();
    for (const QString &id : oldIds) {
        if (!newIds.contains(id)) closeScreenTab(id);
    }

    // 更新 m_project.screens（保留 widgets）
    QHash<QString, QList<WidgetInstance>> widgetMap;
    QHash<QString, QString> bgColorMap;
    for (const ScreenData &s : std::as_const(m_project.screens)) {
        widgetMap.insert(s.id, s.widgets);
        bgColorMap.insert(s.id, s.bgColor);
    }

    m_project.screens.clear();
    for (ScreenData s : updatedScreens) {
        s.widgets = widgetMap.value(s.id);
        s.bgColor = bgColorMap.value(s.id, s.bgColor);
        m_project.screens.append(s);
    }

    // 刷新已打开 tab 的标题
    for (ScreenData &s : m_project.screens) {
        if (ScreenTab *tab = m_openTabs.value(s.id, nullptr)) {
            const int idx = m_tabWidget->indexOf(tab);
            if (idx >= 0)
                m_tabWidget->setTabText(idx,
                    QStringLiteral("%1. %2").arg(s.order + 1).arg(s.name));
        }
    }

    // 应用分辨率变化到所有已开 tab
    for (ScreenTab *tab : std::as_const(m_openTabs))
        tab->scene()->setCanvasSize(m_project.target.width, m_project.target.height);

    if (m_projectTree)
        m_projectTree->setCurrentScreen(currentScene(), currentScreenId(), currentScreenName());
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *widget = m_tabWidget->widget(index);
    if (widget == m_bindingGraphView) {
        m_tabWidget->removeTab(index);
        if (m_tabWidget->count() == 0 && !m_project.screens.isEmpty())
            openScreenTab(m_project.screens.first().id);
        return;
    }
    auto *tab = qobject_cast<ScreenTab *>(m_tabWidget->widget(index));
    if (!tab) return;
    closeScreenTab(tab->screenId());
    // 如果已无打开的 tab，自动打开第一个
    if (m_tabWidget->count() == 0 && !m_project.screens.isEmpty())
        openScreenTab(m_project.screens.first().id);
}

void MainWindow::setProjectOpen(bool open)
{
    m_projectOpen = open;

    // 切换中央区域：欢迎页 ↔ 设计区
    if (m_stackedWidget)
        m_stackedWidget->setCurrentIndex(open ? 1 : 0);

    // 侧边栏停靠窗口随工程状态显示/隐藏
    if (m_screenManager)
        m_screenManager->setVisible(open);
    if (m_widgetToolbox)
        m_widgetToolbox->setVisible(open);
    if (m_projectTree) {
        m_projectTree->setVisible(open);
        if (!open) m_projectTree->setCurrentScene(nullptr);
    }
    if(m_propertyPanel)
        m_propertyPanel->setVisible(open);
    resizeDocks({m_screenManager, m_widgetToolbox}, {100, 400}, Qt::Vertical);
    // 若切回欢迎页，刷新最近工程列表
    if (!open && m_welcomeWidget)
        m_welcomeWidget->setRecentProjects(m_recentProjects);

    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("罗米视控");
    if (m_projectOpen) {
        const QString name = m_projectFilePath.isEmpty()
                                 ? (m_project.name.isEmpty() ? tr("未命名") : m_project.name)
                                 : QFileInfo(m_projectFilePath).fileName();
        title = QStringLiteral("%1 - %2").arg(name, title);
    }
    setWindowTitle(title);
}

bool MainWindow::saveProjectToPath(const QString &path)
{
    syncSceneToProject();
    QString err;
    if (!ProjectManager::saveToFile(m_project, path, &err)) {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("无法保存工程：%1").arg(err));
        return false;
    }
    m_projectFilePath = path;
    updateWindowTitle();
    addRecentProject(path);
    return true;
}

bool MainWindow::maybeSaveCurrent()
{
    if (!m_projectOpen) return true;

    const auto ret = QMessageBox::question(
        this, tr("关闭工程"),
        tr("是否保存当前工程的修改？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (ret == QMessageBox::Cancel) return false;
    if (ret == QMessageBox::Discard) return true;

    onSave();
    // 保存被取消时 m_projectFilePath 仍可能为空
    return !m_projectFilePath.isEmpty();
}

// ============================================================
// 文件
// ============================================================

void MainWindow::onNewProject()
{
    if (m_projectOpen && !maybeSaveCurrent()) return;

    NewProjectDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;

    resetProject();

    const QString name     = dlg.projectName();
    const QString location = dlg.projectLocation();

    if (!name.isEmpty())
        m_project.name = name;

    // 根据模板 ID 做个性化初始化
    const QString tplId = dlg.templateId();
    if (tplId == QLatin1String("hmi")) {
        m_project.target.width  = 1024;
        m_project.target.height = 768;
    } else if (tplId.startsWith(QLatin1String("tpl_")) ||
               tplId.startsWith(QLatin1String("svg"))) {
        m_project.target.width  = 800;
        m_project.target.height = 480;
    } else {
        m_project.target.width  = 480;
        m_project.target.height = 320;
    }

    // 保存到指定路径
    if (location.isEmpty() || name.isEmpty()) {
        setProjectOpen(true);
        return;
    }

    // 1) 在所选目录下新建工程同名文件夹
    QDir parent(location);
    if (!parent.exists()) {
        QMessageBox::warning(this, tr("新建失败"),
                             tr("存储路径不存在：%1").arg(location));
        return;
    }
    if (parent.exists(name)) {
        QMessageBox::warning(this, tr("新建失败"),
                             tr("目录已存在：%1").arg(parent.filePath(name)));
        return;
    }
    if (!parent.mkdir(name)) {
        QMessageBox::warning(this, tr("新建失败"),
                             tr("无法创建工程目录：%1").arg(parent.filePath(name)));
        return;
    }

    const QString projectDir = parent.filePath(name);
    const QString filePath   = projectDir + QLatin1Char('/')
                               + ProjectManager::projectJsonFileName(name);

    // 2) 写入工程 JSON
    m_projectFilePath = filePath;
    QString err;
    if (!ProjectManager::saveToFile(m_project, filePath, &err)) {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("无法创建工程文件：%1").arg(err));
    } else {
        addRecentProject(filePath);
    }

    // 3) 把 LMViCtrl 自带的 lua 运行时（runtime.lua / common / widgets）
    //    拷贝到工程目录，供后续编译产物 require 与仿真器加载
    QString depErr;
    if (!ProjectManager::deployRuntime(projectDir, /*overwriteWidgets=*/false, &depErr)) {
        QMessageBox::warning(this, tr("部署运行时失败"),
                             tr("拷贝 lua 运行时到工程目录失败：%1\n"
                                "仿真功能在此工程中可能不可用。").arg(depErr));
    }

    setProjectOpen(true);
}

void MainWindow::onNewBindingPreviewSample()
{
    if (m_projectOpen && !maybeSaveCurrent()) return;

    const QString parentDir = QFileDialog::getExistingDirectory(
        this, tr("选择样例保存目录"));
    if (parentDir.isEmpty()) return;

    const QString baseName = QStringLiteral("BindingPreviewSample");
    QDir parent(parentDir);
    QString projectName = baseName;
    int suffix = 1;
    while (parent.exists(projectName))
        projectName = QStringLiteral("%1_%2").arg(baseName).arg(++suffix);
    if (!parent.mkdir(projectName)) {
        QMessageBox::warning(this, tr("创建样例失败"),
                             tr("无法创建工程目录：%1").arg(parent.filePath(projectName)));
        return;
    }

    resetProject();
    m_project.name = projectName;
    m_project.description = tr("图形化绑定系统端到端预览样例：按钮事件写入数据变量，变量变化驱动标签文本。");
    m_project.target.width = 480;
    m_project.target.height = 320;
    m_project.target.platform = QStringLiteral("windows");

    ScreenData &screen = m_project.screens.first();
    screen.name = QStringLiteral("BindingDemo");
    screen.bgColor = QStringLiteral("#202020");
    screen.widgets.clear();

    WidgetInstance button;
    button.instanceId = QStringLiteral("btn_increment");
    button.widgetId = QStringLiteral("lv_button_basic");
    button.name = QStringLiteral("Button_Update");
    button.x = 36;
    button.y = 48;
    button.width = 160;
    button.height = 52;
    button.zOrder = 1;
    button.properties.insert(QStringLiteral("label"), tr("点击更新"));
    button.properties.insert(QStringLiteral("bg_color"), QStringLiteral("#007acc"));
    button.properties.insert(QStringLiteral("color"), QStringLiteral("#ffffff"));
    button.properties.insert(QStringLiteral("enabled"), true);
    button.properties.insert(QStringLiteral("visible"), true);

    WidgetInstance directLabel;
    directLabel.instanceId = QStringLiteral("lbl_direct");
    directLabel.widgetId = QStringLiteral("lv_label_basic");
    directLabel.name = QStringLiteral("Label_Direct");
    directLabel.x = 230;
    directLabel.y = 48;
    directLabel.width = 210;
    directLabel.height = 42;
    directLabel.zOrder = 2;
    directLabel.properties.insert(QStringLiteral("text"), tr("等待直接动作"));
    directLabel.properties.insert(QStringLiteral("color"), QStringLiteral("#ffffff"));
    directLabel.properties.insert(QStringLiteral("font_size"), 16);
    directLabel.properties.insert(QStringLiteral("visible"), true);

    WidgetInstance variableLabel;
    variableLabel.instanceId = QStringLiteral("lbl_message");
    variableLabel.widgetId = QStringLiteral("lv_label_basic");
    variableLabel.name = QStringLiteral("Label_Message");
    variableLabel.x = 36;
    variableLabel.y = 140;
    variableLabel.width = 404;
    variableLabel.height = 48;
    variableLabel.zOrder = 3;
    variableLabel.properties.insert(QStringLiteral("text"), tr("等待变量绑定"));
    variableLabel.properties.insert(QStringLiteral("color"), QStringLiteral("#9cdcfe"));
    variableLabel.properties.insert(QStringLiteral("font_size"), 18);
    variableLabel.properties.insert(QStringLiteral("visible"), true);

    screen.widgets = { button, directLabel, variableLabel };

    DataVariable message;
    message.id = QStringLiteral("message");
    message.name = QStringLiteral("$message");
    message.type = QStringLiteral("string");
    message.value = tr("等待变量绑定");
    message.defaultValue = message.value;
    message.description = tr("按钮点击后写入，并通过 BindingGraph 更新 Label_Message.text");
    m_project.dataVariables = { message };

    auto widgetNode = [](const WidgetInstance &instance, int x, int y) {
        BindingNode node;
        node.id = QStringLiteral("widget:%1").arg(instance.instanceId);
        node.type = QStringLiteral("widget");
        node.refId = instance.instanceId;
        node.refName = instance.name;
        node.title = instance.name;
        node.x = x;
        node.y = y;
        return node;
    };
    auto dataNode = [](const DataVariable &variable, int x, int y) {
        BindingNode node;
        node.id = QStringLiteral("data:%1").arg(variable.id);
        node.type = QStringLiteral("data");
        node.refId = variable.id;
        node.refName = variable.name;
        node.title = variable.name;
        node.x = x;
        node.y = y;
        return node;
    };
    auto endpoint = [](const BindingNode &node,
                       const QString &kind,
                       const QString &name,
                       const QString &valueType) {
        BindingEndpoint endpoint;
        endpoint.nodeId = node.id;
        endpoint.nodeType = node.type;
        endpoint.refId = node.refId;
        endpoint.refName = node.refName;
        endpoint.portKind = kind;
        endpoint.portName = name;
        endpoint.valueType = valueType;
        return endpoint;
    };

    const BindingNode buttonNode = widgetNode(button, 40, 80);
    const BindingNode directNode = widgetNode(directLabel, 420, 40);
    const BindingNode messageNode = widgetNode(variableLabel, 420, 250);
    const BindingNode variableNode = dataNode(message, 230, 230);
    m_project.bindingGraph.nodes = { buttonNode, directNode, variableNode, messageNode };

    BindingEdge directEdge;
    directEdge.id = QStringLiteral("edge_button_direct_label");
    directEdge.type = QStringLiteral("event_action");
    directEdge.source = endpoint(buttonNode, QStringLiteral("event"), QStringLiteral("clicked"), QStringLiteral("event"));
    directEdge.target = endpoint(directNode, QStringLiteral("action"), QStringLiteral("setText"), QStringLiteral("string"));
    directEdge.label = tr("按钮点击 -> 直接设置标签");
    directEdge.order = 1;
    directEdge.params.insert(QStringLiteral("value"), tr("直接动作：按钮已点击"));

    BindingEdge dataWriteEdge;
    dataWriteEdge.id = QStringLiteral("edge_button_set_message");
    dataWriteEdge.type = QStringLiteral("event_data");
    dataWriteEdge.source = directEdge.source;
    dataWriteEdge.target = endpoint(variableNode, QStringLiteral("data_set"), QStringLiteral("set"), QStringLiteral("string"));
    dataWriteEdge.label = tr("按钮点击 -> 写入 $message");
    dataWriteEdge.order = 2;
    dataWriteEdge.params.insert(QStringLiteral("value"), tr("数据变量：按钮已点击"));

    BindingEdge propertyEdge;
    propertyEdge.id = QStringLiteral("edge_message_to_label");
    propertyEdge.type = QStringLiteral("property_binding");
    propertyEdge.source = endpoint(variableNode, QStringLiteral("data_get"), QStringLiteral("onChange"), QStringLiteral("string"));
    propertyEdge.target = endpoint(messageNode, QStringLiteral("property"), QStringLiteral("text"), QStringLiteral("string"));
    propertyEdge.label = tr("$message -> Label_Message.text");
    propertyEdge.order = 3;

    m_project.bindingGraph.edges = { directEdge, dataWriteEdge, propertyEdge };

    const QString projectDir = parent.filePath(projectName);
    m_projectFilePath = projectDir + QLatin1Char('/') + ProjectManager::projectJsonFileName(projectName);
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    QString err;
    if (!ProjectManager::saveToFile(m_project, m_projectFilePath, &err)) {
        QMessageBox::warning(this, tr("创建样例失败"), tr("保存工程失败：%1").arg(err));
        return;
    }
    QString depErr;
    if (!ProjectManager::deployRuntime(projectDir, false, &depErr)) {
        QMessageBox::warning(this, tr("创建样例失败"), tr("部署运行时失败：%1").arg(depErr));
        return;
    }
    const QString luaPath = projectDir + QLatin1Char('/') + ProjectManager::projectLuaFileName(projectName);
    if (!ProjectManager::compileFileToLua(m_projectFilePath, luaPath, &err)) {
        QMessageBox::warning(this, tr("创建样例失败"), tr("编译 Lua 失败：%1").arg(err));
        return;
    }

    addRecentProject(m_projectFilePath);
    applyProjectToTabs();
    setProjectOpen(true);
    openBindingGraphTab();
    appendLog(tr("端到端样例已创建：%1。按 F5 可预览运行。").arg(m_projectFilePath));
}

void MainWindow::onOpenProject()
{
    if (m_projectOpen && !maybeSaveCurrent()) return;

    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开工程"), QString(), tr(kProjectFilter));
    if (path.isEmpty()) return;

    ProjectData p;
    QString err;
    if (!ProjectManager::loadFromFile(&p, path, &err)) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法打开工程：%1").arg(err));
        return;
    }
    m_project         = p;
    m_projectFilePath = path;
    applyProjectToTabs();
    setProjectOpen(true);
    addRecentProject(path);
}

void MainWindow::onCloseProject()
{
    if (!m_projectOpen) return;
    if (!maybeSaveCurrent()) return;

    // 清空 tab 和工程数据，回到欢迎页
    if (m_tabWidget) {
        m_tabWidget->clear();
        m_openTabs.clear();
    }
    resetUndoChains();
    m_project = ProjectData{};
    m_projectFilePath.clear();
    if (m_screenManager)
        m_screenManager->setScreens({});
    // 清除工程字体
    CanvasItem::setProjectFont(QString(), 0);
    setProjectOpen(false);
}

// 从欢迎页的最近工程列表直接打开
void MainWindow::onOpenRecentProject(const QString &path)
{
    if (path.isEmpty()) return;
    if (m_projectOpen && !maybeSaveCurrent()) return;

    ProjectData p;
    QString err;
    if (!ProjectManager::loadFromFile(&p, path, &err)) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法打开工程：%1").arg(err));
        return;
    }
    m_project         = p;
    m_projectFilePath = path;
    applyProjectToTabs();
    setProjectOpen(true);
    addRecentProject(path);
}

void MainWindow::onSave()
{
    if (!m_projectOpen) return;
    if (m_projectFilePath.isEmpty()) {
        onSaveAs();
        return;
    }
    saveProjectToPath(m_projectFilePath);
}

void MainWindow::onSaveAs()
{
    if (!m_projectOpen) return;

    const QString projectName = m_project.name.isEmpty() ? QStringLiteral("project")
                                                         : m_project.name;
    QString defaultName;
    if (!m_projectFilePath.isEmpty())
        defaultName = QFileInfo(m_projectFilePath).absoluteDir().absoluteFilePath(
            ProjectManager::projectJsonFileName(projectName));
    else
        defaultName = QDir::current().absoluteFilePath(
            ProjectManager::projectJsonFileName(projectName));

    const QString path = QFileDialog::getSaveFileName(
        this, tr("另存为"), defaultName, tr(kProjectFilter));
    if (path.isEmpty()) return;

    saveProjectToPath(ensureProjectFileSuffix(path));
}

void MainWindow::onExportZip() {}

void MainWindow::onExportXml() {}

void MainWindow::onExportReport() {}

void MainWindow::onImportProject() {}

void MainWindow::onProjectProperties()
{
    if (!m_projectOpen) return;

    const QString projectDir = m_projectFilePath.isEmpty()
        ? QString()
        : QFileInfo(m_projectFilePath).absolutePath();

    ProjectPropertiesDialog dlg(m_project, projectDir, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const ProjectData newData = dlg.projectData();

    const bool resolutionChanged =
        (newData.target.width  != m_project.target.width ||
         newData.target.height != m_project.target.height);

    const bool fontChanged =
        (newData.font.file != m_project.font.file ||
         newData.font.size != m_project.font.size);

    m_project = newData;
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (resolutionChanged) {
        for (ScreenTab *tab : std::as_const(m_openTabs))
            tab->scene()->setCanvasSize(m_project.target.width, m_project.target.height);
    }

    if (fontChanged) {
        CanvasItem::setProjectFont(m_project.font.file,
                                   m_project.font.size,
                                   projectDir);
        for (ScreenTab *tab : std::as_const(m_openTabs)) {
            if (tab->scene()) tab->scene()->update();
        }
    }

    updateWindowTitle();
}

void MainWindow::onExit()
{
    close();
}

// ============================================================
// 编辑
// ============================================================

void MainWindow::onUndo()
{
    while (!m_undoChain.isEmpty()) {
        QUndoStack *top = m_undoChain.last();
        if (!top || !top->canUndo()) {
            m_undoChain.removeLast();
            continue;
        }
        if (!activateUndoStackPage(top)) {
            m_undoChain.removeLast();
            continue;
        }
        top->undo();   // indexChanged 处理器会负责把 stack 从 undoChain 转到 redoChain
        return;
    }
}

void MainWindow::onRedo()
{
    while (!m_redoChain.isEmpty()) {
        QUndoStack *top = m_redoChain.last();
        if (!top || !top->canRedo()) {
            m_redoChain.removeLast();
            continue;
        }
        if (!activateUndoStackPage(top)) {
            m_redoChain.removeLast();
            continue;
        }
        m_inRedoOp = true;
        top->redo();   // indexChanged 处理器会负责把 stack 从 redoChain 转回 undoChain
        m_inRedoOp = false;
        return;
    }
}

void MainWindow::onCut()
{
    if (auto *s = currentScene()) {
        m_widgetClipboard = s->selectedInstances();
        m_widgetPasteCount = 0;
        setSystemWidgetClipboard(m_widgetClipboard);
        s->deleteSelected();
    }
}

void MainWindow::onCopy()
{
    if (auto *s = currentScene()) {
        m_widgetClipboard = s->selectedInstances();
        m_widgetPasteCount = 0;
        if (!m_widgetClipboard.isEmpty()) {
            setSystemWidgetClipboard(m_widgetClipboard);
            appendLog(tr("复制元素：%1 个").arg(m_widgetClipboard.size()));
        }
    }
}

void MainWindow::onPaste()
{
    if (auto *s = currentScene()) {
        const QList<WidgetInstance> systemClipboard = systemWidgetClipboard();
        if (!systemClipboard.isEmpty()) {
            m_widgetClipboard = systemClipboard;
            ++m_widgetPasteCount;
            s->pasteInstances(m_widgetClipboard, m_widgetPasteCount);
            return;
        }

        if (m_widgetClipboard.isEmpty()) {
            s->pasteClipboard();
            return;
        }
        ++m_widgetPasteCount;
        s->pasteInstances(m_widgetClipboard, m_widgetPasteCount);
    }
}

void MainWindow::onDelete()
{
    if (m_bindingGraphView && m_tabWidget && m_tabWidget->currentWidget() == m_bindingGraphView) {
        m_bindingGraphView->deleteSelectedEdges();
        return;
    }
    if (auto *s = currentScene())
        s->deleteSelected();
}

void MainWindow::onSelectAll()
{
    if (auto *s = currentScene())
        for (QGraphicsItem *gi : s->items())
            gi->setSelected(true);
}

void MainWindow::onFind() {}

void MainWindow::onReplace() {}

void MainWindow::onAlignLeft()
{
    if (auto *s = currentScene())
        s->alignSelected(CanvasScene::AlignMode::Left);
}

void MainWindow::onAlignRight()
{
    if (auto *s = currentScene())
        s->alignSelected(CanvasScene::AlignMode::Right);
}

void MainWindow::onAlignTop()
{
    if (auto *s = currentScene())
        s->alignSelected(CanvasScene::AlignMode::Top);
}

void MainWindow::onAlignBottom()
{
    if (auto *s = currentScene())
        s->alignSelected(CanvasScene::AlignMode::Bottom);
}

void MainWindow::onAlignCenter()
{
    if (auto *s = currentScene())
        s->alignSelected(CanvasScene::AlignMode::Center);
}

void MainWindow::onBringToFront()
{
    if (auto *s = currentScene())
        s->changeSelectedZOrder(CanvasScene::ZOrderMode::BringToFront);
}

void MainWindow::onSendToBack()
{
    if (auto *s = currentScene())
        s->changeSelectedZOrder(CanvasScene::ZOrderMode::SendToBack);
}

void MainWindow::onBringForward()
{
    if (auto *s = currentScene())
        s->changeSelectedZOrder(CanvasScene::ZOrderMode::BringForward);
}

void MainWindow::onSendBackward()
{
    if (auto *s = currentScene())
        s->changeSelectedZOrder(CanvasScene::ZOrderMode::SendBackward);
}

void MainWindow::onGroup()
{
    if (auto *s = currentScene())
        s->groupSelected();
}

void MainWindow::onUngroup()
{
    if (auto *s = currentScene())
        s->ungroupSelected();
}

// ============================================================
// 视图
// ============================================================

void MainWindow::onToggleStandardToolbar(bool /*checked*/) {}

void MainWindow::onToggleDrawToolbar(bool /*checked*/) {}

void MainWindow::onToggleFormatToolbar(bool /*checked*/) {}

void MainWindow::onToggleProjectBrowser(bool /*checked*/) {}

void MainWindow::onTogglePropertyPanel(bool checked)
{
    if (m_propertyPanel)
        m_propertyPanel->setVisible(checked);
}

void MainWindow::onToggleOutputWindow(bool /*checked*/) {}

void MainWindow::onToggleStatusBar(bool /*checked*/) {}

void MainWindow::onOpenBindingMode()
{
    if (!m_projectOpen) return;
    syncSceneToProject();
    openBindingGraphTab();
    appendLog(tr("打开绑定模式画布"));
}

void MainWindow::onZoomIn() {}

void MainWindow::onZoomOut() {}

void MainWindow::onFitToWindow() {}

void MainWindow::onCustomZoom() {}

void MainWindow::onToggleGrid(bool /*checked*/) {}

void MainWindow::onGridSettings() {}

void MainWindow::onFullScreen() {}

// ============================================================
// 工程
// ============================================================

void MainWindow::onProjectInfo() {}

void MainWindow::onProjectConfig() {}

void MainWindow::onProjectPermission() {}

void MainWindow::onNewScreen()
{
    if (!m_screenManager) return;
    // 委托给图页管理器（它会发 addScreenRequested 信号）
    m_screenManager->onAddScreen();
}

void MainWindow::onDeleteScreen()
{
    if (!m_screenManager) return;
    m_screenManager->onDeleteScreen();
}

void MainWindow::onScreenProperties()
{
    ScreenTab *tab = currentScreenTab();
    if (!tab) return;

    ScreenData *sd = nullptr;
    for (ScreenData &s : m_project.screens) {
        if (s.id == tab->screenId()) { sd = &s; break; }
    }
    if (!sd) return;

    bool ok = false;
    const QString newName = QInputDialog::getText(
        this, tr("图页属性"), tr("图页名称："),
        QLineEdit::Normal, sd->name, &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    sd->name = newName.trimmed();
    // 刷新 tab 标题和管理器列表
    const int idx = m_tabWidget->indexOf(tab);
    if (idx >= 0)
        m_tabWidget->setTabText(idx,
            QStringLiteral("%1. %2").arg(sd->order + 1).arg(sd->name));
    if (m_screenManager)
        m_screenManager->setScreens(m_project.screens);
    if (m_projectTree)
        m_projectTree->setCurrentScreen(tab->scene(), sd->id, sd->name);
}

void MainWindow::onTagDictionary() {}

void MainWindow::onTagGroup() {}

void MainWindow::onImportTag() {}

void MainWindow::onExportTag() {}

void MainWindow::onAlarmGroupConfig() {}

void MainWindow::onAlarmLevelSettings() {}

void MainWindow::onAlarmRecordPolicy() {}

void MainWindow::onDataStorageConfig() {}

void MainWindow::onDataArchivePolicy() {}

void MainWindow::onRecipeManager() {}

void MainWindow::onGlobalScript() {}

void MainWindow::onEventScript() {}

void MainWindow::onUserList() {}

void MainWindow::onRoleManagement() {}

void MainWindow::onPermissionPolicy() {}

// ============================================================
// 绘图
// ============================================================

void MainWindow::onDrawLine() {}

void MainWindow::onDrawPolyline() {}

void MainWindow::onDrawRect() {}

void MainWindow::onDrawEllipse() {}

void MainWindow::onDrawPolygon() {}

void MainWindow::onDrawArc() {}

void MainWindow::onStaticText() {}

void MainWindow::onDynamicText() {}

void MainWindow::onNewLayer() {}

void MainWindow::onDeleteLayer() {}

void MainWindow::onLayerOrder() {}

void MainWindow::onLayerVisibility() {}

void MainWindow::onFillColor() {}

void MainWindow::onBorderColor() {}

void MainWindow::onLineStyle() {}

void MainWindow::onOpacity() {}

void MainWindow::onBackgroundSettings() {}

// ============================================================
// 组件
// ============================================================

void MainWindow::onCompButton() {}

void MainWindow::onCompIndicator() {}

void MainWindow::onCompInputBox() {}

void MainWindow::onCompLabel() {}

void MainWindow::onCompImage() {}

void MainWindow::onCompGauge() {}

void MainWindow::onCompProgressBar() {}

void MainWindow::onCompThermometer() {}

void MainWindow::onCompLevel() {}

void MainWindow::onCompRealtimeChart() {}

void MainWindow::onCompHistoryChart() {}

void MainWindow::onCompBarChart() {}

void MainWindow::onCompPieChart() {}

void MainWindow::onCompAlarmList() {}

void MainWindow::onCompAlarmBar() {}

void MainWindow::onCompDataReport() {}

void MainWindow::onCompPrintReport() {}

void MainWindow::onSystemLibrary() {}

void MainWindow::onUserLibrary() {}

void MainWindow::onImportLibrary() {}

void MainWindow::onCustomComponent() {}

// ============================================================
// 通信
// ============================================================

void MainWindow::onAddDriver() {}

void MainWindow::onRemoveDriver() {}

void MainWindow::onDriverConfig() {}

void MainWindow::onAddDevice() {}

void MainWindow::onDeviceProperties() {}

void MainWindow::onDeviceDiagnose() {}

void MainWindow::onProtocolOpcUa() {}

void MainWindow::onProtocolOpcDa() {}

void MainWindow::onProtocolModbus() {}

void MainWindow::onProtocolProfibus() {}

void MainWindow::onProtocolEthernetIp() {}

void MainWindow::onProtocolMqtt() {}

void MainWindow::onProtocolCustom() {}

void MainWindow::onSqlDatabase() {}

void MainWindow::onTimeSeriesDatabase() {}

void MainWindow::onCommStatusMonitor() {}

void MainWindow::onCommLog() {}

// ============================================================
// 运行
// ============================================================

void MainWindow::onStartRun() {
    onStartSimulate();
}

void MainWindow::onStopRun()
{
    if (!m_simulatorProcess || m_simulatorProcess->state() == QProcess::NotRunning) {
        appendLog(tr("仿真器未运行"));
        return;
    }

    appendLog(tr("正在停止仿真器..."));
    m_simulatorProcess->terminate();
    if (!m_simulatorProcess->waitForFinished(3000)) {
        appendLog(tr("仿真器未响应，正在强制结束..."));
        m_simulatorProcess->kill();
        if (!m_simulatorProcess->waitForFinished(3000)) {
            QMessageBox::warning(this, tr("停止运行"),
                                 tr("无法停止仿真器：%1")
                                     .arg(m_simulatorProcess->errorString()));
            return;
        }
    }

    appendSimulatorOutput(tr("输出"), m_simulatorProcess->readAllStandardOutput(), &m_simulatorStdoutBuffer);
    appendSimulatorOutput(tr("错误"), m_simulatorProcess->readAllStandardError(), &m_simulatorStderrBuffer);
    flushSimulatorOutput(tr("输出"), &m_simulatorStdoutBuffer);
    flushSimulatorOutput(tr("错误"), &m_simulatorStderrBuffer);
    appendLog(tr("仿真器已停止"));
}

void MainWindow::onPauseRun() {}
void MainWindow::onCompileProject()
{
    if (!m_projectOpen) {
        QMessageBox::information(this, tr("启动编译"),
            tr("请先打开或新建一个工程。"));
        return;
    }
    if (m_projectFilePath.isEmpty()) {
        QMessageBox::information(this, tr("启动编译"),
            tr("当前工程尚未保存，请先保存到工程目录后再启动编译。"));
        return;
    }

    // 1) 同步当前画布到工程数据并落盘
    syncSceneToProject();
    QString err;
    if (!ProjectManager::saveToFile(m_project, m_projectFilePath, &err)) {
        QMessageBox::warning(this, tr("启动编译"),
            tr("保存工程失败：%1").arg(err));
        return;
    }

    // 2) 推断工程目录与产物路径：<projectDir>/<name>.lua
    const QFileInfo jsonFi(m_projectFilePath);
    const QString projectDir = jsonFi.absolutePath();
    const QString projectName =
        m_project.name.isEmpty()
        ? jsonFi.completeBaseName().section(QLatin1Char('.'), 0, 0)
        : m_project.name;
    const QString luaPath = projectDir + QLatin1Char('/')
        + ProjectManager::projectLuaFileName(projectName);

    // 3) 部署运行时。顶层 runtime.lua 始终刷新，common/widgets 默认不覆盖用户修改。
    QString depErr;
    if (!ProjectManager::deployRuntime(projectDir, false, &depErr)) {
        QMessageBox::warning(this, tr("启动编译"),
            tr("部署运行时失败：%1").arg(depErr));
        return;
    }

    // 4) 编译 JSON -> Lua
    if (!ProjectManager::compileFileToLua(m_projectFilePath, luaPath, &err)) {
        QMessageBox::warning(this, tr("启动编译"),
            tr("编译为 Lua 失败：%1").arg(err));
        return;
    }
}

void MainWindow::appendSimulatorOutput(const QString &channelName, const QByteArray &data, QString *buffer)
{
    if (data.isEmpty() || !buffer) return;

    QString text = QString::fromUtf8(data);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLocal8Bit(data);
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    buffer->append(text);
    int lineEnd = buffer->indexOf(QLatin1Char('\n'));
    while (lineEnd >= 0) {
        const QString line = buffer->left(lineEnd);
        buffer->remove(0, lineEnd + 1);
        appendLog(tr("仿真器%1：%2").arg(channelName, line));
        lineEnd = buffer->indexOf(QLatin1Char('\n'));
    }
}

void MainWindow::flushSimulatorOutput(const QString &channelName, QString *buffer)
{
    if (!buffer || buffer->isEmpty()) return;
    appendLog(tr("仿真器%1：%2").arg(channelName, *buffer));
    buffer->clear();
}

void MainWindow::onStartSimulate()
{
    if (!m_projectOpen) {
        QMessageBox::information(this, tr("启动仿真"),
                                 tr("请先打开或新建一个工程。"));
        return;
    }
    if (m_projectFilePath.isEmpty()) {
        QMessageBox::information(this, tr("启动仿真"),
                                 tr("当前工程尚未保存，请先保存到工程目录后再启动仿真。"));
        return;
    }

    // 1) 同步当前画布到工程数据并落盘
    syncSceneToProject();
    QString err;
    if (!ProjectManager::saveToFile(m_project, m_projectFilePath, &err)) {
        QMessageBox::warning(this, tr("启动仿真"),
                             tr("保存工程失败：%1").arg(err));
        return;
    }

    // 2) 推断工程目录与产物路径：<projectDir>/<name>.lua
    const QFileInfo jsonFi(m_projectFilePath);
    const QString projectDir = jsonFi.absolutePath();
    const QString projectName =
        m_project.name.isEmpty()
            ? jsonFi.completeBaseName().section(QLatin1Char('.'), 0, 0)
            : m_project.name;
    const QString luaPath = projectDir + QLatin1Char('/')
                            + ProjectManager::projectLuaFileName(projectName);

    // 3) 部署运行时。顶层 runtime.lua 始终刷新，common/widgets 默认不覆盖用户修改。
    QString depErr;
    if (!ProjectManager::deployRuntime(projectDir, false, &depErr)) {
        QMessageBox::warning(this, tr("启动仿真"),
                             tr("部署运行时失败：%1").arg(depErr));
        return;
    }

    // 4) 编译 JSON -> Lua
    if (!ProjectManager::compileFileToLua(m_projectFilePath, luaPath, &err)) {
        QMessageBox::warning(this, tr("启动仿真"),
                             tr("编译为 Lua 失败：%1").arg(err));
        return;
    }

    // 5) 拉起仿真子进程：<appDir>/QtLvglSimu(.exe)（与 LMViCtrl 同目录）
    const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString exeName = QStringLiteral("LMViCtrlSimulator.exe");
#else
    const QString exeName = QStringLiteral("LMViCtrlSimulator");
#endif
    const QString simuExe = QDir(appDir).filePath(exeName);
    if (!QFile::exists(simuExe)) {
        QMessageBox::warning(this, tr("启动仿真"),
                             tr("未找到仿真器可执行文件：%1").arg(simuExe));
        return;
    }

    const QStringList args = {
        luaPath,
        QStringLiteral("--width"),  QString::number(m_project.target.width),
        QStringLiteral("--height"), QString::number(m_project.target.height),
        QStringLiteral("--title"),  projectName,
    };

    if (!m_simulatorProcess) {
        m_simulatorProcess = new QProcess(this);
        connect(m_simulatorProcess, &QProcess::readyReadStandardOutput, this, [this]() {
            appendSimulatorOutput(tr("输出"),
                                  m_simulatorProcess->readAllStandardOutput(),
                                  &m_simulatorStdoutBuffer);
        });
        connect(m_simulatorProcess, &QProcess::readyReadStandardError, this, [this]() {
            appendSimulatorOutput(tr("错误"),
                                  m_simulatorProcess->readAllStandardError(),
                                  &m_simulatorStderrBuffer);
        });
        connect(m_simulatorProcess, &QProcess::finished, this,
                [this](int exitCode, QProcess::ExitStatus exitStatus) {
                    appendSimulatorOutput(tr("输出"),
                                          m_simulatorProcess->readAllStandardOutput(),
                                          &m_simulatorStdoutBuffer);
                    appendSimulatorOutput(tr("错误"),
                                          m_simulatorProcess->readAllStandardError(),
                                          &m_simulatorStderrBuffer);
                    flushSimulatorOutput(tr("输出"), &m_simulatorStdoutBuffer);
                    flushSimulatorOutput(tr("错误"), &m_simulatorStderrBuffer);
                    appendLog(tr("仿真器已退出，退出码：%1，状态：%2")
                                  .arg(exitCode)
                                  .arg(exitStatus == QProcess::NormalExit
                                           ? tr("正常退出")
                                           : tr("异常退出")));
                });
    }

    if (m_simulatorProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, tr("启动仿真"),
                                 tr("仿真器已在运行。"));
        return;
    }

    m_simulatorStdoutBuffer.clear();
    m_simulatorStderrBuffer.clear();
    if (m_logDock) {
        m_logDock->show();
        m_logDock->raise();
    }
    m_simulatorProcess->setWorkingDirectory(projectDir);
    m_simulatorProcess->start(simuExe, args);
    if (!m_simulatorProcess->waitForStarted(3000)) {
        appendSimulatorOutput(tr("输出"), m_simulatorProcess->readAllStandardOutput(), &m_simulatorStdoutBuffer);
        appendSimulatorOutput(tr("错误"), m_simulatorProcess->readAllStandardError(), &m_simulatorStderrBuffer);
        flushSimulatorOutput(tr("输出"), &m_simulatorStdoutBuffer);
        flushSimulatorOutput(tr("错误"), &m_simulatorStderrBuffer);
        QMessageBox::warning(this, tr("启动仿真"),
                             tr("无法启动仿真器：%1\n%2")
                                 .arg(simuExe, m_simulatorProcess->errorString()));
        return;
    }

    appendLog(tr("仿真器已启动：%1").arg(simuExe));
}

void MainWindow::onSimulateVarSetting() {}

void MainWindow::onScriptDebug() {}

void MainWindow::onVariableMonitor() {}

void MainWindow::onBreakpointManager() {}

void MainWindow::onRunParameterSettings() {}

// ============================================================
// 工具
// ============================================================

void MainWindow::onGeneralSettings() {}

void MainWindow::onLanguageSettings()
{
    const QStringList languages = {tr("简体中文"), tr("English")};
    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("语言设置"), tr("选择界面语言："),
        languages, 0, false, &ok);
    if (!ok)
        return;

    switchLanguage(choice == tr("English") ? QStringLiteral("en") : QStringLiteral("zh_CN"));
}

void MainWindow::onSwitchToZhCN()
{
    switchLanguage(QStringLiteral("zh_CN"));
}

void MainWindow::onSwitchToEn()
{
    switchLanguage(QStringLiteral("en"));
}

void MainWindow::switchLanguage(const QString &locale)
{
    static QTranslator *s_translator = nullptr;

    const QString qmFile = QStringLiteral(":/i18n/LMViCtrl_") + locale;

    if (s_translator) {
        qApp->removeTranslator(s_translator);
        delete s_translator;
        s_translator = nullptr;
    }

    s_translator = new QTranslator(qApp);
    if (s_translator->load(qmFile)) {
        qApp->installTranslator(s_translator);
    }

    // 重建菜单以刷新文本
    setupMenus();
}

void MainWindow::onShortcutSettings() {}

void MainWindow::onThemeSettings() {}

void MainWindow::onScriptEditor() {}

void MainWindow::onFormulaEditor() {}

void MainWindow::onColorEditor() {}

void MainWindow::onViewLicense() {}

void MainWindow::onActivateLicense() {}

void MainWindow::onInstallPlugin() {}

void MainWindow::onUninstallPlugin() {}

void MainWindow::onSystemLog() {}

void MainWindow::onBackupProject() {}

void MainWindow::onRestoreProject() {}

// ============================================================
// 窗口
// ============================================================

void MainWindow::onNewWindow() {}

void MainWindow::onCascade() {}

void MainWindow::onTileHorizontal() {}

void MainWindow::onTileVertical() {}

void MainWindow::onCloseAllWindows() {}

// ============================================================
// 帮助
// ============================================================

void MainWindow::onHelpDocument() {}

void MainWindow::onQuickStartGuide() {}

void MainWindow::onVideoTutorial() {}

void MainWindow::onSampleProject() {}

void MainWindow::onCheckUpdate() {}

void MainWindow::onOnlineSupport() {}

void MainWindow::onSubmitFeedback() {}

void MainWindow::onVersionInfo() {}

void MainWindow::onLicenseInfo() {}

void MainWindow::onTechnicalSupport() {}

// ============================================================
// 最近使用工程
// ============================================================

static QString recentProjectsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("recent_projects.txt"));
}

void MainWindow::loadRecentProjects()
{
    m_recentProjects.clear();
    QFile f(recentProjectsFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty() && QFileInfo::exists(line))
            m_recentProjects.append(line);
    }
    m_recentProjects = m_recentProjects.mid(0, kMaxRecentProjects);
}

void MainWindow::saveRecentProjects()
{
    QFile f(recentProjectsFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    QTextStream out(&f);
    for (const QString &p : std::as_const(m_recentProjects))
        out << p << "\n";
}

void MainWindow::addRecentProject(const QString &path)
{
    if (path.isEmpty()) return;
    const QString canonical = QFileInfo(path).absoluteFilePath();
    m_recentProjects.removeAll(canonical);
    m_recentProjects.prepend(canonical);
    if (m_recentProjects.size() > kMaxRecentProjects)
        m_recentProjects = m_recentProjects.mid(0, kMaxRecentProjects);
    saveRecentProjects();
    updateRecentMenu();
}

void MainWindow::updateRecentMenu()
{
    if (!m_recentMenu) return;
    m_recentMenu->clear();
    if (m_recentProjects.isEmpty()) {
        QAction *a = m_recentMenu->addAction(tr("(空)"));
        a->setEnabled(false);
        return;
    }
    for (const QString &path : std::as_const(m_recentProjects)) {
        const QString label = QFileInfo(path).fileName()
                              + QStringLiteral("  [")
                              + QDir::toNativeSeparators(path)
                              + QStringLiteral("]");
        QAction *a = m_recentMenu->addAction(label);
        connect(a, &QAction::triggered, this, [this, path]() {
            if (!QFileInfo::exists(path)) {
                QMessageBox::warning(this, tr("文件不存在"),
                                     tr("工程文件已移动或删除：\n%1").arg(path));
                m_recentProjects.removeAll(path);
                saveRecentProjects();
                updateRecentMenu();
                return;
            }
            if (!maybeSaveCurrent()) return;
            ProjectData p;
            QString err;
            if (!ProjectManager::loadFromFile(&p, path, &err)) {
                QMessageBox::warning(this, tr("打开失败"),
                                     tr("无法打开工程：%1").arg(err));
                return;
            }
            m_project         = p;
            m_projectFilePath = path;
            applyProjectToTabs();
            setProjectOpen(true);
            addRecentProject(path);
        });
    }
    m_recentMenu->addSeparator();
    QAction *clearAction = m_recentMenu->addAction(tr("清除最近记录"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        m_recentProjects.clear();
        saveRecentProjects();
        updateRecentMenu();
    });
}

// ============================================================
// 属性面板辅助
// ============================================================

QString MainWindow::generateUniqueWidgetName(const QString &baseName) const
{
    QString base = baseName.trimmed();
    if (base.isEmpty()) base = QStringLiteral("widget");

    QString cleaned;
    for (QChar c : base) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_')) cleaned.append(c);
        else cleaned.append(QLatin1Char('_'));
    }
    if (cleaned.isEmpty()) cleaned = QStringLiteral("widget");

    // 收集工程内所有已使用的名字（已打开的 tab 用 scene 数据，未打开的用 ScreenData）
    QSet<QString> used;
    for (const ScreenData &s : m_project.screens) {
        ScreenTab *tab = m_openTabs.value(s.id, nullptr);
        if (tab) {
            for (const auto &inst : tab->scene()->allInstances())
                if (!inst.name.isEmpty()) used.insert(inst.name);
        } else {
            for (const auto &inst : s.widgets)
                if (!inst.name.isEmpty()) used.insert(inst.name);
        }
    }

    int n = 1;
    QString cand;
    do { cand = QStringLiteral("%1_%2").arg(cleaned).arg(n++); }
    while (used.contains(cand));
    return cand;
}

void MainWindow::installSceneNameGenerator(CanvasScene *scene)
{
    if (!scene) return;
    scene->setNameGenerator([this](const QString &base) {
        return this->generateUniqueWidgetName(base);
    });
}

void MainWindow::ensureInstanceNamesAssigned()
{
    QSet<QString> used;
    for (const ScreenData &s : m_project.screens)
        for (const auto &inst : s.widgets)
            if (!inst.name.isEmpty()) used.insert(inst.name);

    QHash<QString, QString> idToBase;
    if (m_widgetToolbox) {
        for (const auto &m : m_widgetToolbox->widgetMetas())
            idToBase.insert(m.id, m.name);
    }

    auto nextName = [&used](QString base) {
        if (base.isEmpty()) base = QStringLiteral("widget");
        int n = 1;
        QString cand;
        do { cand = QStringLiteral("%1_%2").arg(base).arg(n++); }
        while (used.contains(cand));
        used.insert(cand);
        return cand;
    };

    for (ScreenData &s : m_project.screens) {
        for (WidgetInstance &inst : s.widgets) {
            if (!inst.name.isEmpty()) continue;
            QString base = idToBase.value(inst.widgetId, inst.widgetId);
            inst.name = nextName(base);
        }
    }
}

// ============================================================
// 图页 Undo / Redo 支持
// ============================================================

void MainWindow::onScreenAddRequested(const QString &name)
{
    if (!m_projectUndoStack) return;

    ScreenData s;
    s.id    = QUuid::createUuid().toString(QUuid::WithoutBraces);
    s.name  = name;
    s.order = m_project.screens.size();
    m_projectUndoStack->push(new AddScreenCommand(this, s));
}

void MainWindow::onScreenDeleteRequested(const QString &screenId)
{
    if (!m_projectUndoStack) return;
    if (m_project.screens.size() <= 1) return;   // 只有一个图页时禁止删除
    m_projectUndoStack->push(new RemoveScreenCommand(this, screenId));
}

void MainWindow::onDataVariableAddRequested(const QString &name, const QString &type, const QVariant &initialValue)
{
    if (!m_projectUndoStack) return;

    QSet<QString> used;
    for (const DataVariable &variable : std::as_const(m_project.dataVariables))
        used.insert(variable.name);

    const QString baseName = normalizedDataVariableName(name);
    QString candidate = baseName;
    int suffix = 2;
    while (used.contains(candidate))
        candidate = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);

    DataVariable variable;
    variable.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    variable.name = candidate;
    variable.type = type.isEmpty() ? QStringLiteral("number") : type;
    variable.value = initialValue.isValid() ? initialValue : defaultValueForDataVariableType(variable.type);
    variable.defaultValue = variable.value;
    variable.description = tr("项目级数据变量");

    m_projectUndoStack->push(new AddDataVariableCommand(this, variable, m_project.dataVariables.size()));
}

void MainWindow::onDataVariableRemoveRequested(const QString &id)
{
    if (!m_projectUndoStack || id.isEmpty()) return;
    m_projectUndoStack->push(new RemoveDataVariableCommand(this, id));
}

void MainWindow::cmdAddScreen(const ScreenData &screen, int order)
{
    const int insertIdx = qBound(0, order, m_project.screens.size());
    m_project.screens.insert(insertIdx, screen);
    // 重建 order 字段
    for (int i = 0; i < m_project.screens.size(); ++i)
        m_project.screens[i].order = i;
    if (m_screenManager)
        m_screenManager->setScreens(m_project.screens);
    openScreenTab(screen.id);
}

void MainWindow::cmdRemoveScreen(const QString &screenId)
{
    closeScreenTab(screenId);
    for (int i = 0; i < m_project.screens.size(); ++i) {
        if (m_project.screens[i].id == screenId) {
            m_project.screens.removeAt(i);
            break;
        }
    }
    for (int i = 0; i < m_project.screens.size(); ++i)
        m_project.screens[i].order = i;
    if (m_screenManager)
        m_screenManager->setScreens(m_project.screens);
    // 若没有打开的 tab，自动打开第一个图页（若仍有）
    if (m_tabWidget && m_tabWidget->count() == 0 && !m_project.screens.isEmpty())
        openScreenTab(m_project.screens.first().id);
}

ScreenData MainWindow::snapshotScreen(const QString &screenId) const
{
    // 若该图页对应的 tab 已打开，先把画布上的最新内容同步出来
    for (const ScreenData &s : m_project.screens) {
        if (s.id != screenId) continue;
        ScreenData copy = s;
        if (ScreenTab *tab = m_openTabs.value(screenId, nullptr))
            tab->syncToScreen(copy);
        return copy;
    }
    return ScreenData{};
}

void MainWindow::cmdAddDataVariable(const DataVariable &variable, int order)
{
    for (const DataVariable &existing : std::as_const(m_project.dataVariables)) {
        if (existing.id == variable.id)
            return;
    }
    const int insertIdx = qBound(0, order, m_project.dataVariables.size());
    m_project.dataVariables.insert(insertIdx, variable);
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (m_projectTree)
        m_projectTree->setProjectData(&m_project);
}

void MainWindow::cmdRemoveDataVariable(const QString &id)
{
    for (int i = 0; i < m_project.dataVariables.size(); ++i) {
        if (m_project.dataVariables[i].id == id) {
            m_project.dataVariables.removeAt(i);
            break;
        }
    }
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (m_projectTree)
        m_projectTree->setProjectData(&m_project);
}

DataVariable MainWindow::snapshotDataVariable(const QString &id) const
{
    for (const DataVariable &variable : m_project.dataVariables) {
        if (variable.id == id)
            return variable;
    }
    return DataVariable{};
}

int MainWindow::dataVariableOrder(const QString &id) const
{
    for (int i = 0; i < m_project.dataVariables.size(); ++i) {
        if (m_project.dataVariables[i].id == id)
            return i;
    }
    return m_project.dataVariables.size();
}

// ============================================================
// 跨多 stack 的 Undo/Redo 链式调度
// ============================================================

void MainWindow::registerUndoStack(QUndoStack *stack, const QString &screenId)
{
    if (!stack || m_stackLastIndex.contains(stack)) return;
    m_stackLastIndex.insert(stack, stack->index());
    if (!screenId.isEmpty())
        m_stackScreenIds.insert(stack, screenId);

    connect(stack, &QUndoStack::indexChanged, this, [this, stack](int newIdx) {
        onAnyStackIndexChanged(stack, newIdx);
    });
    connect(stack, &QObject::destroyed, this, [this, stack]() {
        m_stackLastIndex.remove(stack);
        m_stackScreenIds.remove(stack);
        m_undoChain.removeAll(stack);
        m_redoChain.removeAll(stack);
    });
}

bool MainWindow::activateUndoStackPage(QUndoStack *stack)
{
    const QString screenId = m_stackScreenIds.value(stack);
    if (screenId.isEmpty())
        return true;

    ScreenTab *tab = m_openTabs.value(screenId, nullptr);
    if (!tab)
        return false;

    if (m_tabWidget && m_tabWidget->currentWidget() != tab)
        m_tabWidget->setCurrentWidget(tab);
    if (m_propertyPanel)
        m_propertyPanel->setCurrentScene(tab->scene());
    return true;
}

void MainWindow::onAnyStackIndexChanged(QUndoStack *stack, int newIdx)
{
    const int prev = m_stackLastIndex.value(stack, 0);
    if (newIdx > prev) {
        int diff = newIdx - prev;
        if (m_inRedoOp && !m_redoChain.isEmpty() && m_redoChain.last() == stack) {
            // 一次 redo()：从 redoChain 顶移回 undoChain 顶
            m_redoChain.removeLast();
            m_undoChain.append(stack);
            --diff;
        }
        for (int i = 0; i < diff; ++i)
            m_undoChain.append(stack);
        if (!m_inRedoOp) {
            // 任意新 push 后，原有 redo 历史失效
            m_redoChain.clear();
        }
    } else if (newIdx < prev) {
        int diff = prev - newIdx;
        for (int i = 0; i < diff; ++i) {
            for (int j = m_undoChain.size() - 1; j >= 0; --j) {
                if (m_undoChain[j] == stack) {
                    m_undoChain.removeAt(j);
                    m_redoChain.append(stack);
                    break;
                }
            }
        }
    }
    m_stackLastIndex[stack] = newIdx;
}

void MainWindow::resetUndoChains(bool keepProjectStackRegistered)
{
    // 断开所有已跟踪的 stack，防止 MainWindow 析构时 destroyed 回调访问已释放的成员
    for (auto it = m_stackLastIndex.keyBegin(); it != m_stackLastIndex.keyEnd(); ++it) {
        if (*it)
            (*it)->clear();
        QObject::disconnect(*it, nullptr, this, nullptr);
    }

    m_undoChain.clear();
    m_redoChain.clear();
    m_stackLastIndex.clear();
    m_stackScreenIds.clear();

    if (keepProjectStackRegistered && m_projectUndoStack) {
        m_projectUndoStack->clear();
        registerUndoStack(m_projectUndoStack);
    }
}
