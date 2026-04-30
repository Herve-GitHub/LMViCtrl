#include "newprojectdialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

// ─────────────────────────────────────────────────────────────────────────────
// 内部数据结构
// ─────────────────────────────────────────────────────────────────────────────
namespace {

struct TemplateInfo {
    QString id;
    QString name;
    QString description;  // 右侧详情
};

struct CategoryInfo {
    QString id;
    QString name;
    QList<TemplateInfo> templates;
};

static const QList<CategoryInfo> g_categories = {
    {
        QStringLiteral("custom"),
        QStringLiteral("自定义工程"),
        {
            {
                QStringLiteral("hmi"),
                QStringLiteral("组态工程"),
                QStringLiteral(
                    "<b>组态工程</b><br/><br/>"
                    "通用 HMI 组态工程，适用于工业控制面板、楼宇自动化、"
                    "设备监控等场景。<br/><br/>"
                    "<b>特性：</b><br/>"
                    "• 可视化拖拽布局，支持多图页管理<br/>"
                    "• 内置丰富组件库（按钮、仪表、图表、滑块等）<br/>"
                    "• 工程自动编译为 Lua 脚本，无缝运行在 LVGL 运行时<br/>"
                    "• 支持分辨率、颜色深度、目标平台灵活配置<br/><br/>"
                    "<b>默认分辨率：</b> 1024 × 768"
                )
            }
        }
    },
    {
        QStringLiteral("widget"),
        QStringLiteral("控件工程"),
        {
            {
                QStringLiteral("btn"),
                QStringLiteral("Button 工程"),
                QStringLiteral(
                    "<b>Button 控件工程</b><br/><br/>"
                    "以 LVGL <code>lv_btn</code> 为核心的单控件工程，"
                    "适合快速验证按钮交互逻辑或开发自定义按钮皮肤。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 一个预配置的 Button 画面<br/>"
                    "• 点击事件 Lua 回调模板<br/>"
                    "• 常用样式属性示例（圆角、颜色、字体）"
                )
            },
            {
                QStringLiteral("checkbox"),
                QStringLiteral("Checkbox 工程"),
                QStringLiteral(
                    "<b>Checkbox 控件工程</b><br/><br/>"
                    "以 LVGL <code>lv_checkbox</code> 为核心的单控件工程，"
                    "用于开关量输入场景。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 勾选/取消勾选事件回调模板<br/>"
                    "• 标签文字动态绑定示例<br/>"
                    "• 状态颜色样式配置"
                )
            },
            {
                QStringLiteral("slider"),
                QStringLiteral("Slider 工程"),
                QStringLiteral(
                    "<b>Slider 控件工程</b><br/><br/>"
                    "以 LVGL <code>lv_slider</code> 为核心的单控件工程，"
                    "适合模拟量调节场景（亮度、音量、速度等）。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 滑块值变化事件 Lua 回调<br/>"
                    "• 最小值/最大值/初始值配置<br/>"
                    "• 实时数值标签联动示例"
                )
            },
            {
                QStringLiteral("label"),
                QStringLiteral("Label 工程"),
                QStringLiteral(
                    "<b>Label 控件工程</b><br/><br/>"
                    "以 LVGL <code>lv_label</code> 为核心的单控件工程，"
                    "适合文字展示与动态数据绑定开发。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 静态文本与动态格式化字符串示例<br/>"
                    "• 长文本滚动（scroll）配置<br/>"
                    "• 多行对齐与字体颜色示例"
                )
            },
            {
                QStringLiteral("chart"),
                QStringLiteral("Chart 工程"),
                QStringLiteral(
                    "<b>Chart 控件工程</b><br/><br/>"
                    "以 LVGL <code>lv_chart</code> 为核心的单控件工程，"
                    "适用于实时数据曲线、历史趋势图开发。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 折线图与柱状图切换示例<br/>"
                    "• 定时器驱动的实时数据更新 Lua 模板<br/>"
                    "• X/Y 轴刻度与网格线配置"
                )
            }
        }
    },
    {
        QStringLiteral("template"),
        QStringLiteral("模板工程"),
        {
            {
                QStringLiteral("tpl_dashboard"),
                QStringLiteral("仪表盘模板"),
                QStringLiteral(
                    "<b>仪表盘模板工程</b><br/><br/>"
                    "包含多个预设仪表、进度条和状态指示灯的完整仪表盘布局，"
                    "可直接二次开发用于设备监控大屏。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• 模拟仪表（Arc + Label 组合）<br/>"
                    "• 多通道实时折线图<br/>"
                    "• 报警状态灯与告警列表<br/>"
                    "• 页面导航栏示例"
                )
            },
            {
                QStringLiteral("tpl_login"),
                QStringLiteral("登录界面模板"),
                QStringLiteral(
                    "<b>登录界面模板工程</b><br/><br/>"
                    "包含用户名/密码输入、登录按钮及错误提示的完整登录页面，"
                    "适用于需要权限控制的 HMI 场景。<br/><br/>"
                    "<b>包含内容：</b><br/>"
                    "• TextArea 输入框组合布局<br/>"
                    "• 密码掩码显示切换<br/>"
                    "• 登录验证 Lua 脚本模板<br/>"
                    "• 跳转主界面逻辑示例"
                )
            }
        }
    },
    {
        QStringLiteral("svg"),
        QStringLiteral("SVG 工程"),
        {
            {
                QStringLiteral("svg_basic"),
                QStringLiteral("SVG 工程"),
                QStringLiteral(
                    "<b>SVG 工程</b><br/><br/>"
                    "基于 LVGL SVG 渲染能力，将矢量图形嵌入 HMI 界面，"
                    "适合需要高精度图标或仪表盘刻度盘的应用场景。<br/><br/>"
                    "<b>特性：</b><br/>"
                    "• 支持 .svg 文件直接导入并在画布上预览<br/>"
                    "• SVG 元素属性（颜色、透明度）可通过 Lua 动态修改<br/>"
                    "• 适合工业仪表指针动画、矢量地图等高级 UI 场景<br/><br/>"
                    "<b>依赖：</b> LVGL SVG 扩展模块（需在 lv_conf.h 中启用）"
                )
            }
        }
    }
};

static QString kStyleCategoryList = QStringLiteral(
    "QListWidget {"
    "  background-color: #1e1e1e;"
    "  border: none;"
    "  outline: none;"
    "}"
    "QListWidget::item {"
    "  padding: 8px 12px;"
    "  color: #CCCCCC;"
    "  border-radius: 4px;"
    "}"
    "QListWidget::item:hover    { background-color: #2a2d2e; }"
    "QListWidget::item:selected { background-color: #094771; color: white; }");

static QString kStyleTemplateList = QStringLiteral(
    "QListWidget {"
    "  background-color: #252526;"
    "  border: none;"
    "  outline: none;"
    "}"
    "QListWidget::item {"
    "  padding: 10px 14px;"
    "  color: #CCCCCC;"
    "  border-bottom: 1px solid #333333;"
    "}"
    "QListWidget::item:hover    { background-color: #2a2d2e; }"
    "QListWidget::item:selected { background-color: #094771; color: white; }");

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 构造
// ─────────────────────────────────────────────────────────────────────────────
NewProjectDialog::NewProjectDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("新建工程"));
    setMinimumSize(860, 560);
    resize(920, 580);
    buildUi();
    populateCategories();
    if (m_categoryList->count() > 0)
        m_categoryList->setCurrentRow(0);
    m_nameEdit->setText(uniqueProjectName(QStringLiteral("NewProject")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 公共访问器
// ─────────────────────────────────────────────────────────────────────────────
QString NewProjectDialog::projectName()     const { return m_nameEdit->text().trimmed(); }
QString NewProjectDialog::projectLocation() const { return m_locationEdit->text().trimmed(); }
QString NewProjectDialog::templateId()      const { return m_templateId; }
QString NewProjectDialog::templateName()    const { return m_templateName; }

// ─────────────────────────────────────────────────────────────────────────────
// UI 构建
// ─────────────────────────────────────────────────────────────────────────────
void NewProjectDialog::buildUi()
{
    // ── 左栏：类别列表 ────────────────────────────────────────────────────────
    auto *catHeader = new QLabel(tr("工程类型"), this);
    catHeader->setStyleSheet(QStringLiteral(
        "color:#9E9E9E; font-size:9pt; padding:4px 12px;"));

    m_categoryList = new QListWidget(this);
    m_categoryList->setStyleSheet(kStyleCategoryList);
    m_categoryList->setFixedWidth(160);
    m_categoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *catPanel = new QWidget(this);
    catPanel->setFixedWidth(160);
    catPanel->setStyleSheet(QStringLiteral("background-color:#1e1e1e;"));
    auto *catLayout = new QVBoxLayout(catPanel);
    catLayout->setContentsMargins(0, 8, 0, 0);
    catLayout->setSpacing(0);
    catLayout->addWidget(catHeader);
    catLayout->addWidget(m_categoryList);

    // ── 中栏：模板列表 ────────────────────────────────────────────────────────
    auto *tplHeader = new QLabel(tr("模板"), this);
    tplHeader->setStyleSheet(QStringLiteral(
        "color:#9E9E9E; font-size:9pt; padding:4px 14px;"));

    m_templateList = new QListWidget(this);
    m_templateList->setStyleSheet(kStyleTemplateList);
    m_templateList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *tplPanel = new QWidget(this);
    tplPanel->setMinimumWidth(200);
    tplPanel->setStyleSheet(QStringLiteral("background-color:#252526;"));
    auto *tplLayout = new QVBoxLayout(tplPanel);
    tplLayout->setContentsMargins(0, 8, 0, 0);
    tplLayout->setSpacing(0);
    tplLayout->addWidget(tplHeader);
    tplLayout->addWidget(m_templateList);

    // ── 右栏：详情 ───────────────────────────────────────────────────────────
    m_detailTitle = new QLabel(this);
    m_detailTitle->setWordWrap(true);
    QFont tf = m_detailTitle->font();
    tf.setPointSize(12);
    tf.setBold(true);
    m_detailTitle->setFont(tf);
    m_detailTitle->setStyleSheet(QStringLiteral("color:#4FC3F7;"));

    m_detailDesc = new QLabel(this);
    m_detailDesc->setWordWrap(true);
    m_detailDesc->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detailDesc->setStyleSheet(QStringLiteral("color:#BDBDBD; font-size:10pt;"));
    m_detailDesc->setTextFormat(Qt::RichText);

    auto *detailPanel = new QWidget(this);
    detailPanel->setMinimumWidth(240);
    detailPanel->setStyleSheet(QStringLiteral(
        "background-color:#2d2d2d;"
        "border-left:1px solid #3c3c3c;"));
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(20, 20, 20, 20);
    detailLayout->setSpacing(12);
    detailLayout->addWidget(m_detailTitle);
    detailLayout->addWidget(m_detailDesc);
    detailLayout->addStretch();

    // ── 三栏拼接 ──────────────────────────────────────────────────────────────
    auto *topSplitter = new QSplitter(Qt::Horizontal, this);
    topSplitter->setHandleWidth(1);
    topSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background-color: #3c3c3c; }"));
    topSplitter->addWidget(catPanel);
    topSplitter->addWidget(tplPanel);
    topSplitter->addWidget(detailPanel);
    topSplitter->setStretchFactor(0, 0);
    topSplitter->setStretchFactor(1, 1);
    topSplitter->setStretchFactor(2, 2);

    // ── 分隔线 ────────────────────────────────────────────────────────────────
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setStyleSheet(QStringLiteral("color:#3c3c3c;"));

    // ── 底部：工程名称 + 存储路径 ─────────────────────────────────────────────
    auto *nameLabel     = new QLabel(tr("工程名称："), this);
    auto *locationLabel = new QLabel(tr("存储路径："), this);
    for (auto *l : {nameLabel, locationLabel})
        l->setStyleSheet(QStringLiteral("color:#CCCCCC;"));

    m_nameEdit = new QLineEdit(QStringLiteral("NewProject"), this);
    m_nameEdit->setPlaceholderText(tr("输入工程名称"));

    m_locationEdit = new QLineEdit(this);
    m_locationEdit->setPlaceholderText(tr("选择保存目录"));
    m_locationEdit->setText(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    m_browseBtn = new QToolButton(this);
    m_browseBtn->setText(QStringLiteral("..."));
    m_browseBtn->setToolTip(tr("浏览…"));

    for (auto *e : {m_nameEdit, m_locationEdit}) {
        e->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            "  background-color:#3c3c3c;"
            "  border:1px solid #555555;"
            "  border-radius:3px;"
            "  color:#CCCCCC;"
            "  padding:4px 6px;"
            "}"));
    }
    m_browseBtn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background-color:#3c3c3c;"
        "  border:1px solid #555555;"
        "  border-radius:3px;"
        "  color:#CCCCCC;"
        "  padding:4px 8px;"
        "}"
        "QToolButton:hover { background-color:#505050; }"));

    auto *formLayout = new QGridLayout;
    formLayout->setColumnStretch(1, 1);
    formLayout->setHorizontalSpacing(8);
    formLayout->setVerticalSpacing(8);
    formLayout->addWidget(nameLabel,     0, 0);
    formLayout->addWidget(m_nameEdit,    0, 1, 1, 2);
    formLayout->addWidget(locationLabel, 1, 0);
    formLayout->addWidget(m_locationEdit,1, 1);
    formLayout->addWidget(m_browseBtn,   1, 2);

    // 错误提示
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(QStringLiteral("color:#F44747; font-size:9pt;"));
    m_errorLabel->setVisible(false);

    // 按钮行
    m_createBtn = new QPushButton(tr("创建"), this);
    m_createBtn->setDefault(true);
    m_createBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#007ACC;"
        "  color:white;"
        "  border-radius:4px;"
        "  padding:6px 24px;"
        "  font-size:10pt;"
        "}"
        "QPushButton:hover   { background-color:#1C97EA; }"
        "QPushButton:pressed { background-color:#005F9E; }"
        "QPushButton:disabled{ background-color:#3c3c3c; color:#666; }"));

    auto *cancelBtn = new QPushButton(tr("取消"), this);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background-color:#3c3c3c;"
        "  color:#CCCCCC;"
        "  border:1px solid #555555;"
        "  border-radius:4px;"
        "  padding:6px 20px;"
        "  font-size:10pt;"
        "}"
        "QPushButton:hover   { background-color:#505050; }"
        "QPushButton:pressed { background-color:#2a2a2a; }"));

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_errorLabel);
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addSpacing(8);
    btnRow->addWidget(m_createBtn);

    auto *bottomLayout = new QVBoxLayout;
    bottomLayout->setContentsMargins(16, 12, 16, 12);
    bottomLayout->setSpacing(8);
    bottomLayout->addLayout(formLayout);
    bottomLayout->addLayout(btnRow);

    // ── 总布局 ────────────────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(topSplitter, 1);
    root->addWidget(separator);
    root->addLayout(bottomLayout);

    // ── 信号连接 ─────────────────────────────────────────────────────────────
    connect(m_categoryList, &QListWidget::currentItemChanged,
            this, &NewProjectDialog::onCategoryChanged);
    connect(m_templateList, &QListWidget::currentItemChanged,
            this, &NewProjectDialog::onTemplateChanged);
    connect(m_browseBtn, &QToolButton::clicked,
            this, &NewProjectDialog::onBrowseLocation);
    connect(m_nameEdit, &QLineEdit::textChanged,
            this, &NewProjectDialog::onProjectNameChanged);
    connect(m_createBtn, &QPushButton::clicked,
            this, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(m_locationEdit, &QLineEdit::textChanged,
        this, &NewProjectDialog::validate);
}

// ─────────────────────────────────────────────────────────────────────────────
// 数据填充
// ─────────────────────────────────────────────────────────────────────────────
void NewProjectDialog::populateCategories()
{
    m_categoryList->clear();
    for (const CategoryInfo &cat : g_categories) {
        auto *item = new QListWidgetItem(cat.name);
        item->setData(Qt::UserRole, cat.id);
        m_categoryList->addItem(item);
    }
}

void NewProjectDialog::populateTemplates(const QString &categoryId)
{
    m_templateList->clear();
    for (const CategoryInfo &cat : g_categories) {
        if (cat.id != categoryId) continue;
        for (const TemplateInfo &tpl : cat.templates) {
            auto *item = new QListWidgetItem(tpl.name);
            item->setData(Qt::UserRole,     tpl.id);
            item->setData(Qt::UserRole + 1, tpl.description);
            item->setData(Qt::UserRole + 2, tpl.name);
            m_templateList->addItem(item);
        }
        break;
    }
    if (m_templateList->count() > 0)
        m_templateList->setCurrentRow(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// 槽
// ─────────────────────────────────────────────────────────────────────────────
void NewProjectDialog::onCategoryChanged(QListWidgetItem *current,
                                         QListWidgetItem * /*previous*/)
{
    if (!current) return;
    populateTemplates(current->data(Qt::UserRole).toString());
}

void NewProjectDialog::onTemplateChanged(QListWidgetItem *current,
                                         QListWidgetItem * /*previous*/)
{
    if (!current) {
        m_detailTitle->clear();
        m_detailDesc->clear();
        m_templateId.clear();
        m_templateName.clear();
        validate();
        return;
    }
    m_templateId   = current->data(Qt::UserRole).toString();
    m_templateName = current->data(Qt::UserRole + 2).toString();
    m_detailTitle->setText(m_templateName);
    m_detailDesc->setText(current->data(Qt::UserRole + 1).toString());
    validate();
}

void NewProjectDialog::onBrowseLocation()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("选择存储路径"),
        m_locationEdit->text().isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            : m_locationEdit->text());
    if (!dir.isEmpty())
        m_locationEdit->setText(dir);
}

void NewProjectDialog::onProjectNameChanged(const QString &)
{
    validate();
}

void NewProjectDialog::validate()
{
    const QString name = m_nameEdit->text().trimmed();
    QString error;

    if (name.isEmpty()) {
        error = tr("工程名称不能为空。");
    } else if (name.contains(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")))) {
        error = tr("工程名称包含非法字符。");
    } else if (m_templateId.isEmpty()) {
        error = tr("请选择一个工程模板。");
    }
    if (error.isEmpty()) {
        const QString loc = m_locationEdit->text().trimmed();
        if (!loc.isEmpty()) {
            const QString projFile = loc + QLatin1Char('/') + name
                                    + QStringLiteral(".json");
            if (QDir(loc).exists(name + QStringLiteral(".json"))
                || QDir(loc).exists(name)) {
                error = tr("该路径下已存在同名工程，请修改工程名称。");
            }
        }
    }
    m_errorLabel->setVisible(!error.isEmpty());
    m_errorLabel->setText(error);
    m_createBtn->setEnabled(error.isEmpty());
}
QString NewProjectDialog::uniqueProjectName(const QString &base) const
{
    const QString loc = m_locationEdit->text().trimmed();
    if (loc.isEmpty()) return base;
    QString name = base;
    int i = 1;
    while (QDir(loc).exists(name + QStringLiteral(".json"))
           || QDir(loc).exists(name)) {
        name = base + QString::number(i++);
    }
    return name;
}