#include "bindingdetaildock.h"

#include "bindinggraphview.h"

#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
QString edgeTypeLabel(const QString &type)
{
    if (type == QLatin1String("event_data")) return QObject::tr("控件事件 -> 数据写入");
    if (type == QLatin1String("data_action")) return QObject::tr("数据事件 -> 控件动作");
    if (type == QLatin1String("property_binding")) return QObject::tr("属性绑定");
    return QObject::tr("控件事件 -> 控件动作");
}

bool sameSource(const BindingEdge &lhs, const BindingEdge &rhs)
{
    return lhs.source.nodeId == rhs.source.nodeId
        && lhs.source.portKind == rhs.source.portKind
        && lhs.source.portName == rhs.source.portName;
}
}

BindingDetailDock::BindingDetailDock(QWidget *parent)
    : QDockWidget(tr("绑定详情"), parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(300);
    buildUi();
    buildEmptyPanel();
}

void BindingDetailDock::setProjectData(ProjectData *project)
{
    m_project = project;
    refreshPanel();
}

void BindingDetailDock::setWidgetMetas(const QList<WidgetMeta> &metas)
{
    m_metas = metas;
    refreshPanel();
}

void BindingDetailDock::setGraphView(BindingGraphView *view)
{
    if (m_graphView == view) return;
    if (m_graphView)
        disconnect(m_graphView, nullptr, this, nullptr);
    m_graphView = view;
    if (m_graphView) {
        connect(m_graphView.data(), &BindingGraphView::selectedEdgeChanged,
                this, &BindingDetailDock::setCurrentEdge);
        connect(m_graphView.data(), &BindingGraphView::graphChanged,
                this, &BindingDetailDock::refreshPanel);
    }
    refreshPanel();
}

void BindingDetailDock::setCurrentEdge(const QString &edgeId)
{
    if (m_currentEdgeId == edgeId) return;
    m_currentEdgeId = edgeId;
    refreshPanel();
}

void BindingDetailDock::refreshPanel()
{
    if (!m_project || !m_graphView || m_currentEdgeId.isEmpty()) {
        buildEmptyPanel();
        return;
    }
    const BindingEdge edge = m_graphView->edgeSnapshot(m_currentEdgeId);
    if (edge.id.isEmpty()) {
        m_currentEdgeId.clear();
        buildEmptyPanel();
        return;
    }
    buildEdgePanel(edge);
}

void BindingDetailDock::buildUi()
{
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_root = new QWidget;
    auto *layout = new QVBoxLayout(m_root);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_titleLabel = new QLabel(m_root);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet(QStringLiteral("font-weight:bold; color:#f0f0f0;"));

    m_summaryBox = new QGroupBox(tr("触发事件"), m_root);
    m_summaryForm = new QFormLayout(m_summaryBox);
    m_summaryForm->setLabelAlignment(Qt::AlignRight);

    m_sequenceBox = new QGroupBox(tr("动作序列"), m_root);
    auto *sequenceLayout = new QVBoxLayout(m_sequenceBox);
    m_sequenceList = new QListWidget(m_sequenceBox);
    m_sequenceList->setSelectionMode(QAbstractItemView::SingleSelection);
    sequenceLayout->addWidget(m_sequenceList);

    m_editBox = new QGroupBox(tr("参数 / 条件 / 延迟"), m_root);
    m_editForm = new QFormLayout(m_editBox);
    m_editForm->setLabelAlignment(Qt::AlignRight);

    m_sourceEdit = new QLineEdit(m_summaryBox);
    m_targetEdit = new QLineEdit(m_summaryBox);
    m_typeEdit = new QLineEdit(m_summaryBox);
    for (QLineEdit *edit : { m_sourceEdit, m_targetEdit, m_typeEdit }) {
        edit->setReadOnly(true);
        edit->setStyleSheet(QStringLiteral("QLineEdit { background:#2a2a2a; color:#bdbdbd; }"));
    }
    m_summaryForm->addRow(tr("事件"), m_sourceEdit);
    m_summaryForm->addRow(tr("目标"), m_targetEdit);
    m_summaryForm->addRow(tr("类型"), m_typeEdit);

    m_modeCombo = new QComboBox(m_editBox);
    m_modeCombo->addItem(tr("顺序执行"), QStringLiteral("sequence"));
    m_modeCombo->addItem(tr("并行执行"), QStringLiteral("parallel"));
    m_enabledCheck = new QCheckBox(tr("启用此绑定"), m_editBox);
    m_orderSpin = new QSpinBox(m_editBox);
    m_orderSpin->setRange(0, 999);
    m_delaySpin = new QSpinBox(m_editBox);
    m_delaySpin->setRange(0, 3600000);
    m_delaySpin->setSuffix(tr(" ms"));
    m_conditionEdit = new QLineEdit(m_editBox);
    m_conditionEdit->setPlaceholderText(tr("为空表示总是执行"));
    m_paramsEdit = new QPlainTextEdit(m_editBox);
    m_paramsEdit->setMinimumHeight(92);
    m_paramsEdit->setPlaceholderText(QStringLiteral("{}"));

    m_editForm->addRow(tr("执行模式"), m_modeCombo);
    m_editForm->addRow(tr("状态"), m_enabledCheck);
    m_editForm->addRow(tr("顺序"), m_orderSpin);
    m_editForm->addRow(tr("延迟"), m_delaySpin);
    m_editForm->addRow(tr("条件"), m_conditionEdit);
    m_editForm->addRow(tr("参数 JSON"), m_paramsEdit);

    auto *buttonRow = new QWidget(m_root);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_applyButton = new QPushButton(tr("应用修改"), buttonRow);
    m_deleteButton = new QPushButton(tr("删除绑定"), buttonRow);
    m_clearButton = new QPushButton(tr("关闭"), buttonRow);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addWidget(m_clearButton);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_summaryBox);
    layout->addWidget(m_sequenceBox);
    layout->addWidget(m_editBox);
    layout->addWidget(buttonRow);
    layout->addStretch();

    m_scroll->setWidget(m_root);
    setWidget(m_scroll);

    connect(m_applyButton, &QPushButton::clicked, this, &BindingDetailDock::applyChanges);
    connect(m_deleteButton, &QPushButton::clicked, this, [this]() {
        if (!m_graphView || m_currentEdgeId.isEmpty()) return;
        m_graphView->deleteEdge(m_currentEdgeId);
    });
    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        if (m_graphView)
            m_graphView->selectEdge(QString());
        else
            setCurrentEdge(QString());
    });
    connect(m_sequenceList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item || !m_graphView) return;
        const QString edgeId = item->data(Qt::UserRole).toString();
        if (!edgeId.isEmpty())
            m_graphView->selectEdge(edgeId);
    });
}

void BindingDetailDock::clearForms()
{
    m_loading = true;
    m_titleLabel->clear();
    m_sourceEdit->clear();
    m_targetEdit->clear();
    m_typeEdit->clear();
    m_sequenceList->clear();
    m_modeCombo->setCurrentIndex(0);
    m_enabledCheck->setChecked(false);
    m_orderSpin->setValue(0);
    m_delaySpin->setValue(0);
    m_conditionEdit->clear();
    m_paramsEdit->clear();
    m_loading = false;
}

void BindingDetailDock::buildEmptyPanel()
{
    clearForms();
    m_titleLabel->setText(tr("请选择一条绑定连线"));
    m_summaryBox->setEnabled(false);
    m_sequenceBox->setEnabled(false);
    m_editBox->setEnabled(false);
    m_applyButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_clearButton->setEnabled(false);
}

void BindingDetailDock::buildEdgePanel(const BindingEdge &edge)
{
    m_loading = true;
    m_summaryBox->setEnabled(true);
    m_sequenceBox->setEnabled(true);
    m_editBox->setEnabled(true);
    m_applyButton->setEnabled(true);
    m_deleteButton->setEnabled(true);
    m_clearButton->setEnabled(true);

    m_titleLabel->setText(edgeTitle(edge));
    m_sourceEdit->setText(endpointTitle(edge.source));
    m_targetEdit->setText(endpointTitle(edge.target));
    m_typeEdit->setText(edgeTypeLabel(edge.type));
    m_modeCombo->setCurrentIndex(qMax(0, m_modeCombo->findData(edge.executionMode)));
    m_enabledCheck->setChecked(edge.enabled);
    m_orderSpin->setValue(edge.order);
    m_delaySpin->setValue(edge.delayMs);
    m_conditionEdit->setText(edge.condition);
    m_paramsEdit->setPlainText(paramsToJson(edge.params));
    buildSequenceList(edge);
    m_loading = false;
}

void BindingDetailDock::buildSequenceList(const BindingEdge &edge)
{
    m_sequenceList->clear();
    if (!m_project) return;

    QList<BindingEdge> sequence;
    for (const BindingEdge &candidate : std::as_const(m_project->bindingGraph.edges)) {
        if (sameSource(candidate, edge))
            sequence.append(candidate);
    }
    std::sort(sequence.begin(), sequence.end(), [](const BindingEdge &lhs, const BindingEdge &rhs) {
        if (lhs.order == rhs.order)
            return lhs.label < rhs.label;
        return lhs.order < rhs.order;
    });

    for (const BindingEdge &candidate : std::as_const(sequence)) {
        const QString text = QStringLiteral("%1. %2  [%3]")
            .arg(candidate.order + 1)
            .arg(endpointTitle(candidate.target))
            .arg(candidate.enabled ? tr("启用") : tr("禁用"));
        auto *item = new QListWidgetItem(text, m_sequenceList);
        item->setData(Qt::UserRole, candidate.id);
        if (candidate.id == edge.id)
            item->setSelected(true);
    }
}

void BindingDetailDock::applyChanges()
{
    if (m_loading || !m_graphView || m_currentEdgeId.isEmpty()) return;
    BindingEdge edge = m_graphView->edgeSnapshot(m_currentEdgeId);
    if (edge.id.isEmpty()) return;

    QVariantMap params;
    if (!paramsFromJson(m_paramsEdit->toPlainText(), &params)) {
        QMessageBox::warning(this, tr("参数格式错误"), tr("参数 JSON 必须是对象，例如 {\"value\": 1}"));
        return;
    }

    edge.executionMode = m_modeCombo->currentData().toString();
    if (edge.executionMode.isEmpty())
        edge.executionMode = QStringLiteral("sequence");
    edge.enabled = m_enabledCheck->isChecked();
    edge.order = m_orderSpin->value();
    edge.delayMs = m_delaySpin->value();
    edge.condition = m_conditionEdit->text().trimmed();
    edge.params = params;

    m_graphView->updateEdge(edge);
}

QString BindingDetailDock::endpointTitle(const BindingEndpoint &endpoint) const
{
    const QString refName = endpoint.refName.isEmpty() ? endpoint.nodeId : endpoint.refName;
    return QStringLiteral("%1 · %2").arg(refName, endpoint.portName);
}

QString BindingDetailDock::edgeTitle(const BindingEdge &edge) const
{
    if (!edge.label.isEmpty())
        return edge.label;
    return QStringLiteral("%1 -> %2").arg(endpointTitle(edge.source), endpointTitle(edge.target));
}

QString BindingDetailDock::paramsToJson(const QVariantMap &params) const
{
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(params)).toJson(QJsonDocument::Indented));
}

bool BindingDetailDock::paramsFromJson(const QString &text, QVariantMap *params) const
{
    if (!params) return false;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        params->clear();
        return true;
    }
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    *params = doc.object().toVariantMap();
    return true;
}
