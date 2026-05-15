#include "eventtriggerdialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <utility>

namespace {
constexpr int kRoleName = Qt::UserRole + 1;
constexpr int kRoleDescription = Qt::UserRole + 2;

} // namespace

EventTriggerDialog::EventTriggerDialog(const WidgetMeta &meta,
                                       const QStringList &existingEvents,
                                       QWidget *parent)
    : QDialog(parent)
    , m_meta(meta)
    , m_existingEvents(existingEvents)
{
    setWindowTitle(tr("Trigger"));
    resize(310, 310);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 0);
    root->setSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("search item......"));
    root->addWidget(m_searchEdit);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->horizontalHeader()->hide();
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setShowGrid(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    root->addWidget(buttons);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { rebuildTable(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { accept(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildTable();
}

EventDef EventTriggerDialog::selectedEvent() const
{
    QTableWidgetItem *item = m_table->currentItem();
    if (!item) return {};
    EventDef def;
    def.name = item->data(kRoleName).toString();
    def.label = item->text();
    def.description = item->data(kRoleDescription).toString();
    return def;
}

void EventTriggerDialog::rebuildTable()
{
    const QString filter = m_searchEdit->text().trimmed();
    m_table->setRowCount(0);

    auto addCell = [this](int row, int column, const EventDef &def) {
        auto *item = new QTableWidgetItem(def.label.isEmpty() ? def.name : def.label);
        item->setData(kRoleName, def.name);
        item->setData(kRoleDescription, def.description);
        if (m_existingEvents.contains(def.name))
            item->setToolTip(tr("该事件已添加，可在事件行右侧继续添加动作"));
        else if (!def.description.isEmpty())
            item->setToolTip(def.description);
        m_table->setItem(row, column, item);
    };

    QList<EventDef> visible;
    for (const EventDef &def : triggerDefs()) {
        const QString label = def.label.isEmpty() ? def.name : def.label;
        if (!filter.isEmpty() &&
            !label.contains(filter, Qt::CaseInsensitive) &&
            !def.name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        visible.append(def);
    }

    for (int i = 0; i < visible.size(); i += 2) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        addCell(row, 0, visible[i]);
        if (i + 1 < visible.size())
            addCell(row, 1, visible[i + 1]);
    }

    if (m_table->rowCount() > 0)
        m_table->selectRow(0);
}

QList<EventDef> EventTriggerDialog::triggerDefs() const
{
    if (!m_meta.eventDefs.isEmpty())
        return m_meta.eventDefs;

    QList<EventDef> defs;
    for (const QString &eventName : m_meta.events) {
        EventDef def;
        def.name = eventName;
        def.label = eventName;
        defs.append(def);
    }

    for (const PropertyMeta &property : m_meta.eventProperties) {
        if (property.event.isEmpty()) continue;
        bool exists = false;
        for (const EventDef &def : std::as_const(defs)) {
            if (def.name == property.event) { exists = true; break; }
        }
        if (exists) continue;

        EventDef def;
        def.name = property.event;
        def.label = property.event;
        def.description = property.description;
        defs.append(def);
    }
    return defs;
}
