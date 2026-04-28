/**
 * @file mainwindow_actions.cpp
 * @brief 菜单栏所有 Action 的实现函数
 *
 * 每个函数对应一个菜单项，在此文件中集中编写具体业务逻辑。
 * 当前均为空实现（接口占位），后续按需填充。
 */

#include "mainwindow.h"
#include "canvasscene.h"
#include "projectmanager.h"
#include "projectpropertiesdialog.h"
#include "screentab.h"
#include "screenmanagerdock.h"
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
#include <QTabWidget>
#include <QTextStream>
#include <QTranslator>
#include <QUndoStack>
#include <QUuid>

namespace {
constexpr const char *kProjectFilter = "QtLvglDesigner Project (*.json *.qlproj)";
}

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

    const QString tabTitle = QStringLiteral("%1. %2").arg(sd->order + 1).arg(sd->name);
    m_tabWidget->addTab(tab, tabTitle);
    m_tabWidget->setCurrentWidget(tab);
    m_openTabs.insert(screenId, tab);
}

void MainWindow::closeScreenTab(const QString &screenId)
{
    ScreenTab *tab = m_openTabs.value(screenId, nullptr);
    if (!tab) return;
    // 先同步数据再关闭
    for (ScreenData &s : m_project.screens) {
        if (s.id == screenId) { tab->syncToScreen(s); break; }
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
    for (const ScreenData &s : std::as_const(m_project.screens))
        widgetMap.insert(s.id, s.widgets);

    m_project.screens.clear();
    for (ScreenData s : updatedScreens) {
        s.widgets = widgetMap.value(s.id);
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
    if (!maybeSaveCurrent()) return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("新建工程"), tr("工程名称："),
        QLineEdit::Normal, tr("NewProject"), &ok);
    if (!ok) return;

    resetProject();
    if (!name.trimmed().isEmpty())
        m_project.name = name.trimmed();
    setProjectOpen(true);
    // 新建工程尚未保存，无路径，不记录最近工程
}

void MainWindow::onOpenProject()
{
    if (!maybeSaveCurrent()) return;

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

    resetProject();
    setProjectOpen(false);
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

    ProjectPropertiesDialog dlg(m_project, this);
    if (dlg.exec() != QDialog::Accepted) return;

    const ProjectData newData = dlg.projectData();

    const bool resolutionChanged =
        (newData.target.width  != m_project.target.width ||
         newData.target.height != m_project.target.height);

    m_project = newData;
    m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);

    if (resolutionChanged) {
        for (ScreenTab *tab : std::as_const(m_openTabs))
            tab->scene()->setCanvasSize(m_project.target.width, m_project.target.height);
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

void MainWindow::onUndo() { if (auto *s = currentScene()) s->undoStack()->undo(); }

void MainWindow::onRedo() { if (auto *s = currentScene()) s->undoStack()->redo(); }

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

void MainWindow::onAlignLeft() {}

void MainWindow::onAlignRight() {}

void MainWindow::onAlignTop() {}

void MainWindow::onAlignBottom() {}

void MainWindow::onAlignCenter() {}

void MainWindow::onGroup() {}

void MainWindow::onUngroup() {}

// ============================================================
// 视图
// ============================================================

void MainWindow::onToggleStandardToolbar(bool /*checked*/) {}

void MainWindow::onToggleDrawToolbar(bool /*checked*/) {}

void MainWindow::onToggleFormatToolbar(bool /*checked*/) {}

void MainWindow::onToggleProjectBrowser(bool /*checked*/) {}

void MainWindow::onTogglePropertyPanel(bool /*checked*/) {}

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
    // 委托给图页管理器（它会发 screensChanged + openScreenRequested 信号）
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

void MainWindow::onStartRun() {}

void MainWindow::onStopRun() {}

void MainWindow::onPauseRun() {}

void MainWindow::onStartSimulate() {}

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
