#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QHash>
#include "widgemeta.h"

class WidgetToolbox;
class CanvasScene;
class CanvasView;
class ScreenTab;
class ScreenManagerDock;
class ProjectTreeDock;
class WelcomeWidget;
class PropertyPanelDock;
class EventPanelDock;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QMenu;
class QAction;
class QByteArray;
class QDockWidget;
class QPlainTextEdit;
class QProcess;
class QTabWidget;
class QStackedWidget;
class QToolBar;
class QUndoStack;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void applyDarkTheme();
    void switchLanguage(const QString &locale);
    void setupMenus();
    void setupFileMenu();
    void setupEditMenu();
    void setupRunMenu();
    void setupLanguageMenu();
    
    void setupToolBars();
    void setupFileToolBar();
    void setupEditToolBar();
    void setupRunToolBar();

    // 工具函数：创建带快捷键的 QAction 并添加到菜单
    QAction *addAction(QMenu *menu,
                       const QString &text,
                       const QKeySequence &shortcut = QKeySequence(),
                       bool checkable = false,
                       bool checked = false);
    void setupLogDock();
    void appendLog(const QString &message);
    void connectSceneLogging(CanvasScene *scene, const QString &screenId);
    void appendSimulatorOutput(const QString &channelName, const QByteArray &data, QString *buffer);
    void flushSimulatorOutput(const QString &channelName, QString *buffer);

private slots:
    // ===== 文件 =====
    void onNewProject();
    void onOpenProject();
    void onCloseProject();
    void onSave();
    void onSaveAs();
    void onExportZip();
    void onExportXml();
    void onExportReport();
    void onImportProject();
    void onProjectProperties();
    void onExit();

    // ===== 编辑 =====
    void onUndo();
    void onRedo();
    void onCut();
    void onCopy();
    void onPaste();
    void onDelete();
    void onSelectAll();
    void onFind();
    void onReplace();
    void onAlignLeft();
    void onAlignRight();
    void onAlignTop();
    void onAlignBottom();
    void onAlignCenter();
    void onBringToFront();
    void onSendToBack();
    void onBringForward();
    void onSendBackward();
    void onGroup();
    void onUngroup();

    // ===== 视图 =====
    void onToggleStandardToolbar(bool checked);
    void onToggleDrawToolbar(bool checked);
    void onToggleFormatToolbar(bool checked);
    void onToggleProjectBrowser(bool checked);
    void onTogglePropertyPanel(bool checked);
    void onToggleOutputWindow(bool checked);
    void onToggleStatusBar(bool checked);
    void onZoomIn();
    void onZoomOut();
    void onFitToWindow();
    void onCustomZoom();
    void onToggleGrid(bool checked);
    void onGridSettings();
    void onFullScreen();

    // ===== 工程 =====
    void onProjectInfo();
    void onProjectConfig();
    void onProjectPermission();
    void onNewScreen();
    void onDeleteScreen();
    void onScreenProperties();
    void onTagDictionary();
    void onTagGroup();
    void onImportTag();
    void onExportTag();
    void onAlarmGroupConfig();
    void onAlarmLevelSettings();
    void onAlarmRecordPolicy();
    void onDataStorageConfig();
    void onDataArchivePolicy();
    void onRecipeManager();
    void onGlobalScript();
    void onEventScript();
    void onUserList();
    void onRoleManagement();
    void onPermissionPolicy();

    // ===== 绘图 =====
    void onDrawLine();
    void onDrawPolyline();
    void onDrawRect();
    void onDrawEllipse();
    void onDrawPolygon();
    void onDrawArc();
    void onStaticText();
    void onDynamicText();
    void onNewLayer();
    void onDeleteLayer();
    void onLayerOrder();
    void onLayerVisibility();
    void onFillColor();
    void onBorderColor();
    void onLineStyle();
    void onOpacity();
    void onBackgroundSettings();

    // ===== 组件 =====
    void onCompButton();
    void onCompIndicator();
    void onCompInputBox();
    void onCompLabel();
    void onCompImage();
    void onCompGauge();
    void onCompProgressBar();
    void onCompThermometer();
    void onCompLevel();
    void onCompRealtimeChart();
    void onCompHistoryChart();
    void onCompBarChart();
    void onCompPieChart();
    void onCompAlarmList();
    void onCompAlarmBar();
    void onCompDataReport();
    void onCompPrintReport();
    void onSystemLibrary();
    void onUserLibrary();
    void onImportLibrary();
    void onCustomComponent();

    // ===== 通信 =====
    void onAddDriver();
    void onRemoveDriver();
    void onDriverConfig();
    void onAddDevice();
    void onDeviceProperties();
    void onDeviceDiagnose();
    void onProtocolOpcUa();
    void onProtocolOpcDa();
    void onProtocolModbus();
    void onProtocolProfibus();
    void onProtocolEthernetIp();
    void onProtocolMqtt();
    void onProtocolCustom();
    void onSqlDatabase();
    void onTimeSeriesDatabase();
    void onCommStatusMonitor();
    void onCommLog();

    // ===== 运行 =====
    void onStartRun();
    void onStopRun();
    void onPauseRun();
	void onCompileProject();
    void onStartSimulate();
    void onSimulateVarSetting();
    void onScriptDebug();
    void onVariableMonitor();
    void onBreakpointManager();
    void onRunParameterSettings();

    // ===== 工具 =====
    void onGeneralSettings();
    void onLanguageSettings();
    void onSwitchToZhCN();
    void onSwitchToEn();
    void onShortcutSettings();
    void onThemeSettings();
    void onScriptEditor();
    void onFormulaEditor();
    void onColorEditor();
    void onViewLicense();
    void onActivateLicense();
    void onInstallPlugin();
    void onUninstallPlugin();
    void onSystemLog();
    void onBackupProject();
    void onRestoreProject();

    // ===== 窗口 =====
    void onNewWindow();
    void onCascade();
    void onTileHorizontal();
    void onTileVertical();
    void onCloseAllWindows();

    // ===== 帮助 =====
    void onHelpDocument();
    void onQuickStartGuide();
    void onVideoTutorial();
    void onSampleProject();
    void onCheckUpdate();
    void onOnlineSupport();
    void onSubmitFeedback();
    void onVersionInfo();
    void onLicenseInfo();
    void onTechnicalSupport();

    // ===== 图页管理槽 =====
    void onScreenManagerOpenRequested(const QString &screenId);
    void onScreensChanged(const QList<ScreenData> &screens);
    void onTabCloseRequested(int index);
    void onOpenRecentProject(const QString &path);
    void onDataVariableAddRequested(const QString &name, const QString &type);
    void onDataVariableRemoveRequested(const QString &id);
    // 来自 ScreenManagerDock 的新增/删除请求（走 Undo 命令）
    void onScreenAddRequested(const QString &name);
    void onScreenDeleteRequested(const QString &screenId);

public:
    // ===== 供 Undo 命令调用的内部接口（不要在其他地方直接调用） =====
    void cmdAddScreen(const ScreenData &screen, int order);
    void cmdRemoveScreen(const QString &screenId);
    void cmdAddDataVariable(const DataVariable &variable, int order);
    void cmdRemoveDataVariable(const QString &id);
    ScreenData snapshotScreen(const QString &screenId) const;
    DataVariable snapshotDataVariable(const QString &id) const;
    int dataVariableOrder(const QString &id) const;

private:
    Ui::MainWindow    *ui;
    WidgetToolbox     *m_widgetToolbox    = nullptr;
    ProjectTreeDock   *m_projectTree      = nullptr;
    QStackedWidget    *m_stackedWidget    = nullptr;
    QTabWidget        *m_tabWidget        = nullptr;
    WelcomeWidget     *m_welcomeWidget    = nullptr;
    ScreenManagerDock *m_screenManager    = nullptr;
    PropertyPanelDock *m_propertyPanel    = nullptr;
    EventPanelDock    *m_eventPanel       = nullptr;
    QDockWidget       *m_logDock          = nullptr;
    QPlainTextEdit    *m_logView          = nullptr;
    QMenu             *m_recentMenu       = nullptr;
    QToolBar          *m_fileToolBar      = nullptr;
    QToolBar          *m_editToolBar      = nullptr;
    QToolBar          *m_runToolBar       = nullptr;
    QProcess          *m_simulatorProcess = nullptr;
    QString            m_simulatorStdoutBuffer;
    QString            m_simulatorStderrBuffer;

    QAction           *m_newProjectAction        = nullptr;
    QAction           *m_openProjectAction       = nullptr;
    QAction           *m_closeProjectAction      = nullptr;
    QAction           *m_saveAction              = nullptr;
    QAction           *m_saveAsAction            = nullptr;
    QAction           *m_projectPropertiesAction = nullptr;
    QAction           *m_exitAction              = nullptr;

    QAction           *m_undoAction        = nullptr;
    QAction           *m_redoAction        = nullptr;
    QAction           *m_cutAction         = nullptr;
    QAction           *m_copyAction        = nullptr;
    QAction           *m_pasteAction       = nullptr;
    QAction           *m_deleteAction      = nullptr;
    QAction           *m_selectAllAction   = nullptr;
    QAction           *m_alignLeftAction   = nullptr;
    QAction           *m_alignRightAction  = nullptr;
    QAction           *m_alignTopAction    = nullptr;
    QAction           *m_alignBottomAction = nullptr;
    QAction           *m_alignCenterAction = nullptr;
    QAction           *m_bringToFrontAction = nullptr;
    QAction           *m_sendToBackAction   = nullptr;
    QAction           *m_bringForwardAction = nullptr;
    QAction           *m_sendBackwardAction = nullptr;
    QAction           *m_groupAction       = nullptr;
    QAction           *m_ungroupAction     = nullptr;

    QAction           *m_startRunAction       = nullptr;
    QAction           *m_stopRunAction        = nullptr;
    QAction           *m_compileProjectAction = nullptr;

    // screenId → ScreenTab*（已打开的图页）
    QHash<QString, ScreenTab *> m_openTabs;

    // ===== 工程状态 =====
    ProjectData      m_project;
    QString          m_projectFilePath;
    bool             m_projectOpen = false;
    QStringList      m_recentProjects;
    QList<WidgetInstance> m_widgetClipboard;
    int              m_widgetPasteCount = 0;

    static constexpr int kMaxRecentProjects = 10;

    // 工程相关辅助函数
    void resetProject();
    void syncSceneToProject();
    bool maybeSaveCurrent();
    bool saveProjectToPath(const QString &path);
    void updateWindowTitle();
    void setProjectOpen(bool open);

    // 图页辅助
    void openScreenTab(const QString &screenId);
    void closeScreenTab(const QString &screenId);
    void applyProjectToTabs();          // 工程加载后打开所有图页
    void refreshTabWidgetMetas();       // 把 widgetMetas 注入所有打开的 tab
    ScreenTab *currentScreenTab() const;
    CanvasScene *currentScene() const;  // 当前活动图页的 scene（可为 nullptr）
    QString currentScreenName() const;

    // 最近工程
    void loadRecentProjects();
    void saveRecentProjects();
    void addRecentProject(const QString &path);
    void updateRecentMenu();

    // 属性面板辅助
    QString generateUniqueWidgetName(const QString &baseName) const;
    void    installSceneNameGenerator(CanvasScene *scene);
    void    ensureInstanceNamesAssigned();

    // ===== Undo / Redo 跨多 stack 的链式调度 =====
    QUndoStack              *m_projectUndoStack = nullptr;
    QList<QUndoStack *>      m_undoChain;        // 按时间顺序记录每次 push 的 stack
    QList<QUndoStack *>      m_redoChain;        // 与 m_undoChain 对偶
    QHash<QUndoStack *, int> m_stackLastIndex;
    QHash<QUndoStack *, QString> m_stackScreenIds;
    bool                     m_inRedoOp = false; // 调用 stack->redo() 时设为 true
    void registerUndoStack(QUndoStack *stack, const QString &screenId = QString());
    bool activateUndoStackPage(QUndoStack *stack);
    void onAnyStackIndexChanged(QUndoStack *stack, int newIdx);
    void resetUndoChains(bool keepProjectStackRegistered = true);

public:
    QUndoStack *projectUndoStack() const { return m_projectUndoStack; }
};
#endif // MAINWINDOW_H
