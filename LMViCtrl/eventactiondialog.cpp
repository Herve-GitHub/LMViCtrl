#include "eventactiondialog.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QUuid>

#include <utility>

namespace {

constexpr int kRoleId = Qt::UserRole + 1;
constexpr int kRoleName = Qt::UserRole + 2;

QString actionLabel(const QString &type)
{
    if (type == QLatin1String("load_screen")) return QObject::tr("Load Screen");
    if (type == QLatin1String("custom_code")) return QObject::tr("Custom Code");
    if (type == QLatin1String("set_property")) return QObject::tr("Set Property");
    if (type == QLatin1String("call_method")) return QObject::tr("Call Method");
    if (type == QLatin1String("freemaster")) return QObject::tr("Freemaster");
    return type;
}

} // namespace

EventActionDialog::EventActionDialog(const QString &eventName,
                                     const QList<ScreenData> &screens,
                                     const QList<WidgetInstance> &widgets,
                                     QWidget *parent)
    : QDialog(parent)
    , m_eventName(eventName)
    , m_screens(screens)
    , m_widgets(widgets)
{
    setWindowTitle(tr("配置事件动作 - %1").arg(eventName));
    resize(560, 420);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *topForm = new QFormLayout;
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Load Screen"), QStringLiteral("load_screen"));
    m_typeCombo->addItem(tr("Custom Code"), QStringLiteral("custom_code"));
    m_typeCombo->addItem(tr("Set Property"), QStringLiteral("set_property"));
    m_typeCombo->addItem(tr("Call Method"), QStringLiteral("call_method"));
    m_typeCombo->addItem(tr("Freemaster"), QStringLiteral("freemaster"));
    topForm->addRow(tr("动作"), m_typeCombo);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("search item......"));
    topForm->addRow(tr("Target"), m_searchEdit);
    root->addLayout(topForm);

    m_targetTable = new QTableWidget(this);
    m_targetTable->setColumnCount(2);
    m_targetTable->setHorizontalHeaderLabels({tr("类型"), tr("目标")});
    m_targetTable->horizontalHeader()->setStretchLastSection(true);
    m_targetTable->verticalHeader()->setVisible(false);
    m_targetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_targetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_targetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_targetTable, 1);

    m_detailStack = new QStackedWidget(this);
    auto *emptyPage = new QWidget(m_detailStack);
    m_detailStack->addWidget(emptyPage);

    m_codeEdit = new QPlainTextEdit(m_detailStack);
    m_codeEdit->setPlaceholderText(tr("-- self 为当前控件实例，e 为事件码\nprint('event triggered')"));
    m_detailStack->addWidget(m_codeEdit);

    auto *propPage = new QWidget(m_detailStack);
    auto *propForm = new QFormLayout(propPage);
    m_propertyEdit = new QLineEdit(propPage);
    m_propertyEdit->setPlaceholderText(tr("属性名，例如 label"));
    m_valueEdit = new QLineEdit(propPage);
    m_valueEdit->setPlaceholderText(tr("属性值"));
    propForm->addRow(tr("属性"), m_propertyEdit);
    propForm->addRow(tr("值"), m_valueEdit);
    m_detailStack->addWidget(propPage);

    auto *methodPage = new QWidget(m_detailStack);
    auto *methodForm = new QFormLayout(methodPage);
    m_methodEdit = new QLineEdit(methodPage);
    m_methodEdit->setPlaceholderText(tr("方法名，例如 set_property"));
    methodForm->addRow(tr("方法"), m_methodEdit);
    m_detailStack->addWidget(methodPage);

    auto *freemasterPage = new QWidget(m_detailStack);
    auto *fmForm = new QFormLayout(freemasterPage);
    m_freemasterTargetEdit = new QLineEdit(freemasterPage);
    m_freemasterTargetEdit->setPlaceholderText(tr("变量名，例如 sw_1"));
    m_freemasterValueEdit = new QLineEdit(freemasterPage);
    m_freemasterValueEdit->setPlaceholderText(tr("写入值，可为空"));
    fmForm->addRow(tr("变量"), m_freemasterTargetEdit);
    fmForm->addRow(tr("值"), m_freemasterValueEdit);
    m_detailStack->addWidget(freemasterPage);
    root->addWidget(m_detailStack);

    m_enabledCheck = new QCheckBox(tr("启用"), this);
    m_enabledCheck->setChecked(true);
    root->addWidget(m_enabledCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    root->addWidget(buttons);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateEditorState();
        rebuildTargetTable();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { rebuildTargetTable(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateEditorState();
    rebuildTargetTable();
}

void EventActionDialog::setAction(const EventAction &action)
{
    m_initialAction = action;
    const int idx = m_typeCombo->findData(action.type);
    if (idx >= 0) m_typeCombo->setCurrentIndex(idx);
    m_codeEdit->setPlainText(action.code);
    m_propertyEdit->setText(action.params.value(QStringLiteral("property")).toString());
    m_valueEdit->setText(action.params.value(QStringLiteral("value")).toString());
    m_methodEdit->setText(action.method.isEmpty()
                              ? action.params.value(QStringLiteral("method")).toString()
                              : action.method);
    m_freemasterTargetEdit->setText(action.targetName);
    m_freemasterValueEdit->setText(action.params.value(QStringLiteral("value")).toString());
    m_enabledCheck->setChecked(action.enabled);
    updateEditorState();
    rebuildTargetTable();

    for (int row = 0; row < m_targetTable->rowCount(); ++row) {
        const QString id = m_targetTable->item(row, 0)->data(kRoleId).toString();
        const QString name = m_targetTable->item(row, 0)->data(kRoleName).toString();
        if ((!action.targetId.isEmpty() && id == action.targetId) ||
            (!action.targetName.isEmpty() && name == action.targetName)) {
            m_targetTable->selectRow(row);
            break;
        }
    }
}

EventAction EventActionDialog::action() const
{
    EventAction action = m_initialAction;
    if (action.id.isEmpty())
        action.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

    action.type = currentActionType();
    action.label = currentActionLabel();
    action.enabled = m_enabledCheck->isChecked();
    action.targetId.clear();
    action.targetName.clear();
    action.method.clear();
    action.params.clear();
    action.code.clear();

    const int row = m_targetTable->currentRow();
    if (row >= 0) {
        action.targetId = m_targetTable->item(row, 0)->data(kRoleId).toString();
        action.targetName = m_targetTable->item(row, 0)->data(kRoleName).toString();
    }

    if (action.type == QLatin1String("load_screen")) {
        action.method = QStringLiteral("goto_page");
    } else if (action.type == QLatin1String("custom_code")) {
        action.code = m_codeEdit->toPlainText();
    } else if (action.type == QLatin1String("set_property")) {
        action.method = QStringLiteral("set_property");
        action.params.insert(QStringLiteral("property"), m_propertyEdit->text());
        action.params.insert(QStringLiteral("value"), m_valueEdit->text());
    } else if (action.type == QLatin1String("call_method")) {
        action.method = m_methodEdit->text();
        action.params.insert(QStringLiteral("method"), m_methodEdit->text());
    } else if (action.type == QLatin1String("freemaster")) {
        action.targetName = m_freemasterTargetEdit->text();
        action.method = QStringLiteral("write");
        action.params.insert(QStringLiteral("value"), m_freemasterValueEdit->text());
    }
    return action;
}

void EventActionDialog::rebuildTargetTable()
{
    const QString type = currentActionType();
    const QString filter = m_searchEdit->text().trimmed();
    m_targetTable->setRowCount(0);

    auto addRow = [this, &filter](const QString &kind, const QString &id, const QString &name) {
        if (!filter.isEmpty() && !kind.contains(filter, Qt::CaseInsensitive) &&
            !name.contains(filter, Qt::CaseInsensitive) &&
            !id.contains(filter, Qt::CaseInsensitive)) {
            return;
        }
        const int row = m_targetTable->rowCount();
        m_targetTable->insertRow(row);
        auto *kindItem = new QTableWidgetItem(kind);
        kindItem->setData(kRoleId, id);
        kindItem->setData(kRoleName, name);
        auto *nameItem = new QTableWidgetItem(name);
        nameItem->setData(kRoleId, id);
        nameItem->setData(kRoleName, name);
        m_targetTable->setItem(row, 0, kindItem);
        m_targetTable->setItem(row, 1, nameItem);
    };

    if (type == QLatin1String("load_screen")) {
        for (const ScreenData &screen : std::as_const(m_screens))
            addRow(tr("Load Screen"), screen.id, screen.name);
    } else if (type == QLatin1String("set_property") || type == QLatin1String("call_method")) {
        for (const WidgetInstance &widget : std::as_const(m_widgets))
            addRow(widget.widgetId, widget.instanceId, widget.name.isEmpty() ? widget.widgetId : widget.name);
    }

    if (m_targetTable->rowCount() > 0)
        m_targetTable->selectRow(0);
}

void EventActionDialog::updateEditorState()
{
    const QString type = currentActionType();
    const bool needsTarget = type == QLatin1String("load_screen") ||
                             type == QLatin1String("set_property") ||
                             type == QLatin1String("call_method");
    m_searchEdit->setEnabled(needsTarget);
    m_targetTable->setEnabled(needsTarget);

    if (type == QLatin1String("custom_code"))
        m_detailStack->setCurrentIndex(1);
    else if (type == QLatin1String("set_property"))
        m_detailStack->setCurrentIndex(2);
    else if (type == QLatin1String("call_method"))
        m_detailStack->setCurrentIndex(3);
    else if (type == QLatin1String("freemaster"))
        m_detailStack->setCurrentIndex(4);
    else
        m_detailStack->setCurrentIndex(0);
}

QString EventActionDialog::currentActionType() const
{
    return m_typeCombo->currentData().toString();
}

QString EventActionDialog::currentActionLabel() const
{
    return actionLabel(currentActionType());
}
