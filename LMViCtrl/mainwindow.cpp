#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgettoolbox.h"
#include "projecttreedock.h"
#include "screenmanagerdock.h"
#include "propertypaneldock.h"
#include "bindinggraphview.h"
#include "bindingdetaildock.h"
#include "screentab.h"
#include "canvasscene.h"
#include "welcomewidget.h"
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFont>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextCursor>
#include <QToolBar>
#include <QUndoStack>

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	QFont font(QStringLiteral("SimHei"), 10);
	font.setStyleHint(QFont::SansSerif);
	qApp->setFont(font);
	applyDarkTheme();
	setupMenus();
	setupToolBars();

	// 项目级 Undo 栈（图页新增/删除命令）
	m_projectUndoStack = new QUndoStack(this);
	registerUndoStack(m_projectUndoStack);

	// 图页管理器（停靠在左侧上方）
	m_screenManager = new ScreenManagerDock(this);

	addDockWidget(Qt::LeftDockWidgetArea, m_screenManager);

	// 组件工具箱（停靠在左侧，位于图页管理器下方）
	m_widgetToolbox = new WidgetToolbox(this);
	addDockWidget(Qt::LeftDockWidgetArea, m_widgetToolbox);
	splitDockWidget(m_screenManager, m_widgetToolbox, Qt::Vertical);
	m_projectTree = new ProjectTreeDock(this);
	m_projectTree->setProjectData(&m_project);
	addDockWidget(Qt::LeftDockWidgetArea, m_projectTree);
	tabifyDockWidget(m_widgetToolbox, m_projectTree);
	setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);
	m_widgetToolbox->raise();
	const QString widgetsDir = QDir::current().absoluteFilePath(
		QStringLiteral("lua/widgets"));
	m_widgetToolbox->loadFromDirectory(widgetsDir);
	connect(m_screenManager, &ScreenManagerDock::openScreenRequested,
		this, &MainWindow::onScreenManagerOpenRequested);
	connect(m_screenManager, &ScreenManagerDock::screensChanged,
		this, &MainWindow::onScreensChanged);
	connect(m_screenManager, &ScreenManagerDock::addScreenRequested,
		this, &MainWindow::onScreenAddRequested);
	connect(m_screenManager, &ScreenManagerDock::deleteScreenRequested,
		this, &MainWindow::onScreenDeleteRequested);
	connect(m_projectTree, &ProjectTreeDock::addDataVariableRequested,
		this, &MainWindow::onDataVariableAddRequested);
	connect(m_projectTree, &ProjectTreeDock::removeDataVariableRequested,
		this, &MainWindow::onDataVariableRemoveRequested);

	// 多图页 TabWidget
	m_tabWidget = new QTabWidget;
	m_tabWidget->setTabsClosable(true);
	m_tabWidget->setMovable(false);
	connect(m_tabWidget, &QTabWidget::tabCloseRequested,
		this, &MainWindow::onTabCloseRequested);
	connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int) {
		const bool bindingMode = m_bindingGraphView && m_tabWidget->currentWidget() == m_bindingGraphView;
		if (bindingMode)
			m_bindingGraphView->refreshGraph();
		if (m_propertyPanel)
			m_propertyPanel->setCurrentScene(currentScene());
		if (m_bindingDetailPanel) {
			m_bindingDetailPanel->setVisible(bindingMode);
			if (bindingMode)
				m_bindingDetailPanel->raise();
		}
		if (m_projectTree)
			m_projectTree->setCurrentScene(currentScene(), currentScreenName());
		});

	m_bindingGraphView = new BindingGraphView(m_tabWidget);
	m_bindingGraphView->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
	connect(m_bindingGraphView, &BindingGraphView::statusMessageRequested,
		this, &MainWindow::appendLog);
	connect(m_bindingGraphView, &BindingGraphView::graphChanged, this, [this]() {
		m_project.updatedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
	});

	// 属性面板（停靠在右侧）
	m_propertyPanel = new PropertyPanelDock(this);
	addDockWidget(Qt::RightDockWidgetArea, m_propertyPanel);

	// 绑定详情面板（绑定模式下使用）
	m_bindingDetailPanel = new BindingDetailDock(this);
	m_bindingDetailPanel->setProjectData(&m_project);
	m_bindingDetailPanel->setWidgetMetas(m_widgetToolbox ? m_widgetToolbox->widgetMetas() : QList<WidgetMeta>{});
	m_bindingDetailPanel->setGraphView(m_bindingGraphView);
	addDockWidget(Qt::RightDockWidgetArea, m_bindingDetailPanel);
	tabifyDockWidget(m_propertyPanel, m_bindingDetailPanel);
	m_bindingDetailPanel->hide();

	setupLogDock();

	// 欢迎页
	m_welcomeWidget = new WelcomeWidget;
	connect(m_welcomeWidget, &WelcomeWidget::newProjectRequested,
		this, &MainWindow::onNewProject);
	connect(m_welcomeWidget, &WelcomeWidget::openProjectRequested,
		this, &MainWindow::onOpenProject);
	connect(m_welcomeWidget, &WelcomeWidget::openRecentRequested,
		this, &MainWindow::onOpenRecentProject);

	// 堆叠容器：0=欢迎页，1=设计区
	m_stackedWidget = new QStackedWidget(this);
	m_stackedWidget->addWidget(m_welcomeWidget);
	m_stackedWidget->addWidget(m_tabWidget);
	setCentralWidget(m_stackedWidget);

	// 加载最近工程并显示欢迎页
	loadRecentProjects();
	updateRecentMenu();
	m_welcomeWidget->setRecentProjects(m_recentProjects);
	setProjectOpen(false);   // 启动时停留在欢迎页
    this->setWindowState(Qt::WindowState::WindowMaximized);
}

MainWindow::~MainWindow()
{
	// 在成员变量析构前，主动断开所有 QUndoStack 的 destroyed 连接，
	// 防止 ~QObject() 销毁子对象时触发访问已销毁成员的回调
	resetUndoChains(false);
	delete ui;
}
void MainWindow::applyDarkTheme()
{
	qApp->setStyle("Fusion");
	QPalette pal;
	pal.setColor(QPalette::Window, QColor("#252526"));
	pal.setColor(QPalette::WindowText, QColor("#CCCCCC"));
	pal.setColor(QPalette::Base, QColor("#1e1e1e"));
	pal.setColor(QPalette::AlternateBase, QColor("#2d2d2d"));
	pal.setColor(QPalette::Text, QColor("#CCCCCC"));
	pal.setColor(QPalette::Button, QColor("#3c3c3c"));
	pal.setColor(QPalette::ButtonText, QColor("#CCCCCC"));
	pal.setColor(QPalette::Highlight, QColor("#007ACC"));
	pal.setColor(QPalette::HighlightedText, Qt::white);
	pal.setColor(QPalette::ToolTipBase, QColor("#252526"));
	pal.setColor(QPalette::ToolTipText, QColor("#CCCCCC"));
	qApp->setPalette(pal);
}
QAction* MainWindow::addAction(QMenu* menu,
	const QString& text,
	const QKeySequence& shortcut,
	bool checkable,
	bool checked)
{
	QAction* action = new QAction(text, this);
	if (!shortcut.isEmpty()) {
		action->setShortcut(shortcut);
		action->setShortcutContext(Qt::WindowShortcut);
	}
	if (checkable) {
		action->setCheckable(true);
		action->setChecked(checked);
	}
	connect(action, &QAction::triggered, this, [this, action](bool checked) {
		QString actionText = action->text();
		actionText.remove(QLatin1Char('&'));
		if (action->isCheckable()) {
			appendLog(tr("菜单操作：%1（%2）")
				.arg(actionText, checked ? tr("开启") : tr("关闭")));
		}
		else {
			appendLog(tr("菜单操作：%1").arg(actionText));
		}
		});
	menu->addAction(action);
	return action;
}

void MainWindow::setupLogDock()
{
	m_logDock = new QDockWidget(tr("日志"), this);
	m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

	m_logView = new QPlainTextEdit(m_logDock);
	m_logView->setReadOnly(true);
	m_logView->setMaximumBlockCount(1000);
	m_logView->setLineWrapMode(QPlainTextEdit::NoWrap);
	m_logView->setStyleSheet(QStringLiteral(
		"QPlainTextEdit { background:#1e1e1e; color:#cccccc; border:0; }"));

	m_logDock->setWidget(m_logView);
	addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
	resizeDocks({ m_logDock }, { 140 }, Qt::Vertical);
	appendLog(tr("日志窗口已启动"));
}

void MainWindow::appendLog(const QString& message)
{
	if (!m_logView || message.isEmpty()) return;
	const QString time = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
	m_logView->appendPlainText(QStringLiteral("[%1] %2").arg(time, message));
	m_logView->moveCursor(QTextCursor::End);
}

void MainWindow::connectSceneLogging(CanvasScene* scene, const QString& screenId)
{
	if (!scene) return;
	connect(scene, &CanvasScene::operationLogged, this, [this, screenId](const QString& message) {
		QString screenName;
		for (const ScreenData& screen : std::as_const(m_project.screens)) {
			if (screen.id == screenId) {
				screenName = screen.name;
				break;
			}
		}
		if (screenName.isEmpty()) screenName = screenId;
		appendLog(tr("画面[%1]：%2").arg(screenName, message));
		});
}

void MainWindow::setupMenus()
{
	// 清空 .ui 中可能存在的占位菜单
	menuBar()->clear();
	setupFileMenu();
	setupEditMenu();
	setupViewMenu();
	setupRunMenu();
	//setupLanguageMenu();
}

void MainWindow::setupViewMenu()
{
	QMenu* viewMenu = menuBar()->addMenu(tr("视图(&V)"));
	m_openBindingModeAction = addAction(viewMenu, tr("绑定模式"), QKeySequence(Qt::CTRL | Qt::Key_B));
	connect(m_openBindingModeAction, &QAction::triggered, this, &MainWindow::onOpenBindingMode);
}

void MainWindow::setupFileMenu()
{
	QMenu* fileMenu = menuBar()->addMenu(tr("文件(&F)"));

	m_newProjectAction = addAction(fileMenu, tr("新建工程"), QKeySequence::New);
	connect(m_newProjectAction, &QAction::triggered, this, &MainWindow::onNewProject);
	connect(addAction(fileMenu, tr("新建端到端样例工程")),
		&QAction::triggered, this, &MainWindow::onNewBindingPreviewSample);
	m_openProjectAction = addAction(fileMenu, tr("打开工程"), QKeySequence::Open);
	connect(m_openProjectAction, &QAction::triggered, this, &MainWindow::onOpenProject);
	m_closeProjectAction = addAction(fileMenu, tr("关闭工程"));
	connect(m_closeProjectAction, &QAction::triggered, this, &MainWindow::onCloseProject);
	fileMenu->addSeparator();
	m_saveAction = addAction(fileMenu, tr("保存"), QKeySequence::Save);
	connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSave);
	m_saveAsAction = addAction(fileMenu, tr("另存为"), QKeySequence::SaveAs);
	connect(m_saveAsAction, &QAction::triggered, this, &MainWindow::onSaveAs);
#if 0
	//目前不需要导入导出工程功能，先隐藏相关菜单项
	QMenu* exportMenu = fileMenu->addMenu(tr("导出工程"));
	connect(addAction(exportMenu, tr("导出为 ZIP")),
		&QAction::triggered, this, &MainWindow::onExportZip);
	connect(addAction(exportMenu, tr("导出为 XML")),
		&QAction::triggered, this, &MainWindow::onExportXml);
	connect(addAction(exportMenu, tr("导出报表")),
		&QAction::triggered, this, &MainWindow::onExportReport);

	connect(addAction(fileMenu, tr("导入工程")),
		&QAction::triggered, this, &MainWindow::onImportProject);
#endif
	fileMenu->addSeparator();
	m_projectPropertiesAction = addAction(fileMenu, tr("工程属性"));
	connect(m_projectPropertiesAction, &QAction::triggered, this, &MainWindow::onProjectProperties);

	m_recentMenu = fileMenu->addMenu(tr("最近打开工程"));
	updateRecentMenu();

	fileMenu->addSeparator();
	m_exitAction = addAction(fileMenu, tr("退出"), QKeySequence(Qt::ALT | Qt::Key_F4));
	connect(m_exitAction, &QAction::triggered, this, &MainWindow::onExit);
}

void MainWindow::setupEditMenu()
{
	QMenu* editMenu = menuBar()->addMenu(tr("编辑(&E)"));

	m_undoAction = addAction(editMenu, tr("撤销"), QKeySequence::Undo);
	connect(m_undoAction, &QAction::triggered, this, &MainWindow::onUndo);
	m_redoAction = addAction(editMenu, tr("重做"), QKeySequence::Redo);
	connect(m_redoAction, &QAction::triggered, this, &MainWindow::onRedo);
	editMenu->addSeparator();
	m_cutAction = addAction(editMenu, tr("剪切"), QKeySequence::Cut);
	connect(m_cutAction, &QAction::triggered, this, &MainWindow::onCut);
	m_copyAction = addAction(editMenu, tr("复制"), QKeySequence::Copy);
	connect(m_copyAction, &QAction::triggered, this, &MainWindow::onCopy);
	m_pasteAction = addAction(editMenu, tr("粘贴"), QKeySequence::Paste);
	connect(m_pasteAction, &QAction::triggered, this, &MainWindow::onPaste);
	m_deleteAction = addAction(editMenu, tr("删除"), QKeySequence::Delete);
	connect(m_deleteAction, &QAction::triggered, this, &MainWindow::onDelete);
	m_selectAllAction = addAction(editMenu, tr("全选"), QKeySequence::SelectAll);
	connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAll);
	editMenu->addSeparator();
#if 0
	// 目前不需要查找替换功能，先隐藏相关菜单项
	connect(addAction(editMenu, tr("查找"), QKeySequence::Find),
		&QAction::triggered, this, &MainWindow::onFind);
	connect(addAction(editMenu, tr("替换"), QKeySequence::Replace),
		&QAction::triggered, this, &MainWindow::onReplace);
	editMenu->addSeparator();
#endif
	QMenu* alignMenu = editMenu->addMenu(tr("对齐方式"));
	m_alignLeftAction = addAction(alignMenu, tr("左对齐"));
	connect(m_alignLeftAction, &QAction::triggered, this, &MainWindow::onAlignLeft);
	m_alignRightAction = addAction(alignMenu, tr("右对齐"));
	connect(m_alignRightAction, &QAction::triggered, this, &MainWindow::onAlignRight);
	m_alignTopAction = addAction(alignMenu, tr("顶部对齐"));
	connect(m_alignTopAction, &QAction::triggered, this, &MainWindow::onAlignTop);
	m_alignBottomAction = addAction(alignMenu, tr("底部对齐"));
	connect(m_alignBottomAction, &QAction::triggered, this, &MainWindow::onAlignBottom);
	m_alignCenterAction = addAction(alignMenu, tr("居中对齐"));
	connect(m_alignCenterAction, &QAction::triggered, this, &MainWindow::onAlignCenter);

	QMenu* zOrderMenu = editMenu->addMenu(tr("层级顺序"));
	m_bringToFrontAction = addAction(zOrderMenu, tr("置于顶层"),
		QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketRight));
	connect(m_bringToFrontAction, &QAction::triggered, this, &MainWindow::onBringToFront);
	m_sendToBackAction = addAction(zOrderMenu, tr("置于底层"),
		QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_BracketLeft));
	connect(m_sendToBackAction, &QAction::triggered, this, &MainWindow::onSendToBack);
	zOrderMenu->addSeparator();
	m_bringForwardAction = addAction(zOrderMenu, tr("上移一层"),
		QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
	connect(m_bringForwardAction, &QAction::triggered, this, &MainWindow::onBringForward);
	m_sendBackwardAction = addAction(zOrderMenu, tr("下移一层"),
		QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
	connect(m_sendBackwardAction, &QAction::triggered, this, &MainWindow::onSendBackward);

	m_groupAction = addAction(editMenu, tr("组合"), QKeySequence(Qt::CTRL | Qt::Key_G));
	connect(m_groupAction, &QAction::triggered, this, &MainWindow::onGroup);
	m_ungroupAction = addAction(editMenu, tr("取消组合"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
	connect(m_ungroupAction, &QAction::triggered, this, &MainWindow::onUngroup);
}

void MainWindow::setupToolBars()
{
	setupFileToolBar();
	setupEditToolBar();
	setupRunToolBar();
}

void MainWindow::setupFileToolBar()
{
	m_fileToolBar = addToolBar(tr("文件"));
	m_fileToolBar->setObjectName(QStringLiteral("fileToolBar"));
	m_fileToolBar->setMovable(false);
	m_fileToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

	m_fileToolBar->addAction(m_newProjectAction);
	m_fileToolBar->addAction(m_openProjectAction);
	m_fileToolBar->addAction(m_closeProjectAction);
	m_fileToolBar->addSeparator();
	m_fileToolBar->addAction(m_saveAction);
	m_fileToolBar->addAction(m_saveAsAction);
	m_fileToolBar->addSeparator();
	m_fileToolBar->addAction(m_projectPropertiesAction);
}

void MainWindow::setupEditToolBar()
{
	m_editToolBar = addToolBar(tr("编辑"));
	m_editToolBar->setObjectName(QStringLiteral("editToolBar"));
	m_editToolBar->setMovable(false);
	m_editToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

	m_editToolBar->addAction(m_undoAction);
	m_editToolBar->addAction(m_redoAction);
	m_editToolBar->addSeparator();
	m_editToolBar->addAction(m_cutAction);
	m_editToolBar->addAction(m_copyAction);
	m_editToolBar->addAction(m_pasteAction);
	m_editToolBar->addAction(m_deleteAction);
	m_editToolBar->addAction(m_selectAllAction);
	m_editToolBar->addSeparator();
	m_editToolBar->addAction(m_alignLeftAction);
	m_editToolBar->addAction(m_alignRightAction);
	m_editToolBar->addAction(m_alignTopAction);
	m_editToolBar->addAction(m_alignBottomAction);
	m_editToolBar->addAction(m_alignCenterAction);
	m_editToolBar->addSeparator();
	m_editToolBar->addAction(m_bringToFrontAction);
	m_editToolBar->addAction(m_sendToBackAction);
	m_editToolBar->addAction(m_bringForwardAction);
	m_editToolBar->addAction(m_sendBackwardAction);
	m_editToolBar->addSeparator();
	m_editToolBar->addAction(m_groupAction);
	m_editToolBar->addAction(m_ungroupAction);
}

void MainWindow::setupRunToolBar()
{
	m_runToolBar = addToolBar(tr("运行"));
	m_runToolBar->setObjectName(QStringLiteral("runToolBar"));
	m_runToolBar->setMovable(false);
	m_runToolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

	m_runToolBar->addAction(m_startRunAction);
	m_runToolBar->addAction(m_stopRunAction);
	m_runToolBar->addSeparator();
	m_runToolBar->addAction(m_compileProjectAction);
}

void MainWindow::setupRunMenu()
{
	QMenu* runMenu = menuBar()->addMenu(tr("运行(&R)"));

	m_startRunAction = addAction(runMenu, tr("预览运行"), QKeySequence(Qt::Key_F5));
	connect(m_startRunAction, &QAction::triggered, this, &MainWindow::onStartRun);
	m_stopRunAction = addAction(runMenu, tr("停止运行"), QKeySequence(Qt::Key_F6));
	connect(m_stopRunAction, &QAction::triggered, this, &MainWindow::onStopRun);
#if 0
	connect(addAction(runMenu, tr("暂停运行")),
		&QAction::triggered, this, &MainWindow::onPauseRun);
#endif
	runMenu->addSeparator();
	m_compileProjectAction = addAction(runMenu, tr("编译工程"), QKeySequence(Qt::Key_F7));
	connect(m_compileProjectAction, &QAction::triggered, this, &MainWindow::onCompileProject);
#if 0
	QMenu* simMenu = runMenu->addMenu(tr("模拟运行"));
	connect(addAction(simMenu, tr("开始模拟")),
		&QAction::triggered, this, &MainWindow::onStartSimulate);
	connect(addAction(simMenu, tr("模拟变量设置")),
		&QAction::triggered, this, &MainWindow::onSimulateVarSetting);

	runMenu->addSeparator();

	QMenu* debugMenu = runMenu->addMenu(tr("调试"));
	connect(addAction(debugMenu, tr("脚本调试")),
		&QAction::triggered, this, &MainWindow::onScriptDebug);
	connect(addAction(debugMenu, tr("变量监视")),
		&QAction::triggered, this, &MainWindow::onVariableMonitor);
	connect(addAction(debugMenu, tr("断点管理")),
		&QAction::triggered, this, &MainWindow::onBreakpointManager);

	runMenu->addSeparator();
	connect(addAction(runMenu, tr("运行参数设置")),
		&QAction::triggered, this, &MainWindow::onRunParameterSettings);
#endif
}

void MainWindow::setupLanguageMenu()
{
	QMenu* langMenu = menuBar()->addMenu(tr("语言(&L)"));

	connect(addAction(langMenu, tr("简体中文"), QKeySequence(), true),
		&QAction::triggered, this, &MainWindow::onSwitchToZhCN);
	connect(addAction(langMenu, tr("English"), QKeySequence(), true),
		&QAction::triggered, this, &MainWindow::onSwitchToEn);
}