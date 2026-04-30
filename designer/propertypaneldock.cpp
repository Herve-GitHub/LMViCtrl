#include "propertypaneldock.h"
#include "canvasscene.h"
#include "canvasitem.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// 给颜色按钮设置展示样式
void styleColorButton(QPushButton *btn, const QColor &c)
{
    const QColor color = c.isValid() ? c : QColor("#000000");
    btn->setText(color.name(QColor::HexRgb).toUpper());
    const bool dark = color.lightness() < 128;
    btn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: %2; border: 1px solid #555; padding: 2px 6px; }")
        .arg(color.name(), dark ? "white" : "black"));
}

QColor toColor(const QVariant &v)
{
    if (v.canConvert<QColor>()) {
        QColor c = v.value<QColor>();
        if (c.isValid()) return c;
    }
    const QString s = v.toString();
    if (!s.isEmpty()) {
        QColor c(s);
        if (c.isValid()) return c;
    }
    return QColor("#000000");
}

} // namespace


PropertyPanelDock::PropertyPanelDock(QWidget *parent)
    : QDockWidget(tr("属性"), parent)
{
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setMinimumWidth(250);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);

    m_root = new QWidget;
    auto *vlay = new QVBoxLayout(m_root);
    vlay->setContentsMargins(6, 6, 6, 6);
    vlay->setSpacing(8);

    m_metaBox  = new QGroupBox(tr("基本信息"), m_root);
    m_metaForm = new QFormLayout(m_metaBox);
    m_metaForm->setLabelAlignment(Qt::AlignRight);

    m_propsBox  = new QGroupBox(tr("属性"), m_root);
    m_propsForm = new QFormLayout(m_propsBox);
    m_propsForm->setLabelAlignment(Qt::AlignRight);

    vlay->addWidget(m_metaBox);
    vlay->addWidget(m_propsBox);
    vlay->addStretch();

    m_scroll->setWidget(m_root);
    setWidget(m_scroll);

    clearPanel();
}

void PropertyPanelDock::setCurrentScene(CanvasScene *scene)
{
    if (m_scene == scene) {
        onSceneSelectionChanged();
        return;
    }
    if (m_scene) {
        disconnect(m_scene, nullptr, this, nullptr);
    }
    m_scene = scene;
    if (m_scene) {
        connect(m_scene.data(), &QGraphicsScene::selectionChanged,
                this, &PropertyPanelDock::onSceneSelectionChanged);
        connect(m_scene.data(), &CanvasScene::instanceChanged,
                this, &PropertyPanelDock::onInstanceChangedFromScene);
    }
    onSceneSelectionChanged();
}

void PropertyPanelDock::onSceneSelectionChanged()
{
    if (!m_scene) { clearPanel(); return; }

    // 仅在单选时显示
    const auto sel = m_scene->selectedItems();
    if (sel.count() != 1) { clearPanel(); return; }

    auto *ci = qgraphicsitem_cast<CanvasItem*>(sel.first());
    if (!ci) { clearPanel(); return; }

    const WidgetInstance inst = ci->instance();
    const WidgetMeta     meta = m_scene->widgetMeta(inst.widgetId);
    m_currentInstanceId = inst.instanceId;
    buildPanel(inst, meta);
}

void PropertyPanelDock::onInstanceChangedFromScene(const QString &instanceId)
{
    if (instanceId != m_currentInstanceId) return;
    // 重新拉取并刷新
    onSceneSelectionChanged();
}

void PropertyPanelDock::clearPanel()
{
    m_currentInstanceId.clear();

    auto clearForm = [](QFormLayout *form) {
        if (!form) return;
        while (form->count() > 0) {
            QLayoutItem *item = form->takeAt(0);
            if (!item) break;
            if (QWidget *w = item->widget()) w->deleteLater();
            delete item;
        }
    };
    clearForm(m_metaForm);
    clearForm(m_propsForm);

    auto *placeholder = new QLabel(tr("（请选中单个控件）"), m_propsBox);
    placeholder->setStyleSheet("color: #888;");
    m_propsForm->addRow(placeholder);
}

void PropertyPanelDock::buildPanel(const WidgetInstance &inst, const WidgetMeta &meta)
{
    // 清空旧控件
    auto clearForm = [](QFormLayout *form) {
        while (form->count() > 0) {
            QLayoutItem *item = form->takeAt(0);
            if (!item) break;
            if (QWidget *w = item->widget()) w->deleteLater();
            delete item;
        }
    };
    clearForm(m_metaForm);
    clearForm(m_propsForm);

    // ===== 第一部分：不可编辑 =====
    auto addReadOnly = [this](const QString &label, const QString &val) {
        auto *le = new QLineEdit(val);
        le->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 设置从左向右显示
        le->setReadOnly(true);
        le->setStyleSheet("QLineEdit { background:#2a2a2a; color:#aaa; }");
        m_metaForm->addRow(new QLabel(label), le);
    };
    addReadOnly(tr("id"),             meta.id);
    addReadOnly(tr("description"),    meta.description);
    addReadOnly(tr("schema_version"), meta.schemaVersion);
    addReadOnly(tr("version"),        meta.version);

    // ===== 第二部分：name + properties =====
    // -- name (可编辑) --
    {
        auto *nameEdit = new QLineEdit(inst.name);
        nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);  // 设置从左向右显示
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit]() {
            if (!m_scene || m_currentInstanceId.isEmpty()) return;
            m_scene->setInstanceName(m_currentInstanceId, nameEdit->text());
        });
        m_propsForm->addRow(new QLabel(tr("name")), nameEdit);
    }

    // -- properties --
    for (const PropertyMeta &pm : meta.properties) {
        const QVariant value = inst.properties.value(pm.name, pm.defaultValue);
        QWidget *editor = makeEditor(pm, value);
        if (!editor) continue;
        const QString label = pm.label.isEmpty() ? pm.name : pm.label;
        m_propsForm->addRow(new QLabel(label), editor);
    }

    if (meta.properties.isEmpty()) {
        auto *lbl = new QLabel(tr("（该组件没有可编辑属性）"));
        lbl->setStyleSheet("color: #888;");
        m_propsForm->addRow(lbl);
    }
}

QWidget *PropertyPanelDock::makeEditor(const PropertyMeta &pm, const QVariant &value)
{
    const QString key = pm.name;
    auto pushValue = [this, key](const QVariant &v) {
        if (!m_scene || m_currentInstanceId.isEmpty()) return;
        m_scene->setInstanceProperty(m_currentInstanceId, key, v);
    };

    const QString type = pm.type.toLower();

    // boolean
    if (type == QLatin1String("boolean") || type == QLatin1String("bool")) {
        auto *cb = new QCheckBox;
        cb->setChecked(value.toBool());
        connect(cb, &QCheckBox::toggled, this, [pushValue](bool v){ pushValue(v); });
        return cb;
    }

    // number
    if (type == QLatin1String("number") || type == QLatin1String("int") ||
        type == QLatin1String("integer") || type == QLatin1String("float") ||
        type == QLatin1String("double")) {
        const bool isInt = (type == QLatin1String("int") || type == QLatin1String("integer"));
        if (isInt) {
            auto *sb = new QSpinBox;
            sb->setRange(int(pm.min), int(pm.max));
            sb->setValue(value.toInt());
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [pushValue](int v){ pushValue(v); });
            return sb;
        }
        auto *sb = new QDoubleSpinBox;
        sb->setRange(pm.min, pm.max);
        sb->setDecimals(3);
        sb->setValue(value.toDouble());
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [pushValue](double v){ pushValue(v); });
        return sb;
    }

    // color
    if (type == QLatin1String("color")) {
        auto *btn = new QPushButton;
        QColor c = toColor(value);
        styleColorButton(btn, c);
        connect(btn, &QPushButton::clicked, this, [btn, pushValue]() {
            const QColor cur(btn->text());
            const QColor picked = QColorDialog::getColor(
                cur.isValid() ? cur : QColor("#000000"),
                btn, tr("选择颜色"), QColorDialog::ShowAlphaChannel);
            if (!picked.isValid()) return;
            styleColorButton(btn, picked);
            pushValue(picked.name(QColor::HexRgb));
        });
        return btn;
    }

    // enum
    if (type == QLatin1String("enum")) {
        auto *cb = new QComboBox;
        int curIdx = -1;
        const QString cur = value.toString();
        for (int i = 0; i < pm.options.size(); ++i) {
            const auto &opt = pm.options[i];
            const QString shown = opt.label.isEmpty() ? opt.value : opt.label;
            cb->addItem(shown, opt.value);
            if (opt.value == cur) curIdx = i;
        }
        if (curIdx >= 0) cb->setCurrentIndex(curIdx);
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [cb, pushValue](int){ pushValue(cb->currentData().toString()); });
        return cb;
    }

    // code / multiline
    if (type == QLatin1String("code") || pm.multiline) {
        auto *te = new QPlainTextEdit;
        te->setPlainText(value.toString());
        const int lines = qMax(2, pm.lines > 0 ? pm.lines : 4);
        te->setFixedHeight(lines * 18 + 8);
        connect(te, &QPlainTextEdit::textChanged, this, [te, pushValue]() {
            pushValue(te->toPlainText());
        });
        return te;
    }

    // string / resource / 默认
    auto *le = new QLineEdit(value.toString());
    connect(le, &QLineEdit::editingFinished, this, [le, pushValue]() {
        pushValue(le->text());
    });
    return le;
}
