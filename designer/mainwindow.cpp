#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgettoolbox.h"
#include "canvasscene.h"
#include "canvasview.h"

#include <QAction>
#include <QDir>
#include <QFont>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QFont font(QStringLiteral("SimHei"), 10);
    font.setStyleHint(QFont::SansSerif);
    qApp->setFont(font);
    applyDarkTheme();
    setupMenus();

    // 组件工具箱（停靠在左侧）
    m_widgetToolbox = new WidgetToolbox(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_widgetToolbox);
    const QString widgetsDir = QDir::current().absoluteFilePath(
        QStringLiteral("lua/widgets"));
    m_widgetToolbox->loadFromDirectory(widgetsDir);

    // 画布
    m_canvasScene = new CanvasScene(this);
    m_canvasScene->setCanvasSize(1024, 768);   // 默认与 ProjectTarget 一致
    m_canvasScene->setWidgetMetas(m_widgetToolbox->widgetMetas());

    m_canvasView = new CanvasView(this);
    m_canvasView->setScene(m_canvasScene);
    setCentralWidget(m_canvasView);
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::applyDarkTheme()
{
    qApp->setStyle("Fusion");
    QPalette pal;
    pal.setColor(QPalette::Window,          QColor("#252526"));
    pal.setColor(QPalette::WindowText,      QColor("#CCCCCC"));
    pal.setColor(QPalette::Base,            QColor("#1e1e1e"));
    pal.setColor(QPalette::AlternateBase,   QColor("#2d2d2d"));
    pal.setColor(QPalette::Text,            QColor("#CCCCCC"));
    pal.setColor(QPalette::Button,          QColor("#3c3c3c"));
    pal.setColor(QPalette::ButtonText,      QColor("#CCCCCC"));
    pal.setColor(QPalette::Highlight,       QColor("#007ACC"));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    pal.setColor(QPalette::ToolTipBase,     QColor("#252526"));
    pal.setColor(QPalette::ToolTipText,     QColor("#CCCCCC"));
    qApp->setPalette(pal);
}
QAction *MainWindow::addAction(QMenu *menu,
                               const QString &text,
                               const QKeySequence &shortcut,
                               bool checkable,
                               bool checked)
{
    QAction *action = new QAction(text, this);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        action->setShortcutContext(Qt::WindowShortcut);
    }
    if (checkable) {
        action->setCheckable(true);
        action->setChecked(checked);
    }
    menu->addAction(action);
    return action;
}

void MainWindow::setupMenus()
{
    // 清空 .ui 中可能存在的占位菜单
    menuBar()->clear();

    setupFileMenu();
    setupEditMenu();
    setupViewMenu();
    setupProjectMenu();
    setupDrawMenu();
    setupComponentMenu();
    setupCommunicationMenu();
    setupRunMenu();
    setupToolsMenu();
    setupWindowMenu();
    setupLanguageMenu();
    setupHelpMenu();
}

void MainWindow::setupFileMenu()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    connect(addAction(fileMenu, tr("新建工程"), QKeySequence::New),
            &QAction::triggered, this, &MainWindow::onNewProject);
    connect(addAction(fileMenu, tr("打开工程"), QKeySequence::Open),
            &QAction::triggered, this, &MainWindow::onOpenProject);
    connect(addAction(fileMenu, tr("关闭工程")),
            &QAction::triggered, this, &MainWindow::onCloseProject);
    fileMenu->addSeparator();
    connect(addAction(fileMenu, tr("保存"), QKeySequence::Save),
            &QAction::triggered, this, &MainWindow::onSave);
    connect(addAction(fileMenu, tr("另存为"), QKeySequence::SaveAs),
            &QAction::triggered, this, &MainWindow::onSaveAs);

    QMenu *exportMenu = fileMenu->addMenu(tr("导出工程"));
    connect(addAction(exportMenu, tr("导出为 ZIP")),
            &QAction::triggered, this, &MainWindow::onExportZip);
    connect(addAction(exportMenu, tr("导出为 XML")),
            &QAction::triggered, this, &MainWindow::onExportXml);
    connect(addAction(exportMenu, tr("导出报表")),
            &QAction::triggered, this, &MainWindow::onExportReport);

    connect(addAction(fileMenu, tr("导入工程")),
            &QAction::triggered, this, &MainWindow::onImportProject);
    fileMenu->addSeparator();
    connect(addAction(fileMenu, tr("工程属性")),
            &QAction::triggered, this, &MainWindow::onProjectProperties);

    QMenu *recentMenu = fileMenu->addMenu(tr("最近打开工程"));
    QAction *emptyRecent = recentMenu->addAction(tr("(空)"));
    emptyRecent->setEnabled(false);

    fileMenu->addSeparator();
    connect(addAction(fileMenu, tr("退出"), QKeySequence(Qt::ALT | Qt::Key_F4)),
            &QAction::triggered, this, &MainWindow::onExit);
}

void MainWindow::setupEditMenu()
{
    QMenu *editMenu = menuBar()->addMenu(tr("编辑(&E)"));

    connect(addAction(editMenu, tr("撤销"), QKeySequence::Undo),
            &QAction::triggered, this, &MainWindow::onUndo);
    connect(addAction(editMenu, tr("重做"), QKeySequence::Redo),
            &QAction::triggered, this, &MainWindow::onRedo);
    editMenu->addSeparator();
    connect(addAction(editMenu, tr("剪切"), QKeySequence::Cut),
            &QAction::triggered, this, &MainWindow::onCut);
    connect(addAction(editMenu, tr("复制"), QKeySequence::Copy),
            &QAction::triggered, this, &MainWindow::onCopy);
    connect(addAction(editMenu, tr("粘贴"), QKeySequence::Paste),
            &QAction::triggered, this, &MainWindow::onPaste);
    connect(addAction(editMenu, tr("删除"), QKeySequence::Delete),
            &QAction::triggered, this, &MainWindow::onDelete);
    connect(addAction(editMenu, tr("全选"), QKeySequence::SelectAll),
            &QAction::triggered, this, &MainWindow::onSelectAll);
    editMenu->addSeparator();
    connect(addAction(editMenu, tr("查找"), QKeySequence::Find),
            &QAction::triggered, this, &MainWindow::onFind);
    connect(addAction(editMenu, tr("替换"), QKeySequence::Replace),
            &QAction::triggered, this, &MainWindow::onReplace);
    editMenu->addSeparator();

    QMenu *alignMenu = editMenu->addMenu(tr("对齐方式"));
    connect(addAction(alignMenu, tr("左对齐")),
            &QAction::triggered, this, &MainWindow::onAlignLeft);
    connect(addAction(alignMenu, tr("右对齐")),
            &QAction::triggered, this, &MainWindow::onAlignRight);
    connect(addAction(alignMenu, tr("顶部对齐")),
            &QAction::triggered, this, &MainWindow::onAlignTop);
    connect(addAction(alignMenu, tr("底部对齐")),
            &QAction::triggered, this, &MainWindow::onAlignBottom);
    connect(addAction(alignMenu, tr("居中对齐")),
            &QAction::triggered, this, &MainWindow::onAlignCenter);

    connect(addAction(editMenu, tr("组合"), QKeySequence(Qt::CTRL | Qt::Key_G)),
            &QAction::triggered, this, &MainWindow::onGroup);
    connect(addAction(editMenu, tr("取消组合"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G)),
            &QAction::triggered, this, &MainWindow::onUngroup);
}

void MainWindow::setupViewMenu()
{
    QMenu *viewMenu = menuBar()->addMenu(tr("视图(&V)"));

    QMenu *toolbarMenu = viewMenu->addMenu(tr("工具栏"));
    connect(addAction(toolbarMenu, tr("标准工具栏"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleStandardToolbar);
    connect(addAction(toolbarMenu, tr("绘图工具栏"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleDrawToolbar);
    connect(addAction(toolbarMenu, tr("格式工具栏"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleFormatToolbar);

    viewMenu->addSeparator();
    connect(addAction(viewMenu, tr("工程浏览器"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleProjectBrowser);
    connect(addAction(viewMenu, tr("属性面板"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onTogglePropertyPanel);
    connect(addAction(viewMenu, tr("输出窗口"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleOutputWindow);
    connect(addAction(viewMenu, tr("状态栏"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleStatusBar);
    viewMenu->addSeparator();

    connect(addAction(viewMenu, tr("放大"), QKeySequence::ZoomIn),
            &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(addAction(viewMenu, tr("缩小"), QKeySequence::ZoomOut),
            &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(addAction(viewMenu, tr("适应窗口"), QKeySequence(Qt::CTRL | Qt::Key_0)),
            &QAction::triggered, this, &MainWindow::onFitToWindow);
    connect(addAction(viewMenu, tr("自定义缩放比例")),
            &QAction::triggered, this, &MainWindow::onCustomZoom);
    viewMenu->addSeparator();

    connect(addAction(viewMenu, tr("网格显示"), QKeySequence(), true, true),
            &QAction::toggled, this, &MainWindow::onToggleGrid);
    connect(addAction(viewMenu, tr("网格设置")),
            &QAction::triggered, this, &MainWindow::onGridSettings);
    viewMenu->addSeparator();

    connect(addAction(viewMenu, tr("全屏"), QKeySequence(Qt::Key_F11)),
            &QAction::triggered, this, &MainWindow::onFullScreen);
}

void MainWindow::setupProjectMenu()
{
    QMenu *projectMenu = menuBar()->addMenu(tr("工程(&P)"));

    QMenu *manageMenu = projectMenu->addMenu(tr("工程管理"));
    connect(addAction(manageMenu, tr("工程信息")),
            &QAction::triggered, this, &MainWindow::onProjectInfo);
    connect(addAction(manageMenu, tr("工程配置")),
            &QAction::triggered, this, &MainWindow::onProjectConfig);
    connect(addAction(manageMenu, tr("工程权限管理")),
            &QAction::triggered, this, &MainWindow::onProjectPermission);

    projectMenu->addSeparator();

    QMenu *screenMenu = projectMenu->addMenu(tr("画面管理"));
    connect(addAction(screenMenu, tr("新建画面")),
            &QAction::triggered, this, &MainWindow::onNewScreen);
    connect(addAction(screenMenu, tr("删除画面")),
            &QAction::triggered, this, &MainWindow::onDeleteScreen);
    connect(addAction(screenMenu, tr("画面属性")),
            &QAction::triggered, this, &MainWindow::onScreenProperties);

    QMenu *varMenu = projectMenu->addMenu(tr("变量管理"));
    connect(addAction(varMenu, tr("变量字典")),
            &QAction::triggered, this, &MainWindow::onTagDictionary);
    connect(addAction(varMenu, tr("变量组")),
            &QAction::triggered, this, &MainWindow::onTagGroup);
    connect(addAction(varMenu, tr("导入变量")),
            &QAction::triggered, this, &MainWindow::onImportTag);
    connect(addAction(varMenu, tr("导出变量")),
            &QAction::triggered, this, &MainWindow::onExportTag);

    QMenu *alarmMenu = projectMenu->addMenu(tr("报警管理"));
    connect(addAction(alarmMenu, tr("报警组配置")),
            &QAction::triggered, this, &MainWindow::onAlarmGroupConfig);
    connect(addAction(alarmMenu, tr("报警级别设置")),
            &QAction::triggered, this, &MainWindow::onAlarmLevelSettings);
    connect(addAction(alarmMenu, tr("报警记录策略")),
            &QAction::triggered, this, &MainWindow::onAlarmRecordPolicy);

    QMenu *historyMenu = projectMenu->addMenu(tr("历史数据管理"));
    connect(addAction(historyMenu, tr("数据存储配置")),
            &QAction::triggered, this, &MainWindow::onDataStorageConfig);
    connect(addAction(historyMenu, tr("数据归档策略")),
            &QAction::triggered, this, &MainWindow::onDataArchivePolicy);

    connect(addAction(projectMenu, tr("配方管理")),
            &QAction::triggered, this, &MainWindow::onRecipeManager);

    QMenu *scriptMenu = projectMenu->addMenu(tr("脚本管理"));
    connect(addAction(scriptMenu, tr("全局脚本")),
            &QAction::triggered, this, &MainWindow::onGlobalScript);
    connect(addAction(scriptMenu, tr("事件脚本")),
            &QAction::triggered, this, &MainWindow::onEventScript);

    projectMenu->addSeparator();

    QMenu *userMenu = projectMenu->addMenu(tr("用户权限管理"));
    connect(addAction(userMenu, tr("用户列表")),
            &QAction::triggered, this, &MainWindow::onUserList);
    connect(addAction(userMenu, tr("角色管理")),
            &QAction::triggered, this, &MainWindow::onRoleManagement);
    connect(addAction(userMenu, tr("权限策略")),
            &QAction::triggered, this, &MainWindow::onPermissionPolicy);
}

void MainWindow::setupDrawMenu()
{
    QMenu *drawMenu = menuBar()->addMenu(tr("绘图(&D)"));

    QMenu *basicMenu = drawMenu->addMenu(tr("基本图形"));
    connect(addAction(basicMenu, tr("直线")),
            &QAction::triggered, this, &MainWindow::onDrawLine);
    connect(addAction(basicMenu, tr("折线")),
            &QAction::triggered, this, &MainWindow::onDrawPolyline);
    connect(addAction(basicMenu, tr("矩形")),
            &QAction::triggered, this, &MainWindow::onDrawRect);
    connect(addAction(basicMenu, tr("椭圆/圆")),
            &QAction::triggered, this, &MainWindow::onDrawEllipse);
    connect(addAction(basicMenu, tr("多边形")),
            &QAction::triggered, this, &MainWindow::onDrawPolygon);
    connect(addAction(basicMenu, tr("弧线")),
            &QAction::triggered, this, &MainWindow::onDrawArc);

    QMenu *textMenu = drawMenu->addMenu(tr("文本"));
    connect(addAction(textMenu, tr("静态文本")),
            &QAction::triggered, this, &MainWindow::onStaticText);
    connect(addAction(textMenu, tr("动态文本")),
            &QAction::triggered, this, &MainWindow::onDynamicText);

    drawMenu->addSeparator();

    QMenu *layerMenu = drawMenu->addMenu(tr("图层管理"));
    connect(addAction(layerMenu, tr("新建图层")),
            &QAction::triggered, this, &MainWindow::onNewLayer);
    connect(addAction(layerMenu, tr("删除图层")),
            &QAction::triggered, this, &MainWindow::onDeleteLayer);
    connect(addAction(layerMenu, tr("图层顺序")),
            &QAction::triggered, this, &MainWindow::onLayerOrder);
    connect(addAction(layerMenu, tr("图层可见性")),
            &QAction::triggered, this, &MainWindow::onLayerVisibility);

    drawMenu->addSeparator();

    QMenu *propMenu = drawMenu->addMenu(tr("图形属性"));
    connect(addAction(propMenu, tr("填充颜色")),
            &QAction::triggered, this, &MainWindow::onFillColor);
    connect(addAction(propMenu, tr("边框颜色")),
            &QAction::triggered, this, &MainWindow::onBorderColor);
    connect(addAction(propMenu, tr("线型/线宽")),
            &QAction::triggered, this, &MainWindow::onLineStyle);
    connect(addAction(propMenu, tr("透明度")),
            &QAction::triggered, this, &MainWindow::onOpacity);

    connect(addAction(drawMenu, tr("背景设置")),
            &QAction::triggered, this, &MainWindow::onBackgroundSettings);
}

void MainWindow::setupComponentMenu()
{
    QMenu *compMenu = menuBar()->addMenu(tr("组件(&C)"));

    QMenu *basicMenu = compMenu->addMenu(tr("基础控件"));
    connect(addAction(basicMenu, tr("按钮")),
            &QAction::triggered, this, &MainWindow::onCompButton);
    connect(addAction(basicMenu, tr("指示灯")),
            &QAction::triggered, this, &MainWindow::onCompIndicator);
    connect(addAction(basicMenu, tr("输入框")),
            &QAction::triggered, this, &MainWindow::onCompInputBox);
    connect(addAction(basicMenu, tr("标签")),
            &QAction::triggered, this, &MainWindow::onCompLabel);
    connect(addAction(basicMenu, tr("图片")),
            &QAction::triggered, this, &MainWindow::onCompImage);

    QMenu *meterMenu = compMenu->addMenu(tr("仪表控件"));
    connect(addAction(meterMenu, tr("仪表盘")),
            &QAction::triggered, this, &MainWindow::onCompGauge);
    connect(addAction(meterMenu, tr("进度条")),
            &QAction::triggered, this, &MainWindow::onCompProgressBar);
    connect(addAction(meterMenu, tr("温度计")),
            &QAction::triggered, this, &MainWindow::onCompThermometer);
    connect(addAction(meterMenu, tr("液位计")),
            &QAction::triggered, this, &MainWindow::onCompLevel);

    QMenu *chartMenu = compMenu->addMenu(tr("图表控件"));
    connect(addAction(chartMenu, tr("实时曲线")),
            &QAction::triggered, this, &MainWindow::onCompRealtimeChart);
    connect(addAction(chartMenu, tr("历史曲线")),
            &QAction::triggered, this, &MainWindow::onCompHistoryChart);
    connect(addAction(chartMenu, tr("棒图")),
            &QAction::triggered, this, &MainWindow::onCompBarChart);
    connect(addAction(chartMenu, tr("饼图")),
            &QAction::triggered, this, &MainWindow::onCompPieChart);

    QMenu *alarmMenu = compMenu->addMenu(tr("报警控件"));
    connect(addAction(alarmMenu, tr("报警列表")),
            &QAction::triggered, this, &MainWindow::onCompAlarmList);
    connect(addAction(alarmMenu, tr("报警条")),
            &QAction::triggered, this, &MainWindow::onCompAlarmBar);

    QMenu *reportMenu = compMenu->addMenu(tr("报表控件"));
    connect(addAction(reportMenu, tr("数据报表")),
            &QAction::triggered, this, &MainWindow::onCompDataReport);
    connect(addAction(reportMenu, tr("打印报表")),
            &QAction::triggered, this, &MainWindow::onCompPrintReport);

    compMenu->addSeparator();

    QMenu *libMenu = compMenu->addMenu(tr("图库管理"));
    connect(addAction(libMenu, tr("系统图库")),
            &QAction::triggered, this, &MainWindow::onSystemLibrary);
    connect(addAction(libMenu, tr("用户图库")),
            &QAction::triggered, this, &MainWindow::onUserLibrary);
    connect(addAction(libMenu, tr("导入图库")),
            &QAction::triggered, this, &MainWindow::onImportLibrary);

    connect(addAction(compMenu, tr("自定义组件")),
            &QAction::triggered, this, &MainWindow::onCustomComponent);
}

void MainWindow::setupCommunicationMenu()
{
    QMenu *commMenu = menuBar()->addMenu(tr("通信(&M)"));

    QMenu *driverMenu = commMenu->addMenu(tr("驱动管理"));
    connect(addAction(driverMenu, tr("添加驱动")),
            &QAction::triggered, this, &MainWindow::onAddDriver);
    connect(addAction(driverMenu, tr("删除驱动")),
            &QAction::triggered, this, &MainWindow::onRemoveDriver);
    connect(addAction(driverMenu, tr("驱动配置")),
            &QAction::triggered, this, &MainWindow::onDriverConfig);

    QMenu *deviceMenu = commMenu->addMenu(tr("设备管理"));
    connect(addAction(deviceMenu, tr("添加设备")),
            &QAction::triggered, this, &MainWindow::onAddDevice);
    connect(addAction(deviceMenu, tr("设备属性")),
            &QAction::triggered, this, &MainWindow::onDeviceProperties);
    connect(addAction(deviceMenu, tr("设备诊断")),
            &QAction::triggered, this, &MainWindow::onDeviceDiagnose);

    commMenu->addSeparator();

    QMenu *protocolMenu = commMenu->addMenu(tr("协议配置"));
    connect(addAction(protocolMenu, tr("OPC UA")),
            &QAction::triggered, this, &MainWindow::onProtocolOpcUa);
    connect(addAction(protocolMenu, tr("OPC DA")),
            &QAction::triggered, this, &MainWindow::onProtocolOpcDa);
    connect(addAction(protocolMenu, tr("Modbus TCP/RTU")),
            &QAction::triggered, this, &MainWindow::onProtocolModbus);
    connect(addAction(protocolMenu, tr("PROFIBUS")),
            &QAction::triggered, this, &MainWindow::onProtocolProfibus);
    connect(addAction(protocolMenu, tr("EtherNet/IP")),
            &QAction::triggered, this, &MainWindow::onProtocolEthernetIp);
    connect(addAction(protocolMenu, tr("MQTT")),
            &QAction::triggered, this, &MainWindow::onProtocolMqtt);
    connect(addAction(protocolMenu, tr("自定义协议")),
            &QAction::triggered, this, &MainWindow::onProtocolCustom);

    commMenu->addSeparator();

    QMenu *dbMenu = commMenu->addMenu(tr("数据库连接"));
    connect(addAction(dbMenu, tr("关系数据库 (SQL)")),
            &QAction::triggered, this, &MainWindow::onSqlDatabase);
    connect(addAction(dbMenu, tr("时序数据库")),
            &QAction::triggered, this, &MainWindow::onTimeSeriesDatabase);

    QMenu *diagMenu = commMenu->addMenu(tr("通信诊断"));
    connect(addAction(diagMenu, tr("通信状态监控")),
            &QAction::triggered, this, &MainWindow::onCommStatusMonitor);
    connect(addAction(diagMenu, tr("通信日志")),
            &QAction::triggered, this, &MainWindow::onCommLog);
}

void MainWindow::setupRunMenu()
{
    QMenu *runMenu = menuBar()->addMenu(tr("运行(&R)"));

    connect(addAction(runMenu, tr("启动运行"), QKeySequence(Qt::Key_F5)),
            &QAction::triggered, this, &MainWindow::onStartRun);
    connect(addAction(runMenu, tr("停止运行"), QKeySequence(Qt::Key_F6)),
            &QAction::triggered, this, &MainWindow::onStopRun);
    connect(addAction(runMenu, tr("暂停运行")),
            &QAction::triggered, this, &MainWindow::onPauseRun);
    runMenu->addSeparator();

    QMenu *simMenu = runMenu->addMenu(tr("模拟运行"));
    connect(addAction(simMenu, tr("开始模拟")),
            &QAction::triggered, this, &MainWindow::onStartSimulate);
    connect(addAction(simMenu, tr("模拟变量设置")),
            &QAction::triggered, this, &MainWindow::onSimulateVarSetting);

    runMenu->addSeparator();

    QMenu *debugMenu = runMenu->addMenu(tr("调试"));
    connect(addAction(debugMenu, tr("脚本调试")),
            &QAction::triggered, this, &MainWindow::onScriptDebug);
    connect(addAction(debugMenu, tr("变量监视")),
            &QAction::triggered, this, &MainWindow::onVariableMonitor);
    connect(addAction(debugMenu, tr("断点管理")),
            &QAction::triggered, this, &MainWindow::onBreakpointManager);

    runMenu->addSeparator();
    connect(addAction(runMenu, tr("运行参数设置")),
            &QAction::triggered, this, &MainWindow::onRunParameterSettings);
}

void MainWindow::setupToolsMenu()
{
    QMenu *toolsMenu = menuBar()->addMenu(tr("工具(&T)"));

    QMenu *settingsMenu = toolsMenu->addMenu(tr("系统设置"));
    connect(addAction(settingsMenu, tr("常规设置")),
            &QAction::triggered, this, &MainWindow::onGeneralSettings);
    connect(addAction(settingsMenu, tr("语言设置")),
            &QAction::triggered, this, &MainWindow::onLanguageSettings);
    connect(addAction(settingsMenu, tr("快捷键设置")),
            &QAction::triggered, this, &MainWindow::onShortcutSettings);
    connect(addAction(settingsMenu, tr("主题设置")),
            &QAction::triggered, this, &MainWindow::onThemeSettings);

    toolsMenu->addSeparator();
    connect(addAction(toolsMenu, tr("脚本编辑器")),
            &QAction::triggered, this, &MainWindow::onScriptEditor);
    connect(addAction(toolsMenu, tr("公式编辑器")),
            &QAction::triggered, this, &MainWindow::onFormulaEditor);
    connect(addAction(toolsMenu, tr("颜色编辑器")),
            &QAction::triggered, this, &MainWindow::onColorEditor);
    toolsMenu->addSeparator();

    QMenu *licenseMenu = toolsMenu->addMenu(tr("授权管理"));
    connect(addAction(licenseMenu, tr("查看授权信息")),
            &QAction::triggered, this, &MainWindow::onViewLicense);
    connect(addAction(licenseMenu, tr("激活授权")),
            &QAction::triggered, this, &MainWindow::onActivateLicense);

    QMenu *pluginMenu = toolsMenu->addMenu(tr("插件管理"));
    connect(addAction(pluginMenu, tr("安装插件")),
            &QAction::triggered, this, &MainWindow::onInstallPlugin);
    connect(addAction(pluginMenu, tr("卸载插件")),
            &QAction::triggered, this, &MainWindow::onUninstallPlugin);

    toolsMenu->addSeparator();
    connect(addAction(toolsMenu, tr("系统日志")),
            &QAction::triggered, this, &MainWindow::onSystemLog);

    QMenu *backupMenu = toolsMenu->addMenu(tr("备份与还原"));
    connect(addAction(backupMenu, tr("备份工程")),
            &QAction::triggered, this, &MainWindow::onBackupProject);
    connect(addAction(backupMenu, tr("还原工程")),
            &QAction::triggered, this, &MainWindow::onRestoreProject);
}

void MainWindow::setupWindowMenu()
{
    QMenu *windowMenu = menuBar()->addMenu(tr("窗口(&W)"));

    connect(addAction(windowMenu, tr("新建窗口")),
            &QAction::triggered, this, &MainWindow::onNewWindow);
    windowMenu->addSeparator();
    connect(addAction(windowMenu, tr("层叠排列")),
            &QAction::triggered, this, &MainWindow::onCascade);
    connect(addAction(windowMenu, tr("水平平铺")),
            &QAction::triggered, this, &MainWindow::onTileHorizontal);
    connect(addAction(windowMenu, tr("垂直平铺")),
            &QAction::triggered, this, &MainWindow::onTileVertical);
    windowMenu->addSeparator();
    connect(addAction(windowMenu, tr("关闭所有窗口")),
            &QAction::triggered, this, &MainWindow::onCloseAllWindows);
    windowMenu->addSeparator();
    QAction *placeholder = windowMenu->addAction(tr("(无打开的窗口)"));
    placeholder->setEnabled(false);
}

void MainWindow::setupLanguageMenu()
{
    QMenu *langMenu = menuBar()->addMenu(tr("语言(&L)"));

    connect(addAction(langMenu, tr("简体中文"), QKeySequence(), true),
            &QAction::triggered, this, &MainWindow::onSwitchToZhCN);
    connect(addAction(langMenu, tr("English"), QKeySequence(), true),
            &QAction::triggered, this, &MainWindow::onSwitchToEn);
}

void MainWindow::setupHelpMenu()
{
    QMenu *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));

    connect(addAction(helpMenu, tr("帮助文档"), QKeySequence(Qt::Key_F1)),
            &QAction::triggered, this, &MainWindow::onHelpDocument);
    connect(addAction(helpMenu, tr("快速入门指南")),
            &QAction::triggered, this, &MainWindow::onQuickStartGuide);
    connect(addAction(helpMenu, tr("视频教程")),
            &QAction::triggered, this, &MainWindow::onVideoTutorial);
    helpMenu->addSeparator();
    connect(addAction(helpMenu, tr("示例工程")),
            &QAction::triggered, this, &MainWindow::onSampleProject);
    helpMenu->addSeparator();
    connect(addAction(helpMenu, tr("检查更新")),
            &QAction::triggered, this, &MainWindow::onCheckUpdate);
    connect(addAction(helpMenu, tr("在线支持")),
            &QAction::triggered, this, &MainWindow::onOnlineSupport);
    connect(addAction(helpMenu, tr("提交反馈")),
            &QAction::triggered, this, &MainWindow::onSubmitFeedback);
    helpMenu->addSeparator();

    QMenu *aboutMenu = helpMenu->addMenu(tr("关于"));
    connect(addAction(aboutMenu, tr("版本信息")),
            &QAction::triggered, this, &MainWindow::onVersionInfo);
    connect(addAction(aboutMenu, tr("许可证信息")),
            &QAction::triggered, this, &MainWindow::onLicenseInfo);
    connect(addAction(aboutMenu, tr("技术支持信息")),
            &QAction::triggered, this, &MainWindow::onTechnicalSupport);
}
