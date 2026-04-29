#include "welcomewidget.h"

#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

WelcomeWidget::WelcomeWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void WelcomeWidget::setRecentProjects(const QStringList &paths)
{
    m_recentList->clear();
    for (const QString &p : paths) {
        QListWidgetItem *item = new QListWidgetItem(m_recentList);
        const QFileInfo fi(p);
        item->setText(fi.fileName());
        item->setToolTip(p);
        item->setData(Qt::UserRole, p);
    }
    m_recentList->setVisible(!paths.isEmpty());
}

void WelcomeWidget::onRecentItemDoubleClicked()
{
    const QListWidgetItem *item = m_recentList->currentItem();
    if (!item) return;
    emit openRecentRequested(item->data(Qt::UserRole).toString());
}

void WelcomeWidget::buildUi()
{
    // ── 整体背景色继承暗色主题 ─────────────────────────────────────────────

    // ══════════════════════════════════════════════════════════════════════
    // 顶部标题区
    // ══════════════════════════════════════════════════════════════════════
    auto *titleLabel = new QLabel(QStringLiteral("QtLvglDesigner"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QStringLiteral("color: #4FC3F7;"));

    auto *subtitleLabel = new QLabel(
        tr("基于 LVGL + Lua 的嵌入式 HMI 组态设计工具"), this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = subtitleLabel->font();
    subFont.setPointSize(11);
    subtitleLabel->setFont(subFont);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #9E9E9E;"));

    // ══════════════════════════════════════════════════════════════════════
    // 功能简介
    // ══════════════════════════════════════════════════════════════════════
    const QString introHtml = QStringLiteral(
        "<div style='line-height:1.8; color:#BDBDBD; font-size:10pt;'>"
        "<b style='color:#80CBC4;'>◆ 可视化组态设计</b>&nbsp;&nbsp;"
        "拖拽式布局，所见即所得地设计 LVGL 界面。<br/>"
        "<b style='color:#80CBC4;'>◆ Lua 脚本驱动</b>&nbsp;&nbsp;"
        "工程自动编译为 Lua 脚本，无缝运行在 LVGL + Lua 运行时。<br/>"
        "<b style='color:#80CBC4;'>◆ 多图页管理</b>&nbsp;&nbsp;"
        "支持创建多个画面，灵活管理页面跳转逻辑。<br/>"
        "<b style='color:#80CBC4;'>◆ 丰富组件库</b>&nbsp;&nbsp;"
        "内置按钮、仪表、图表、滑块、文本框等常用 LVGL 组件。<br/>"
        "<b style='color:#80CBC4;'>◆ 跨平台模拟</b>&nbsp;&nbsp;"
        "集成 SDL2 仿真运行，直接在 PC 上预览目标效果。"
        "</div>");

    auto *introLabel = new QLabel(introHtml, this);
    introLabel->setWordWrap(true);
    introLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    introLabel->setContentsMargins(0, 0, 0, 0);

    // ══════════════════════════════════════════════════════════════════════
    // 操作按钮
    // ══════════════════════════════════════════════════════════════════════
    auto *btnNew = new QPushButton(tr("  新建工程"), this);
    btnNew->setFixedSize(160, 42);
    btnNew->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #007ACC;"
        "  color: white;"
        "  border-radius: 4px;"
        "  font-size: 11pt;"
        "}"
        "QPushButton:hover  { background-color: #1C97EA; }"
        "QPushButton:pressed{ background-color: #005F9E; }"));

    auto *btnOpen = new QPushButton(tr("  打开工程"), this);
    btnOpen->setFixedSize(160, 42);
    btnOpen->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color: #3c3c3c;"
        "  color: #CCCCCC;"
        "  border: 1px solid #555555;"
        "  border-radius: 4px;"
        "  font-size: 11pt;"
        "}"
        "QPushButton:hover  { background-color: #505050; }"
        "QPushButton:pressed{ background-color: #2a2a2a; }"));

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(16);
    btnRow->addStretch();
    btnRow->addWidget(btnNew);
    btnRow->addWidget(btnOpen);
    btnRow->addStretch();

    connect(btnNew,  &QPushButton::clicked, this, &WelcomeWidget::newProjectRequested);
    connect(btnOpen, &QPushButton::clicked, this, &WelcomeWidget::openProjectRequested);

    // ══════════════════════════════════════════════════════════════════════
    // 最近工程列表
    // ══════════════════════════════════════════════════════════════════════
    auto *recentTitle = new QLabel(tr("最近工程"), this);
    QFont rf = recentTitle->font();
    rf.setBold(true);
    rf.setPointSize(10);
    recentTitle->setFont(rf);
    recentTitle->setStyleSheet(QStringLiteral("color: #9E9E9E;"));

    m_recentList = new QListWidget(this);
    m_recentList->setFixedHeight(160);
    m_recentList->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  background-color: #1e1e1e;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 4px;"
        "  color: #CCCCCC;"
        "  font-size: 10pt;"
        "}"
        "QListWidget::item:hover    { background-color: #2a2d2e; }"
        "QListWidget::item:selected { background-color: #094771; }"));
    m_recentList->hide();   // 无记录时不占空间

    connect(m_recentList, &QListWidget::itemDoubleClicked,
            this,          &WelcomeWidget::onRecentItemDoubleClicked);

    // ══════════════════════════════════════════════════════════════════════
    // 页脚版本
    // ══════════════════════════════════════════════════════════════════════
    auto *footerLabel = new QLabel(
        QStringLiteral("QtLvglDesigner  v0.1  |  © 2026"), this);
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setStyleSheet(QStringLiteral("color: #616161; font-size: 9pt;"));

    // ══════════════════════════════════════════════════════════════════════
    // 内容卡片（有最大宽度限制）
    // ══════════════════════════════════════════════════════════════════════
    auto *card = new QFrame(this);
    card->setMaximumWidth(680);
    card->setStyleSheet(QStringLiteral(
        "QFrame {"
        "  background-color: #2d2d2d;"
        "  border-radius: 8px;"
        "}"));

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(40, 36, 40, 36);
    cardLayout->setSpacing(20);
    cardLayout->addWidget(titleLabel);
    cardLayout->addWidget(subtitleLabel);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(introLabel);
    cardLayout->addSpacing(8);
    cardLayout->addLayout(btnRow);
    cardLayout->addSpacing(8);
    cardLayout->addWidget(recentTitle);
    cardLayout->addWidget(m_recentList);

    // ══════════════════════════════════════════════════════════════════════
    // 主布局：居中显示卡片
    // ══════════════════════════════════════════════════════════════════════
    auto *hCenter = new QHBoxLayout;
    hCenter->addStretch();
    hCenter->addWidget(card, 0, Qt::AlignVCenter);
    hCenter->addStretch();

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addStretch();
    root->addLayout(hCenter);
    root->addSpacing(12);
    root->addWidget(footerLabel);
    root->addStretch();
}
