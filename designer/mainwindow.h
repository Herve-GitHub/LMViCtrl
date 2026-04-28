#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class WidgetToolbox;
class CanvasScene;
class CanvasView;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
class QMenu;
class QAction;
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
    void setupViewMenu();
    void setupProjectMenu();
    void setupDrawMenu();
    void setupComponentMenu();
    void setupCommunicationMenu();
    void setupRunMenu();
    void setupToolsMenu();
    void setupWindowMenu();
    void setupLanguageMenu();
    void setupHelpMenu();

    // 工具函数：创建带快捷键的 QAction 并添加到菜单
    QAction *addAction(QMenu *menu,
                       const QString &text,
                       const QKeySequence &shortcut = QKeySequence(),
                       bool checkable = false,
                       bool checked = false);

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

private:
    Ui::MainWindow  *ui;
    WidgetToolbox   *m_widgetToolbox = nullptr;
    CanvasScene     *m_canvasScene   = nullptr;
    CanvasView      *m_canvasView    = nullptr;
};
#endif // MAINWINDOW_H
