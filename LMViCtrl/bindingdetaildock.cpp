#include "bindingdetaildock.h"

#include "bindinggraphview.h"

#include <algorithm>
#include <functional>
#include <QCheckBox>
#include <QComboBox>
#include <QAbstractItemView>
#include <QDoubleSpinBox>
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
#include <QVariant>
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

bool valueToBool(const QVariant &value)
{
    if (value.typeId() == QMetaType::Bool)
        return value.toBool();
    const QString text = value.toString().trimmed().toLower();
    return text == QLatin1String("true")
        || text == QLatin1String("1")
        || text == QLatin1String("yes")
        || text == QLatin1String("on");
}

QString valueToEditorText(const QVariant &value)
{
    if (!value.isValid() || value.isNull()) return QString();
    if (value.typeId() == QMetaType::Bool)
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return value.toString();
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
    m_sequenceList->setMaximumHeight(82);
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
    m_valueSourceCombo = new QComboBox(m_editBox);
    m_valueSourceCombo->addItem(tr("默认事件值"), QStringLiteral("event_default"));
    m_valueSourceCombo->addItem(tr("固定值"), QStringLiteral("literal"));
    m_valueSourceCombo->addItem(tr("事件参数"), QStringLiteral("event"));
    m_valueSourceCombo->addItem(tr("数据变量"), QStringLiteral("variable"));
    m_valueSourceCombo->addItem(tr("表达式"), QStringLiteral("expression"));
    m_valueTextEdit = new QLineEdit(m_editBox);
    m_valueTextEdit->setPlaceholderText(tr("固定值"));
    m_valueNumberSpin = new QDoubleSpinBox(m_editBox);
    m_valueNumberSpin->setRange(-999999999.0, 999999999.0);
    m_valueNumberSpin->setDecimals(6);
    m_valueBoolCombo = new QComboBox(m_editBox);
    m_valueBoolCombo->addItem(tr("true"), true);
    m_valueBoolCombo->addItem(tr("false"), false);
    m_eventParamCombo = new QComboBox(m_editBox);
    m_eventParamCombo->setEditable(true);
    m_eventParamCombo->setInsertPolicy(QComboBox::NoInsert);
    m_variableCombo = new QComboBox(m_editBox);
    m_variableCombo->setEditable(true);
    m_variableCombo->setInsertPolicy(QComboBox::NoInsert);
    m_expressionEdit = new QLineEdit(m_editBox);
    m_expressionEdit->setPlaceholderText(tr("Lua 表达式，例如 e.value * 10"));
    m_indexEdit = new QLineEdit(m_editBox);
    m_indexEdit->setPlaceholderText(tr("列表索引，仅 set_item 使用"));
    m_patternEdit = new QLineEdit(m_editBox);
    m_patternEdit->setPlaceholderText(tr("格式化模板，例如 %s ℃"));
    m_debugPrintCheck = new QCheckBox(tr("执行前打印本次设置值"), m_editBox);
    m_debugLabelEdit = new QLineEdit(m_editBox);
    m_debugLabelEdit->setPlaceholderText(tr("可选，例如 Label_1 文本"));
    m_paramsEdit = new QPlainTextEdit(m_editBox);
    m_paramsEdit->setMinimumHeight(54);
    m_paramsEdit->setMaximumHeight(68);
    m_paramsEdit->setPlaceholderText(QStringLiteral("{}"));

    m_editForm->addRow(tr("执行模式"), m_modeCombo);
    m_editForm->addRow(tr("状态"), m_enabledCheck);
    m_editForm->addRow(tr("顺序"), m_orderSpin);
    m_editForm->addRow(tr("延迟"), m_delaySpin);
    m_editForm->addRow(tr("条件"), m_conditionEdit);
    m_editForm->addRow(tr("值来源"), m_valueSourceCombo);
    m_editForm->addRow(tr("固定值"), m_valueTextEdit);
    m_editForm->addRow(tr("固定值"), m_valueNumberSpin);
    m_editForm->addRow(tr("固定值"), m_valueBoolCombo);
    m_editForm->addRow(tr("事件参数"), m_eventParamCombo);
    m_editForm->addRow(tr("数据变量"), m_variableCombo);
    m_editForm->addRow(tr("表达式"), m_expressionEdit);
    m_editForm->addRow(tr("索引"), m_indexEdit);
    m_editForm->addRow(tr("格式"), m_patternEdit);
    m_editForm->addRow(tr("打印"), m_debugPrintCheck);
    m_editForm->addRow(tr("打印标签"), m_debugLabelEdit);
    m_editForm->addRow(tr("高级 JSON"), m_paramsEdit);

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

    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        applyPendingChanges();
    });
    connect(m_valueSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BindingDetailDock::updateValueEditorState);
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
    m_valueSourceCombo->setCurrentIndex(0);
    m_valueTextEdit->clear();
    m_valueNumberSpin->setValue(0.0);
    m_valueBoolCombo->setCurrentIndex(0);
    m_eventParamCombo->clear();
    m_variableCombo->clear();
    m_expressionEdit->clear();
    m_indexEdit->clear();
    m_patternEdit->clear();
    m_debugPrintCheck->setChecked(false);
    m_debugLabelEdit->clear();
    m_paramsEdit->clear();
    updateValueEditorState();
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

    m_eventParamCombo->clear();
    const QStringList eventParams = eventParamNamesForEdge(edge);
    for (const QString &name : eventParams)
        m_eventParamCombo->addItem(name, name);
    if (m_eventParamCombo->count() == 0) {
        m_eventParamCombo->addItem(QStringLiteral("value"), QStringLiteral("value"));
        m_eventParamCombo->addItem(QStringLiteral("text"), QStringLiteral("text"));
        m_eventParamCombo->addItem(QStringLiteral("checked"), QStringLiteral("checked"));
    }

    m_variableCombo->clear();
    if (m_project) {
        for (const DataVariable &variable : std::as_const(m_project->dataVariables)) {
            const QString shown = variable.name.isEmpty() ? variable.id : variable.name;
            m_variableCombo->addItem(shown, shown);
        }
    }

    const QVariantMap params = edge.params;
    QString valueSource = params.value(QStringLiteral("valueSource")).toString();
    if (valueSource.isEmpty())
        valueSource = params.contains(QStringLiteral("value")) ? QStringLiteral("literal") : QStringLiteral("event_default");
    int sourceIndex = m_valueSourceCombo->findData(valueSource);
    if (sourceIndex < 0)
        sourceIndex = 0;
    m_valueSourceCombo->setCurrentIndex(sourceIndex);
    setEditorLiteralValue(params.value(QStringLiteral("value")), edgeValueType(edge));
    const QString eventParam = params.value(QStringLiteral("eventParam"), QStringLiteral("value")).toString();
    int eventParamIndex = m_eventParamCombo->findText(eventParam);
    if (eventParamIndex >= 0)
        m_eventParamCombo->setCurrentIndex(eventParamIndex);
    else
        m_eventParamCombo->setEditText(eventParam);
    const QString variable = params.value(QStringLiteral("variable")).toString();
    int variableIndex = m_variableCombo->findText(variable);
    if (variableIndex >= 0)
        m_variableCombo->setCurrentIndex(variableIndex);
    else
        m_variableCombo->setEditText(variable);
    m_expressionEdit->setText(params.value(QStringLiteral("expression")).toString());
    m_indexEdit->setText(params.value(QStringLiteral("index")).toString());
    m_patternEdit->setText(params.value(QStringLiteral("pattern")).toString());
    m_debugPrintCheck->setChecked(params.value(QStringLiteral("debugPrint")).toBool());
    m_debugLabelEdit->setText(params.value(QStringLiteral("debugLabel")).toString());
    m_paramsEdit->setPlainText(paramsToJson(edge.params));
    updateValueEditorState();
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

void BindingDetailDock::updateValueEditorState()
{
    if (!m_valueSourceCombo) return;
    const QString source = m_valueSourceCombo->currentData().toString();
    BindingEdge edge;
    if (m_graphView && !m_currentEdgeId.isEmpty())
        edge = m_graphView->edgeSnapshot(m_currentEdgeId);
    const QString valueType = edgeValueType(edge);
    const QString normalized = normalizedValueType(valueType);
    const bool literal = source == QLatin1String("literal");

    setFormFieldVisible(m_valueTextEdit, literal && normalized != QLatin1String("number") && normalized != QLatin1String("boolean"));
    setFormFieldVisible(m_valueNumberSpin, literal && normalized == QLatin1String("number"));
    setFormFieldVisible(m_valueBoolCombo, literal && normalized == QLatin1String("boolean"));
    setFormFieldVisible(m_eventParamCombo, source == QLatin1String("event"));
    setFormFieldVisible(m_variableCombo, source == QLatin1String("variable"));
    setFormFieldVisible(m_expressionEdit, source == QLatin1String("expression"));
    setFormFieldVisible(m_indexEdit, edge.target.portName == QLatin1String("set_item"));
    setFormFieldVisible(m_patternEdit, edge.target.portName == QLatin1String("format"));
}

QString BindingDetailDock::edgeValueType(const BindingEdge &edge) const
{
    if (!edge.target.valueType.isEmpty()) return edge.target.valueType;
    if (!edge.source.valueType.isEmpty()) return edge.source.valueType;
    return QStringLiteral("any");
}

QStringList BindingDetailDock::eventParamNamesForEdge(const BindingEdge &edge) const
{
    QStringList names;
    if (edge.source.portKind == QLatin1String("data_get")) {
        names << QStringLiteral("value") << QStringLiteral("oldValue");
        return names;
    }

    if (edge.source.portKind != QLatin1String("event"))
        return names;

    QString widgetId;
    if (m_project) {
        for (const ScreenData &screen : std::as_const(m_project->screens)) {
            for (const WidgetInstance &instance : screen.widgets) {
                if (instance.instanceId == edge.source.refId) {
                    widgetId = instance.widgetId;
                    break;
                }
            }
            if (!widgetId.isEmpty()) break;
        }
    }

    for (const WidgetMeta &meta : std::as_const(m_metas)) {
        if (meta.id != widgetId && meta.type != widgetId)
            continue;
        for (const EventDef &event : meta.eventDefs) {
            if (event.name != edge.source.portName)
                continue;
            for (const EventParamDef &param : event.params) {
                if (!param.name.isEmpty() && !names.contains(param.name))
                    names.append(param.name);
            }
            return names;
        }
    }
    return names;
}

QVariant BindingDetailDock::editorLiteralValue(const QString &valueType) const
{
    const QString normalized = normalizedValueType(valueType);
    if (normalized == QLatin1String("number"))
        return m_valueNumberSpin->value();
    if (normalized == QLatin1String("boolean"))
        return m_valueBoolCombo->currentData().toBool();
    return m_valueTextEdit->text();
}

void BindingDetailDock::setEditorLiteralValue(const QVariant &value, const QString &valueType)
{
    const QString normalized = normalizedValueType(valueType);
    if (normalized == QLatin1String("number")) {
        bool ok = false;
        const double number = value.toDouble(&ok);
        m_valueNumberSpin->setValue(ok ? number : 0.0);
        m_valueTextEdit->setText(valueToEditorText(value));
        return;
    }
    if (normalized == QLatin1String("boolean")) {
        m_valueBoolCombo->setCurrentIndex(valueToBool(value) ? 0 : 1);
        m_valueTextEdit->setText(valueToEditorText(value));
        return;
    }
    m_valueTextEdit->setText(valueToEditorText(value));
}

void BindingDetailDock::setFormFieldVisible(QWidget *field, bool visible)
{
    if (!field || !m_editForm) return;
    field->setVisible(visible);
    if (QWidget *label = m_editForm->labelForField(field))
        label->setVisible(visible);
}

QVariantMap BindingDetailDock::paramsFromEditors(const BindingEdge &edge, bool *ok) const
{
    if (ok) *ok = false;
    QVariantMap params;
    if (!paramsFromJson(m_paramsEdit->toPlainText(), &params))
        return params;

    params.remove(QStringLiteral("valueSource"));
    params.remove(QStringLiteral("value"));
    params.remove(QStringLiteral("eventParam"));
    params.remove(QStringLiteral("variable"));
    params.remove(QStringLiteral("expression"));
    params.remove(QStringLiteral("index"));
    params.remove(QStringLiteral("pattern"));
    params.remove(QStringLiteral("debugPrint"));
    params.remove(QStringLiteral("debugLabel"));

    const QString source = m_valueSourceCombo->currentData().toString();
    params.insert(QStringLiteral("valueSource"), source.isEmpty() ? QStringLiteral("event_default") : source);
    if (source == QLatin1String("literal")) {
        params.insert(QStringLiteral("value"), editorLiteralValue(edgeValueType(edge)));
    } else if (source == QLatin1String("event")) {
        const QString paramName = m_eventParamCombo->currentText().trimmed();
        if (!paramName.isEmpty())
            params.insert(QStringLiteral("eventParam"), paramName);
    } else if (source == QLatin1String("variable")) {
        const QString variable = m_variableCombo->currentText().trimmed();
        if (!variable.isEmpty())
            params.insert(QStringLiteral("variable"), variable);
    } else if (source == QLatin1String("expression")) {
        const QString expression = m_expressionEdit->text().trimmed();
        if (!expression.isEmpty())
            params.insert(QStringLiteral("expression"), expression);
    }

    const QString index = m_indexEdit->text().trimmed();
    if (!index.isEmpty())
        params.insert(QStringLiteral("index"), index);
    const QString pattern = m_patternEdit->text();
    if (!pattern.isEmpty())
        params.insert(QStringLiteral("pattern"), pattern);
    if (m_debugPrintCheck->isChecked())
        params.insert(QStringLiteral("debugPrint"), true);
    const QString debugLabel = m_debugLabelEdit->text().trimmed();
    if (!debugLabel.isEmpty())
        params.insert(QStringLiteral("debugLabel"), debugLabel);

    if (ok) *ok = true;
    return params;
}

bool BindingDetailDock::applyPendingChanges()
{
    if (m_loading || !m_graphView || m_currentEdgeId.isEmpty()) return true;
    BindingEdge edge = m_graphView->edgeSnapshot(m_currentEdgeId);
    if (edge.id.isEmpty()) return true;

    bool paramsOk = false;
    QVariantMap params = paramsFromEditors(edge, &paramsOk);
    if (!paramsOk) {
        QMessageBox::warning(this, tr("参数格式错误"), tr("高级 JSON 必须是对象，例如 {\"value\": 1}"));
        return false;
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
    return true;
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
