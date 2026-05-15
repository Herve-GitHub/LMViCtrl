#include "widgettoolbox.h"
#include "luawidgetparser.h"

#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QFileInfo>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// 自定义 MIME 类型
// ---------------------------------------------------------------------------
static constexpr char kWidgetMimeType[] = "application/x-lvgl-widget";
static const int const_itemHint = 40;

// 预设类别顺序（与 LMViCtrl/lua/widgets/说明.txt 一致）
static const QStringList &preferredCategoryOrder()
{
    static const QStringList order = {
        QStringLiteral("基础对象"),
        QStringLiteral("交互输入"),
        QStringLiteral("数值与状态显示"),
        QStringLiteral("数据展示"),
        QStringLiteral("容器与页面组织"),
        QStringLiteral("绘制与媒体"),
        QStringLiteral("自定义控件"),
    };
    return order;
}

// ---------------------------------------------------------------------------
// 条目渲染代理
// ---------------------------------------------------------------------------
class WidgetItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &idx) const override
    {
        Q_UNUSED(opt);
        // 顶级（分类）项使用稍小行高
        if (!idx.parent().isValid())
            return QSize(0, 26);
        return QSize(0, const_itemHint);
    }
};

// ---------------------------------------------------------------------------
// 支持拖拽的树控件（仅叶子节点可拖动）
// ---------------------------------------------------------------------------
class ToolboxTreeWidget : public QTreeWidget
{
public:
    explicit ToolboxTreeWidget(QWidget *parent = nullptr) : QTreeWidget(parent)
    {
        setHeaderHidden(true);
        setColumnCount(1);
        setIndentation(12);
        setUniformRowHeights(false);
        setAnimated(true);
        setRootIsDecorated(true);
        setExpandsOnDoubleClick(true);
        setDragEnabled(true);
        setDefaultDropAction(Qt::CopyAction);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setItemDelegate(new WidgetItemDelegate(this));
        setFocusPolicy(Qt::StrongFocus);
        if (header()) header()->setSectionResizeMode(QHeaderView::Stretch);
    }

protected:
    void startDrag(Qt::DropActions /*supportedActions*/) override
    {
        QTreeWidgetItem *item = currentItem();
        if (!item || item->parent() == nullptr) return; // 顶级分类不可拖

        const WidgetMeta meta = qvariant_cast<WidgetMeta>(item->data(0, Qt::UserRole));
        if (meta.id.isEmpty()) return;

        QJsonObject obj;
        obj[QStringLiteral("widgetId")]    = meta.id;
        obj[QStringLiteral("luaFilePath")] = meta.luaFilePath;
        obj[QStringLiteral("name")]        = meta.name;

        auto *mimeData = new QMimeData;
        mimeData->setData(QLatin1String(kWidgetMimeType),
                          QJsonDocument(obj).toJson(QJsonDocument::Compact));

        // 拖拽预览图
        const QSize ps(140, 36);
        QPixmap pix(ps);
        pix.fill(Qt::transparent);
        {
            QPainter p(&pix);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(QColor("#1a6fa0"));
            p.setPen(QPen(QColor("#4db8ff"), 1));
            p.drawRoundedRect(QRect(QPoint(0, 0), ps).adjusted(1, 1, -1, -1), 5, 5);
            p.setPen(Qt::white);
            p.drawText(QRect(QPoint(0, 0), ps), Qt::AlignCenter, meta.name);
        }

        auto *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->setPixmap(pix);
        drag->setHotSpot(QPoint(ps.width() / 2, ps.height() / 2));
        drag->exec(Qt::CopyAction);
    }
};

// ---------------------------------------------------------------------------
// WidgetToolbox
// ---------------------------------------------------------------------------

WidgetToolbox::WidgetToolbox(QWidget *parent)
    : QDockWidget(tr("组件工具箱"), parent)
{
    buildUi();
    setMinimumWidth(180);
}

void WidgetToolbox::buildUi()
{
    auto *container = new QWidget(this);
    auto *layout    = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // 搜索框
    m_searchEdit = new QLineEdit(container);
    m_searchEdit->setPlaceholderText(tr("搜索组件…"));
    m_searchEdit->setStyleSheet(QStringLiteral("color: white;"));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &WidgetToolbox::onFilterChanged);
    layout->addWidget(m_searchEdit);

    // 树
    m_tree = new ToolboxTreeWidget(container);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #2b2b2b; color: #d8d8d8; border: 1px solid #3a3a3a; }"
        "QTreeWidget::item { padding: 2px 4px; }"
        "QTreeWidget::item:selected { background: #1a6fa0; color: white; }"
    ));
    layout->addWidget(m_tree, 1);

    // 底部计数标签
    m_countLabel = new QLabel(container);
    m_countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_countLabel->setStyleSheet(QStringLiteral("color: #707070; font-size: 9pt;"));
    layout->addWidget(m_countLabel);

    setWidget(container);
}

void WidgetToolbox::loadFromDirectory(const QString &dirPath)
{
    m_metas = LuaWidgetParser::parseDirectory(dirPath);
    populateTree();
}

void WidgetToolbox::onFilterChanged(const QString &text)
{
    populateTree(text.trimmed());
}

void WidgetToolbox::populateTree(const QString &filter)
{
    // 记住分组的展开状态
    QHash<QString, bool> wasExpanded;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *g = m_tree->topLevelItem(i);
        const QString cat = g->data(0, Qt::UserRole + 2).toString();
        if (!cat.isEmpty()) wasExpanded.insert(cat, g->isExpanded());
    }

    m_tree->clear();

    // 1) 收集每个分类下匹配过滤条件的 widgets
    QHash<QString, QList<const WidgetMeta*>> groups;
    QStringList extraGroupOrder;
    for (const WidgetMeta &meta : std::as_const(m_metas)) {
        if (!filter.isEmpty()) {
            bool hit = meta.name.contains(filter, Qt::CaseInsensitive)
                    || meta.id.contains(filter, Qt::CaseInsensitive)
                    || meta.description.contains(filter, Qt::CaseInsensitive);
            if (!hit) {
                for (const QString &t : meta.tags)
                    if (t.contains(filter, Qt::CaseInsensitive)) { hit = true; break; }
            }
            if (!hit) continue;
        }
        const QString cat = meta.category.isEmpty() ? tr("未分类") : meta.category;
        if (!preferredCategoryOrder().contains(cat) && !extraGroupOrder.contains(cat))
            extraGroupOrder << cat;
        groups[cat].append(&meta);
    }

    // 2) 按预设顺序 + 额外分类（按首次出现）输出
    QStringList finalOrder;
    for (const QString &c : preferredCategoryOrder()) finalOrder << c;
    for (const QString &c : std::as_const(extraGroupOrder)) finalOrder << c;

    // 解析 icon 路径
    auto resolveIcon = [](const WidgetMeta &m) -> QIcon {
        if (m.icon.isEmpty()) return {};
        const QFileInfo luaFi(m.luaFilePath);
        QStringList candidates;
        if (QFileInfo(m.icon).isAbsolute()) candidates << m.icon;
        if (luaFi.exists()) {
            candidates << QDir(luaFi.absolutePath()).absoluteFilePath(m.icon);
            candidates << QDir(luaFi.absolutePath() + QStringLiteral("/../..")).absoluteFilePath(m.icon);
        }
        candidates << QDir::current().absoluteFilePath(m.icon);
        for (const QString &p : std::as_const(candidates)) {
            if (QFileInfo::exists(p)) return QIcon(p);
        }
        return {};
    };

    int shown = 0;
    int groupIndex = 1;
    for (const QString &cat : std::as_const(finalOrder)) {
        const auto items = groups.value(cat);
        if (items.isEmpty()) continue;

        // 分组节点
        auto *group = new QTreeWidgetItem(m_tree);
        const QString head = QStringLiteral("%1. %2  (%3)")
                                 .arg(groupIndex++)
                                 .arg(cat)
                                 .arg(items.size());
        group->setText(0, head);
        QFont f = group->font(0);
        f.setBold(true);
        group->setFont(0, f);
        group->setForeground(0, QColor("#dcdcdc"));
        group->setBackground(0, QColor(60, 60, 60));
        group->setFlags(Qt::ItemIsEnabled);
        group->setData(0, Qt::UserRole + 2, cat); // 原始 cat 名（用于状态保存）

        // 子条目
        for (const WidgetMeta *pm : items) {
            const WidgetMeta &meta = *pm;
            auto *child = new QTreeWidgetItem(group);
            child->setText(0, meta.name);
            child->setData(0, Qt::UserRole, QVariant::fromValue(meta));

            const QIcon ic = resolveIcon(meta);
            if (!ic.isNull()) child->setIcon(0, ic);

            const QString toolTip = QStringLiteral("%1\nid: %2\ntype: %3\nv%4\n\n%5")
                                        .arg(meta.name, meta.id, meta.type, meta.version,
                                             meta.description);
            child->setToolTip(0, toolTip);
            child->setSizeHint(0, QSize(0, const_itemHint));
            child->setFlags(Qt::ItemIsEnabled
                            | Qt::ItemIsSelectable
                            | Qt::ItemIsDragEnabled
                            | Qt::ItemNeverHasChildren);
            ++shown;
        }

        // 恢复展开状态（首次或带过滤时默认展开）
        const bool expand = filter.isEmpty()
                              ? wasExpanded.value(cat, true)
                              : true;
        group->setExpanded(expand);
    }

    m_countLabel->setText(tr("%1 / %2 个组件").arg(shown).arg(m_metas.size()));
}
