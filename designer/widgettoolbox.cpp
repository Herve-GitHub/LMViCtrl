#include "widgettoolbox.h"
#include "luawidgetparser.h"

#include <QApplication>
#include <QDrag>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// 自定义 MIME 类型
// ---------------------------------------------------------------------------
static constexpr char kWidgetMimeType[] = "application/x-lvgl-widget";
static const int const_itemHint = 40;
// ---------------------------------------------------------------------------
// 条目渲染代理：上行显示 name（粗体），下行显示 description（灰色小字）
// ---------------------------------------------------------------------------
class WidgetItemDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(0, const_itemHint);
    }
};

// ---------------------------------------------------------------------------
// 支持拖拽的列表控件（内部实现，不需要 Q_OBJECT）
// ---------------------------------------------------------------------------
class ToolboxListWidget : public QListWidget
{
public:
    explicit ToolboxListWidget(QWidget *parent = nullptr) : QListWidget(parent)
    {
        setDragEnabled(true);
        setDefaultDropAction(Qt::CopyAction);
        setDragDropMode(QAbstractItemView::DragOnly);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setItemDelegate(new WidgetItemDelegate(this));
        setSpacing(1);
        setFocusPolicy(Qt::StrongFocus);
    }

protected:
    void startDrag(Qt::DropActions /*supportedActions*/) override
    {
        QListWidgetItem *item = currentItem();
        if (!item) return;

        const WidgetMeta meta = qvariant_cast<WidgetMeta>(item->data(Qt::UserRole));
        if (meta.id.isEmpty()) return;

        // 序列化到 JSON
        QJsonObject obj;
        obj[QStringLiteral("widgetId")]    = meta.id;
        obj[QStringLiteral("luaFilePath")] = meta.luaFilePath;
        obj[QStringLiteral("name")]        = meta.name;

        auto *mimeData = new QMimeData;
        mimeData->setData(QLatin1String(kWidgetMimeType),
                          QJsonDocument(obj).toJson(QJsonDocument::Compact));

        // 拖拽预览图
        const QSize  ps(140, 36);
        QPixmap      pix(ps);
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
    setMinimumWidth(160);
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
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &WidgetToolbox::onFilterChanged);
    layout->addWidget(m_searchEdit);

    // 列表
    m_listWidget = new ToolboxListWidget(container);
    layout->addWidget(m_listWidget, 1);

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
    populateList();
}

void WidgetToolbox::onFilterChanged(const QString &text)
{
    populateList(text.trimmed());
}

void WidgetToolbox::populateList(const QString &filter)
{
    m_listWidget->clear();
    int shown = 0;
    for (const WidgetMeta &meta : std::as_const(m_metas)) {
        if (!filter.isEmpty()
            && !meta.name.contains(filter, Qt::CaseInsensitive)
            && !meta.id.contains(filter, Qt::CaseInsensitive))
        {
            continue;
        }

        auto *item = new QListWidgetItem(meta.name, m_listWidget);
        item->setData(Qt::UserRole,     QVariant::fromValue(meta));
        item->setData(Qt::UserRole + 1, meta.description);
        item->setData(Qt::DecorationRole + 1, meta.name);
        QString toolTip =QStringLiteral("%1\nid: %2\nv%3\n\n%4")
                              .arg(meta.name).arg( meta.id).arg( meta.version).arg(meta.description);
        item->setToolTip(toolTip);
        item->setSizeHint(QSize(0, const_itemHint));
        ++shown;
    }
    m_countLabel->setText(tr("%1 / %2 个组件").arg(shown).arg(m_metas.size()));
}
