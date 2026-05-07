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
#include "screentab.h"
#include "screenmanagerdock.h"
#include "welcomewidget.h"
#include "widgettoolbox.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsItem>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QTranslator>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

namespace {
constexpr const char *kProjectFilter =
    "QtLvglDesigner Project (*.qlvgl.json *.json *.qlproj)";

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
}

void MainWindow::syncSceneToProject()
{
    // 将所有已打开的图页同步回 m_project
    for (ScreenData &screen : m_project.screens) {
        if (ScreenTab *tab = m_openTabs.value(screen.id, nullptr))
            tab->syncToScreen(screen);
    }
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
}

void MainWindow::applyProjectToTabs()
{
    if (!m_tabWidget) return;
    m_tabWidget->clear();
    m_openTabs.clear();
    resetUndoChains();

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

    const QString tabTitle = QStringLiteral("%1. %2").arg(sd->order + 1).arg(sd->name);
    m_tabWidget->addTab(tab, tabTitle);
    m_tabWidget->setCurrentWidget(tab);
    m_openTabs.insert(screenId, tab);

    if (m_propertyPanel)
        m_propertyPanel->setCurrentScene(tab->scene());
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
}

void MainWindow::onTabCloseRequested(int index)
{
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
    QString title = QStringLiteral("QtLvglDesigner");
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

    // 3) 把 designer 自带的 lua 运行时（runtime.lua / common / widgets）
    //    拷贝到工程目录，供后续编译产物 require 与仿真器加载
    QString depErr;
    if (!ProjectManager::deployRuntime(projectDir, /*overwriteWidgets=*/false, &depErr)) {
        QMessageBox::warning(this, tr("部署运行时失败"),
                             tr("拷贝 lua 运行时到工程目录失败：%1\n"
                                "仿真功能在此工程中可能不可用。").arg(depErr));
    }

    setProjectOpen(true);
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

    QString defaultName = m_project.name.isEmpty() ? QStringLiteral("project")
                                                   : m_project.name;
    if (!m_projectFilePath.isEmpty())
        defaultName = m_projectFilePath;
    else
        defaultName = QDir::current().absoluteFilePath(defaultName + ".json");

    const QString path = QFileDialog::getSaveFileName(
        this, tr("另存为"), defaultName, tr(kProjectFilter));
    if (path.isEmpty()) return;

    saveProjectToPath(path);
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
    if (auto *s = currentScene()) { s->copySelected(); s->deleteSelected(); }
}

void MainWindow::onCopy() { if (auto *s = currentScene()) s->copySelected(); }

void MainWindow::onPaste() { if (auto *s = currentScene()) s->pasteClipboard(); }

void MainWindow::onDelete() { if (auto *s = currentScene()) s->deleteSelected(); }

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

void MainWindow::onGroup() {}

void MainWindow::onUngroup() {}

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

    // 5) 拉起仿真子进程：<appDir>/QtLvglSimu(.exe)（与 designer 同目录）
    const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString exeName = QStringLiteral("simulator.exe");
#else
    const QString exeName = QStringLiteral("simulator");
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
        connect(m_simulatorProcess, &QProcess::finished, this,
                [this](int exitCode, QProcess::ExitStatus exitStatus) {
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

    m_simulatorProcess->setWorkingDirectory(projectDir);
    m_simulatorProcess->start(simuExe, args);
    if (!m_simulatorProcess->waitForStarted(3000)) {
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

    const QString qmFile = QStringLiteral(":/i18n/designer_") + locale;

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
