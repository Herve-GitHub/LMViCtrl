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
    constexpr int kRolePropertyName = Qt::UserRole + 3;

    QString actionLabel(const QString& type)
    {
        if (type == QLatin1String("load_screen")) return QObject::tr("Load Screen");
        if (type == QLatin1String("custom_code")) return QObject::tr("Custom Code");
        if (type == QLatin1String("set_property")) return QObject::tr("Set Property");
        if (type == QLatin1String("call_method")) return QObject::tr("Call Method");
        if (type == QLatin1String("freemaster")) return QObject::tr("Freemaster");
        if (type == QLatin1String("datewrite")) return QObject::tr("DateWrite");
        if (type == QLatin1String("valuechange")) return QObject::tr("ValueChange");
        return type;
    }

} // namespace

EventActionDialog::EventActionDialog(const QString& eventName,
    const QList<ScreenData>& screens,
    const QList<WidgetInstance>& widgets,
    const QList<WidgetMeta>& widgetMetas,
    QWidget* parent)
    : QDialog(parent)
    , m_eventName(eventName)
    , m_screens(screens)
    , m_widgets(widgets)
{
    for (const WidgetMeta& meta : std::as_const(widgetMetas))
        m_widgetMetas.insert(meta.id, meta);

    setWindowTitle(tr("配置事件动作 - %1").arg(eventName));
    resize(560, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto* topForm = new QFormLayout;
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Load Screen"), QStringLiteral("load_screen"));
    m_typeCombo->addItem(tr("Custom Code"), QStringLiteral("custom_code"));
    m_typeCombo->addItem(tr("Set Property"), QStringLiteral("set_property"));
    m_typeCombo->addItem(tr("Call Method"), QStringLiteral("call_method"));
    m_typeCombo->addItem(tr("Freemaster"), QStringLiteral("freemaster"));
    m_typeCombo->addItem(tr("DateWrite"), QStringLiteral("datewrite"));
    m_typeCombo->addItem(tr("ValueChange"), QStringLiteral("valuechange"));
    topForm->addRow(tr("动作"), m_typeCombo);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("search item......"));
    topForm->addRow(tr("Target"), m_searchEdit);
    root->addLayout(topForm);

    m_targetTable = new QTableWidget(this);
    m_targetTable->setColumnCount(2);
    m_targetTable->setHorizontalHeaderLabels({ tr("类型"), tr("目标") });
    m_targetTable->horizontalHeader()->setStretchLastSection(true);
    m_targetTable->verticalHeader()->setVisible(false);
    m_targetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_targetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_targetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    root->addWidget(m_targetTable, 1);

    m_detailStack = new QStackedWidget(this);
    auto* emptyPage = new QWidget(m_detailStack);
    m_detailStack->addWidget(emptyPage);

    m_codeEdit = new QPlainTextEdit(m_detailStack);
    m_codeEdit->setPlaceholderText(tr("-- self 为当前控件实例，e 为事件码\nprint('event triggered')"));
    m_detailStack->addWidget(m_codeEdit);

    auto* propPage = new QWidget(m_detailStack);
    auto* propForm = new QFormLayout(propPage);
    m_propertyCombo = new QComboBox(propPage);
    m_propertyCombo->setEditable(true);
    m_propertyCombo->setInsertPolicy(QComboBox::NoInsert);
    m_propertyCombo->setPlaceholderText(tr("属性名，例如 label"));
    m_valueEdit = new QLineEdit(propPage);
    m_valueEdit->setPlaceholderText(tr("属性值"));
    propForm->addRow(tr("属性"), m_propertyCombo);
    propForm->addRow(tr("值"), m_valueEdit);
    m_detailStack->addWidget(propPage);

    auto* methodPage = new QWidget(m_detailStack);
    auto* methodForm = new QFormLayout(methodPage);
    m_methodEdit = new QLineEdit(methodPage);
    m_methodEdit->setPlaceholderText(tr("方法名，例如 set_property"));
    methodForm->addRow(tr("方法"), m_methodEdit);
    m_detailStack->addWidget(methodPage);

    auto* freemasterPage = new QWidget(m_detailStack);
    auto* fmForm = new QFormLayout(freemasterPage);
    m_freemasterTargetEdit = new QLineEdit(freemasterPage);
    m_freemasterTargetEdit->setPlaceholderText(tr("变量名，例如 sw_1"));
    m_freemasterValueEdit = new QLineEdit(freemasterPage);
    m_freemasterValueEdit->setPlaceholderText(tr("写入值，可为空"));
    fmForm->addRow(tr("变量"), m_freemasterTargetEdit);
    fmForm->addRow(tr("值"), m_freemasterValueEdit);
    m_detailStack->addWidget(freemasterPage);
    root->addWidget(m_detailStack);

    auto* dateWritePage = new QWidget(m_detailStack);
    auto* dateWriteForm = new QFormLayout(dateWritePage);
    m_dataPointCombo = new QComboBox(dateWritePage);
    m_dataPointCombo->setEditable(true);
    m_dataPointCombo->setInsertPolicy(QComboBox::NoInsert);
    m_dataPointCombo->setPlaceholderText(tr("选择数据点"));

    // 改为独立变量
    m_dateWriteValueEdit = new QLineEdit(dateWritePage);
    m_dateWriteValueEdit->setPlaceholderText(tr("输入值"));

    dateWriteForm->addRow(tr("数据点"), m_dataPointCombo);
    dateWriteForm->addRow(tr("值"), m_dateWriteValueEdit);
    m_detailStack->addWidget(dateWritePage);


    // ====================== 值变化 页面 ======================
    auto* valueChangePage = new QWidget(m_detailStack);
    auto* valueChangeForm = new QFormLayout(valueChangePage);

    // 1. 数据点下拉
    m_valuePointCombo = new QComboBox(valueChangePage);
    m_valuePointCombo->setEditable(true);
    m_valuePointCombo->setInsertPolicy(QComboBox::NoInsert);
    m_valuePointCombo->setPlaceholderText(tr("选择数据点"));

    // 2. 起始值输入框（独立）
    m_valueStartEdit = new QLineEdit(valueChangePage);
    m_valueStartEdit->setPlaceholderText(tr("起始值"));

    // 3. 末尾值输入框（独立）
    m_valueEndEdit = new QLineEdit(valueChangePage);
    m_valueEndEdit->setPlaceholderText(tr("末尾值"));

    // 4. 属性下拉（独立）
    m_valuePropCombo = new QComboBox(valueChangePage);
    m_valuePropCombo->setEditable(true);
    m_valuePropCombo->setInsertPolicy(QComboBox::NoInsert);
    m_valuePropCombo->setPlaceholderText(tr("选择属性"));

    // 5. 最终值输入框（独立）
    m_valueFinalEdit = new QLineEdit(valueChangePage);
    m_valueFinalEdit->setPlaceholderText(tr("属性值"));

    // 表单布局
    valueChangeForm->addRow(tr("数据点"), m_valuePointCombo);
    valueChangeForm->addRow(tr("起始值"), m_valueStartEdit);
    valueChangeForm->addRow(tr("末尾值"), m_valueEndEdit);
    valueChangeForm->addRow(tr("属性"), m_valuePropCombo);
    valueChangeForm->addRow(tr("值"), m_valueFinalEdit);

    // 加入堆栈
    m_detailStack->addWidget(valueChangePage);





    m_enabledCheck = new QCheckBox(tr("启用"), this);
    m_enabledCheck->setChecked(true);
    root->addWidget(m_enabledCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    root->addWidget(buttons);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateEditorState();
        rebuildTargetTable();
        });
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() { rebuildTargetTable(); });
    connect(m_targetTable, &QTableWidget::currentCellChanged, this,
        [this](int, int, int, int) { rebuildPropertyCombo(); });
    connect(m_propertyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const QString propertyName = currentPropertyName();
        if (propertyName.isEmpty()) return;

        const WidgetInstance widget = currentTargetWidget();
        const WidgetMeta meta = m_widgetMetas.value(widget.widgetId);
        for (const PropertyMeta& property : meta.properties) {
            if (property.name == propertyName) {
                m_valueEdit->setText(valueTextForProperty(widget, property));
                return;
            }
        }
        });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateEditorState();
    rebuildTargetTable();
    rebuildPropertyCombo();
}


















void EventActionDialog::setAction(const EventAction& action)
{
    m_initialAction = action;
    const int idx = m_typeCombo->findData(action.type);
    if (idx >= 0) m_typeCombo->setCurrentIndex(idx);
    m_codeEdit->setPlainText(action.code);
    const QString propertyName = action.params.value(QStringLiteral("property")).toString();
    const QString propertyValue = action.params.value(QStringLiteral("value")).toString();
    m_propertyCombo->setEditText(propertyName);
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

    rebuildPropertyCombo(propertyName, false);
    m_propertyCombo->setEditText(propertyName);
    m_valueEdit->setText(propertyValue);
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
    }
    else if (action.type == QLatin1String("custom_code")) {
        action.code = m_codeEdit->toPlainText();
    }
    else if (action.type == QLatin1String("set_property")) {
        action.method = QStringLiteral("set_property");
        action.params.insert(QStringLiteral("property"), currentPropertyName());
        action.params.insert(QStringLiteral("value"), m_valueEdit->text());
    }
    else if (action.type == QLatin1String("call_method")) {
        action.method = m_methodEdit->text();
        action.params.insert(QStringLiteral("method"), m_methodEdit->text());
    }
    else if (action.type == QLatin1String("freemaster")) {
        action.targetName = m_freemasterTargetEdit->text();
        action.method = QStringLiteral("write");
        action.params.insert(QStringLiteral("value"), m_freemasterValueEdit->text());
    }
    else if (action.type == QLatin1String("datewrite")) {
        action.method = QStringLiteral("datewrite");
        action.params.insert(QStringLiteral("datapoint"), m_dataPointCombo->currentText());
        action.params.insert(QStringLiteral("value"), m_dateWriteValueEdit->text());
    }
    else if (action.type == QLatin1String("valuechange")) {
        action.method = QStringLiteral("valuechange");
        action.params.insert(QStringLiteral("datapoint"), m_valuePointCombo->currentText());
        action.params.insert(QStringLiteral("start"), m_valueStartEdit->text());
        action.params.insert(QStringLiteral("final"), m_valueEndEdit->text());
        QVariant propData = m_valuePropCombo->currentData();
        QString propName = propData.isValid() ? propData.toString() : m_valuePropCombo->currentText();
        action.params.insert(QStringLiteral("property"), propName);
        action.params.insert(QStringLiteral("value"), m_valueFinalEdit->text());
    }
    return action;
}

void EventActionDialog::rebuildTargetTable()
{
    const QString type = currentActionType();
    const QString filter = m_searchEdit->text().trimmed();
    m_targetTable->setRowCount(0);

    auto addRow = [this, &filter](const QString& kind, const QString& id, const QString& name) {
        if (!filter.isEmpty() && !kind.contains(filter, Qt::CaseInsensitive) &&
            !name.contains(filter, Qt::CaseInsensitive) &&
            !id.contains(filter, Qt::CaseInsensitive)) {
            return;
        }
        const int row = m_targetTable->rowCount();
        m_targetTable->insertRow(row);
        auto* kindItem = new QTableWidgetItem(kind);
        kindItem->setData(kRoleId, id);
        kindItem->setData(kRoleName, name);
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setData(kRoleId, id);
        nameItem->setData(kRoleName, name);
        m_targetTable->setItem(row, 0, kindItem);
        m_targetTable->setItem(row, 1, nameItem);
        };

    if (type == QLatin1String("load_screen")) {
        for (const ScreenData& screen : std::as_const(m_screens))
            addRow(tr("Load Screen"), screen.id, screen.name);
    }
    else if (type == QLatin1String("set_property") || type == QLatin1String("call_method") || type == QLatin1String("datewrite") || type == QLatin1String("valuechange")) {
        for (const WidgetInstance& widget : std::as_const(m_widgets))
            addRow(widget.widgetId, widget.instanceId, widget.name.isEmpty() ? widget.widgetId : widget.name);
    }

    if (m_targetTable->rowCount() > 0)
        m_targetTable->selectRow(0);
    rebuildPropertyCombo();
}

void EventActionDialog::rebuildPropertyCombo(const QString& preferredProperty, bool updateValue)
{
    if (!m_propertyCombo) return;

    const QString previous = preferredProperty.isEmpty()
        ? m_propertyCombo->currentText()
        : preferredProperty;
    QSignalBlocker blocker(m_propertyCombo);
    m_propertyCombo->clear();


    if (currentActionType() == "datewrite") {
        m_dataPointCombo->clear();
        m_dataPointCombo->addItem("系统时间");
        m_dataPointCombo->addItem("系统日期");
        m_dataPointCombo->addItem("温度");
        m_dataPointCombo->addItem("湿度");
        return;
    }

    // ✅ 新增：如果是 ValueChange，填充 【数据点】 + 【属性】
    if (currentActionType() == "valuechange") {
        // 1. 填充左边：数据点下拉框（固定选项）
        m_valuePointCombo->clear();
        m_valuePointCombo->addItem("系统时间");
        m_valuePointCombo->addItem("系统日期");
        m_valuePointCombo->addItem("温度");
        m_valuePointCombo->addItem("湿度");

        // 2. 填充右边：属性下拉框（控件属性，和 set_property 一样）
        m_valuePropCombo->clear();
        const WidgetInstance widget = currentTargetWidget();
        const WidgetMeta meta = m_widgetMetas.value(widget.widgetId);
        for (const PropertyMeta& property : meta.properties) {
            if (property.hidden) continue;
            QString shown = property.label.isEmpty() || property.label == property.name
                ? property.name
                : QStringLiteral("%1 (%2)").arg(property.label, property.name);
            m_valuePropCombo->addItem(shown, property.name);
        }
        return;
    }




    const WidgetInstance widget = currentTargetWidget();
    const WidgetMeta meta = m_widgetMetas.value(widget.widgetId);
    int selectedIndex = -1;
    for (const PropertyMeta& property : meta.properties) {
        if (property.hidden) continue;
        const QString shown = property.label.isEmpty() || property.label == property.name
            ? property.name
            : QStringLiteral("%1 (%2)").arg(property.label, property.name);
        const int index = m_propertyCombo->count();
        m_propertyCombo->addItem(shown, property.name);
        m_propertyCombo->setItemData(index, property.name, kRolePropertyName);
        if (!property.description.isEmpty())
            m_propertyCombo->setItemData(index, property.description, Qt::ToolTipRole);
        if (property.name == previous)
            selectedIndex = index;
    }

    if (selectedIndex >= 0) {
        m_propertyCombo->setCurrentIndex(selectedIndex);
    }
    else if (!previous.isEmpty()) {
        m_propertyCombo->setEditText(previous);
    }
    else if (m_propertyCombo->count() > 0) {
        m_propertyCombo->setCurrentIndex(0);
    }

    if (!updateValue) return;

    const QString propertyName = currentPropertyName();
    for (const PropertyMeta& property : meta.properties) {
        if (property.name == propertyName) {
            m_valueEdit->setText(valueTextForProperty(widget, property));
            return;
        }
    }
}

void EventActionDialog::updateEditorState()
{
    const QString type = currentActionType();
    const bool needsTarget = type == QLatin1String("load_screen") ||
        type == QLatin1String("set_property") ||
        type == QLatin1String("call_method") ||
        type == QLatin1String("datewrite") ||
        type == QLatin1String("valuechange"); // ✔️ 加这行
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
    else if (type == QLatin1String("datewrite"))
        m_detailStack->setCurrentIndex(5); // 使用属性设置页面
    else if (type == QLatin1String("valuechange"))
        m_detailStack->setCurrentIndex(6); // 使用属性设置页面
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

QString EventActionDialog::currentPropertyName() const
{
    const int idx = m_propertyCombo ? m_propertyCombo->currentIndex() : -1;
    if (idx >= 0 && m_propertyCombo->currentText() == m_propertyCombo->itemText(idx)) {
        const QString propertyName = m_propertyCombo->itemData(idx, kRolePropertyName).toString();
        if (!propertyName.isEmpty())
            return propertyName;
    }
    return m_propertyCombo ? m_propertyCombo->currentText() : QString();
}

WidgetInstance EventActionDialog::currentTargetWidget() const
{
    const int row = m_targetTable ? m_targetTable->currentRow() : -1;
    if (row < 0 || !m_targetTable->item(row, 0)) return {};

    const QString instanceId = m_targetTable->item(row, 0)->data(kRoleId).toString();
    for (const WidgetInstance& widget : m_widgets) {
        if (widget.instanceId == instanceId)
            return widget;
    }
    return {};
}

QString EventActionDialog::valueTextForProperty(const WidgetInstance& widget, const PropertyMeta& property) const
{
    QVariant value;
    if (property.name == QLatin1String("x")) value = widget.x;
    else if (property.name == QLatin1String("y")) value = widget.y;
    else if (property.name == QLatin1String("width")) value = widget.width;
    else if (property.name == QLatin1String("height")) value = widget.height;
    else value = widget.properties.value(property.name, property.defaultValue);

    if (!value.isValid() || value.isNull())
        return QString();
    if (value.typeId() == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return value.toString();
}
