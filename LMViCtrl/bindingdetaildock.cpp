#include "bindingdetaildock.h"

#include "bindinggraphview.h"

#include <algorithm>
#include <functional>
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
#include <QStringList>
#include <QVBoxLayout>

namespace {
QString normalizedValueType(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized == QLatin1String("int")
        || normalized == QLatin1String("integer")
        || normalized == QLatin1String("float")
        || normalized == QLatin1String("double")) {
        return QStringLiteral("number");
    }
    if (normalized == QLatin1String("bool"))
        return QStringLiteral("boolean");
    return normalized;
}

QString typeDisplayName(const QString &type)
{
    const QString normalized = normalizedValueType(type);
    if (normalized.isEmpty() || normalized == QLatin1String("any")) return QObject::tr("任意");
    if (normalized == QLatin1String("event")) return QObject::tr("事件");
    if (normalized == QLatin1String("void")) return QObject::tr("无参数");
    if (normalized == QLatin1String("number")) return QObject::tr("数字");
    if (normalized == QLatin1String("string")) return QObject::tr("文本");
    if (normalized == QLatin1String("boolean")) return QObject::tr("布尔");
    if (normalized == QLatin1String("color")) return QObject::tr("颜色");
    if (normalized == QLatin1String("list")) return QObject::tr("列表");
    return type;
}

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
        connect(m_graphView.data(), &BindingGraphView::selectedNodeChanged,
                this, &BindingDetailDock::setCurrentNode);
        connect(m_graphView.data(), &BindingGraphView::graphChanged,
                this, &BindingDetailDock::refreshPanel);
    }
    refreshPanel();
}

void BindingDetailDock::setCurrentEdge(const QString &edgeId)
{
    if (m_currentEdgeId == edgeId) return;
    m_currentEdgeId = edgeId;
    if (!edgeId.isEmpty())
        m_currentNodeId.clear();
    refreshPanel();
}

void BindingDetailDock::setCurrentNode(const QString &nodeId)
{
    if (m_currentNodeId == nodeId) return;
    m_currentNodeId = nodeId;
    if (!nodeId.isEmpty())
        m_currentEdgeId.clear();
    refreshPanel();
}

void BindingDetailDock::refreshPanel()
{
    if (!m_project || !m_graphView) {
        buildEmptyPanel();
        return;
    }
    if (!m_currentEdgeId.isEmpty()) {
        const BindingEdge edge = m_graphView->edgeSnapshot(m_currentEdgeId);
        if (edge.id.isEmpty()) {
            m_currentEdgeId.clear();
            buildEmptyPanel();
            return;
        }
        buildEdgePanel(edge);
        return;
    }
    if (!m_currentNodeId.isEmpty()) {
        const BindingNode node = m_graphView->nodeSnapshot(m_currentNodeId);
        if (node.id.isEmpty()) {
            m_currentNodeId.clear();
            buildEmptyPanel();
            return;
        }
        buildNodePanel(node);
        return;
    }
    buildEmptyPanel();
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
    m_titleLabel->setText(tr("请选择一个节点或一条绑定连线"));
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

void BindingDetailDock::buildNodePanel(const BindingNode &node)
{
    clearForms();
    m_loading = true;
    m_summaryBox->setEnabled(true);
    m_sequenceBox->setEnabled(true);
    m_editBox->setEnabled(false);
    m_applyButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_clearButton->setEnabled(true);

    const QString name = nodeTitle(node.id);
    m_titleLabel->setText(tr("%1 的动作序列").arg(name));
    m_sourceEdit->setText(name);
    m_typeEdit->setText(tr("节点动作序列"));
    buildNodeActionSequences(node);
    int chainCount = 0;
    for (int i = 0; i < m_sequenceList->count(); ++i) {
        if (!m_sequenceList->item(i)->data(Qt::UserRole).toString().isEmpty())
            ++chainCount;
    }
    m_targetEdit->setText(tr("%1 条逻辑链").arg(chainCount));
    m_loading = false;
}

void BindingDetailDock::buildNodeActionSequences(const BindingNode &node)
{
    m_sequenceList->clear();
    if (!m_project) return;

    auto findGraphNode = [this](const QString &nodeId) -> const BindingNode * {
        if (!m_project) return nullptr;
        for (const BindingNode &candidate : std::as_const(m_project->bindingGraph.nodes)) {
            if (candidate.id == nodeId)
                return &candidate;
        }
        return nullptr;
    };
    auto gridRow = [&](const QString &nodeId) -> int {
        const BindingNode *candidate = findGraphNode(nodeId);
        return candidate ? candidate->visualState.value(QStringLiteral("gridRow"), -1).toInt() : -1;
    };
    auto gridColumn = [&](const QString &nodeId) -> int {
        const BindingNode *candidate = findGraphNode(nodeId);
        return candidate ? candidate->visualState.value(QStringLiteral("gridCol"), -1).toInt() : -1;
    };
    auto eventText = [this](const QString &name) -> QString {
        if (name == QLatin1String("clicked")) return tr("点击");
        if (name == QLatin1String("pressed")) return tr("按下");
        if (name == QLatin1String("released")) return tr("松开");
        if (name == QLatin1String("long_pressed")) return tr("长按");
        return name.isEmpty() ? tr("默认事件") : name;
    };
    auto appendEndpoint = [this](QStringList *parts, const BindingEdge &edge) {
        if (!parts) return;
        const QString action = edge.target.portName.trimmed();
        const QString lowerAction = action.toLower();
        const bool showAction = !action.isEmpty()
            && lowerAction != QLatin1String("value")
            && lowerAction != QLatin1String("input")
            && lowerAction != QLatin1String("output")
            && lowerAction != QLatin1String("data")
            && edge.target.portKind != QLatin1String("event");
        if (showAction && (parts->isEmpty() || parts->last() != action))
            parts->append(action);

        const QString target = nodeTitle(edge.target.nodeId);
        if (!target.isEmpty() && (parts->isEmpty() || parts->last() != target))
            parts->append(target);
    };
    auto sortEdges = [&](QList<BindingEdge> *edges) {
        std::sort(edges->begin(), edges->end(), [&](const BindingEdge &lhs, const BindingEdge &rhs) {
            const int lhsRow = gridRow(lhs.target.nodeId);
            const int rhsRow = gridRow(rhs.target.nodeId);
            if (lhsRow != rhsRow) return lhsRow < rhsRow;
            const int lhsColumn = gridColumn(lhs.target.nodeId);
            const int rhsColumn = gridColumn(rhs.target.nodeId);
            if (lhsColumn != rhsColumn) return lhsColumn < rhsColumn;
            if (lhs.order != rhs.order) return lhs.order < rhs.order;
            return lhs.label < rhs.label;
        });
    };
    auto outgoingEdges = [&](const QString &sourceNodeId, int branchRow, const QStringList &visited) {
        QList<BindingEdge> result;
        for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
            if (edge.source.nodeId != sourceNodeId || edge.target.nodeId.isEmpty())
                continue;
            if (visited.contains(edge.target.nodeId))
                continue;
            if (branchRow >= 0 && gridRow(edge.target.nodeId) != branchRow)
                continue;
            result.append(edge);
        }
        sortEdges(&result);
        return result;
    };

    QList<BindingEdge> firstEdges;
    for (const BindingEdge &edge : std::as_const(m_project->bindingGraph.edges)) {
        if (edge.source.nodeId == node.id && !edge.target.nodeId.isEmpty())
            firstEdges.append(edge);
    }
    sortEdges(&firstEdges);

    if (firstEdges.isEmpty()) {
        auto *item = new QListWidgetItem(tr("暂无从 %1 触发的动作序列").arg(nodeTitle(node.id)), m_sequenceList);
        item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
        return;
    }

    int logicIndex = 1;
    std::function<void(const QString &, int, QStringList, QStringList, const QString &, const QString &, bool)> appendChains;
    appendChains = [&](const QString &currentNodeId,
                       int branchRow,
                       QStringList parts,
                       QStringList visited,
                       const QString &eventName,
                       const QString &firstEdgeId,
                       bool enabled) {
        const QList<BindingEdge> nextEdges = outgoingEdges(currentNodeId, branchRow, visited);
        if (nextEdges.isEmpty()) {
            QString text = tr("%1：逻辑 %2：%3")
                .arg(eventName)
                .arg(logicIndex++)
                .arg(parts.join(QStringLiteral(" -> ")));
            if (!enabled)
                text.append(tr("  [包含禁用绑定]"));
            auto *item = new QListWidgetItem(text, m_sequenceList);
            item->setData(Qt::UserRole, firstEdgeId);
            return;
        }

        for (const BindingEdge &edge : nextEdges) {
            QStringList nextParts = parts;
            QStringList nextVisited = visited;
            appendEndpoint(&nextParts, edge);
            nextVisited.append(edge.target.nodeId);
            appendChains(edge.target.nodeId,
                         branchRow,
                         nextParts,
                         nextVisited,
                         eventName,
                         firstEdgeId,
                         enabled && edge.enabled);
        }
    };

    for (const BindingEdge &edge : std::as_const(firstEdges)) {
        QStringList parts;
        parts.append(nodeTitle(node.id));
        appendEndpoint(&parts, edge);

        QStringList visited;
        visited.append(node.id);
        visited.append(edge.target.nodeId);
        appendChains(edge.target.nodeId,
                     gridRow(edge.target.nodeId),
                     parts,
                     visited,
                     eventText(edge.source.portName),
                     edge.id,
                     edge.enabled);
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
    return QStringLiteral("%1 · %2 [%3]").arg(refName, endpoint.portName, typeDisplayName(endpoint.valueType));
}

QString BindingDetailDock::nodeTitle(const QString &nodeId) const
{
    if (!m_project) return nodeId;
    for (const BindingNode &node : std::as_const(m_project->bindingGraph.nodes)) {
        if (node.id != nodeId)
            continue;
        if (!node.title.isEmpty()) return node.title;
        if (!node.refName.isEmpty()) return node.refName;
        break;
    }
    return nodeId;
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
