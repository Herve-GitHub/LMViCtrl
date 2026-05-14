#include "propertypaneldock.h"
#include "canvasscene.h"
#include "canvasitem.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHash>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSet>
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
        connect(m_scene.data(), &CanvasScene::instanceGeometryChanged,
                this, &PropertyPanelDock::onInstanceGeometryChangedFromScene);
        connect(m_scene.data(), &CanvasScene::canvasChanged,
            this, &PropertyPanelDock::onCanvasChangedFromScene);
    }
    onSceneSelectionChanged();
}

void PropertyPanelDock::onSceneSelectionChanged()
{
    if (!m_scene) { clearPanel(); return; }

    // 仅在单选时显示
    const auto sel = m_scene->selectedItems();
    if (sel.isEmpty()) { buildCanvasPanel(); return; }

    CanvasItem *target = nullptr;
    if (sel.count() == 1) {
        target = qgraphicsitem_cast<CanvasItem*>(sel.first());
    } else {
        QList<CanvasItem *> groupItems;
        QList<CanvasItem *> canvasItems;
        for (QGraphicsItem *gi : sel) {
            if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
                canvasItems.append(ci);
                if (ci->instance().isGroup)
                    groupItems.append(ci);
            }
        }
        if (groupItems.count() == 1) {
            const QString groupId = groupItems.first()->instance().instanceId;
            QHash<QString, QString> parentById;
            for (const WidgetInstance &inst : m_scene->allInstances())
                parentById.insert(inst.instanceId, inst.parentId);

            bool allInGroup = true;
            for (CanvasItem *ci : std::as_const(canvasItems)) {
                const QString id = ci->instance().instanceId;
                if (id == groupId) continue;
                QString parentId = ci->instance().parentId;
                bool found = false;
                QSet<QString> seen;
                while (!parentId.isEmpty() && !seen.contains(parentId)) {
                    if (parentId == groupId) { found = true; break; }
                    seen.insert(parentId);
                    parentId = parentById.value(parentId);
                }
                if (!found) { allInGroup = false; break; }
            }
            if (allInGroup)
                target = groupItems.first();
        }
    }

    if (!target) { clearPanel(); return; }

    const WidgetInstance inst = target->instance();
    const WidgetMeta     meta = m_scene->widgetMeta(inst.widgetId);
    m_currentInstanceId = inst.instanceId;
    buildPanel(inst, meta);
}

void PropertyPanelDock::onInstanceChangedFromScene(const QString &instanceId)
{
    if (instanceId != m_currentInstanceId) return;
    // 当变化来源于本面板自身的编辑器输入时，跳过重建以避免焦点丢失
    if (m_pushingValue) return;
    // 重新拉取并刷新
    onSceneSelectionChanged();
}

void PropertyPanelDock::onInstanceGeometryChangedFromScene(const QString &instanceId)
{
    if (instanceId != m_currentInstanceId || !m_scene) return;
    WidgetInstance inst;
    if (!m_scene->instance(instanceId, &inst)) return;
    refreshGeometryEditors(inst);
}

void PropertyPanelDock::onCanvasChangedFromScene()
{
    if (!m_scene || m_pushingValue || !m_scene->selectedItems().isEmpty()) return;
    buildCanvasPanel();
}

void PropertyPanelDock::refreshGeometryEditors(const WidgetInstance &inst)
{
    auto setIfSpin = [](QWidget *w, int v) {
        if (!w) return;
        if (auto *sb = qobject_cast<QSpinBox*>(w)) {
            QSignalBlocker b(sb);
            sb->setValue(v);
        } else if (auto *db = qobject_cast<QDoubleSpinBox*>(w)) {
            QSignalBlocker b(db);
            db->setValue(v);
        }
    };
    setIfSpin(m_geometryEditors.value(QStringLiteral("x")).data(),      inst.x);
    setIfSpin(m_geometryEditors.value(QStringLiteral("y")).data(),      inst.y);
    setIfSpin(m_geometryEditors.value(QStringLiteral("width")).data(),  inst.width);
    setIfSpin(m_geometryEditors.value(QStringLiteral("height")).data(), inst.height);
}

void PropertyPanelDock::clearForms()
{
    m_geometryEditors.clear();

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
}

void PropertyPanelDock::clearPanel()
{
    m_currentInstanceId.clear();
    clearForms();

    auto *placeholder = new QLabel(tr("（请选择单个控件，或点击空白处编辑画布）"), m_propsBox);
    placeholder->setStyleSheet("color: #888;");
    m_propsForm->addRow(placeholder);
}

void PropertyPanelDock::buildCanvasPanel()
{
    if (!m_scene) { clearPanel(); return; }

    clearForms();

    auto addReadOnly = [](QFormLayout *form, const QString &label, const QString &val) {
        auto *le = new QLineEdit(val);
        le->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        le->setReadOnly(true);
        le->setStyleSheet("QLineEdit { background:#2a2a2a; color:#aaa; }");
        form->addRow(new QLabel(label), le);
    };

    addReadOnly(m_metaForm, tr("对象"), tr("画布"));

    const QSize size = m_scene->canvasSize();
    addReadOnly(m_propsForm, tr("width"), QString::number(size.width()));
    addReadOnly(m_propsForm, tr("height"), QString::number(size.height()));

    auto *bgBtn = new QPushButton;
    styleColorButton(bgBtn, m_scene->canvasBackgroundColor());
    connect(bgBtn, &QPushButton::clicked, this, [this, bgBtn]() {
        if (!m_scene) return;
        const QColor cur(bgBtn->text());
        const QColor picked = QColorDialog::getColor(
            cur.isValid() ? cur : m_scene->canvasBackgroundColor(),
            bgBtn, tr("选择画布背景色"), QColorDialog::ShowAlphaChannel);
        if (!picked.isValid()) return;
        styleColorButton(bgBtn, picked);
        m_pushingValue = true;
        m_scene->setCanvasBackgroundColor(picked);
        m_pushingValue = false;
    });
    m_propsForm->addRow(new QLabel(tr("背景颜色")), bgBtn);
}

void PropertyPanelDock::buildPanel(const WidgetInstance &inst, const WidgetMeta &meta)
{
    // 清空旧控件
    clearForms();
    auto addReadOnly = [this](const QString &label, const QString &val) {
        auto *le = new QLineEdit(val);
        le->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        le->setReadOnly(true);
        le->setStyleSheet("QLineEdit { background:#2a2a2a; color:#aaa; }");
        m_metaForm->addRow(new QLabel(label), le);
    };
    if (inst.isGroup) {
        addReadOnly(tr("id"), inst.instanceId);
        addReadOnly(tr("type"), tr("组合"));
        addReadOnly(tr("说明"), tr("仅用于组态编辑，不导出为 LVGL 控件"));

        auto *nameEdit = new QLineEdit(inst.name);
        nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit]() {
            if (!m_scene || m_currentInstanceId.isEmpty()) return;
            m_pushingValue = true;
            m_scene->setInstanceName(m_currentInstanceId, nameEdit->text());
            m_pushingValue = false;
        });
        m_propsForm->addRow(new QLabel(tr("name")), nameEdit);

        auto addGeom = [this](const QString &key, int value) {
            auto *sb = new QSpinBox;
            sb->setRange(-99999, 99999);
            sb->setValue(value);
            m_geometryEditors.insert(key, sb);
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, key](int v) {
                if (!m_scene || m_currentInstanceId.isEmpty()) return;
                m_pushingValue = true;
                m_scene->setInstanceProperty(m_currentInstanceId, key, v);
                m_pushingValue = false;
            });
            m_propsForm->addRow(new QLabel(key), sb);
        };
        addGeom(QStringLiteral("x"), inst.x);
        addGeom(QStringLiteral("y"), inst.y);
        addGeom(QStringLiteral("width"), inst.width);
        addGeom(QStringLiteral("height"), inst.height);
        return;
    }
    addReadOnly(tr("id"),             meta.id);
    addReadOnly(tr("type"),           meta.type);
    addReadOnly(tr("render_mode"),    meta.renderMode);
    addReadOnly(tr("category"),       meta.category);
    addReadOnly(tr("description"),    meta.description);
    addReadOnly(tr("schema_version"), meta.schemaVersion);
    addReadOnly(tr("version"),        meta.version);

    // ===== 第二部分：name + properties（按 group 分组） =====
    // -- name (可编辑) --
    {
        auto *nameEdit = new QLineEdit(inst.name);
        nameEdit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        connect(nameEdit, &QLineEdit::editingFinished, this, [this, nameEdit]() {
            if (!m_scene || m_currentInstanceId.isEmpty()) return;
            m_pushingValue = true;
            m_scene->setInstanceName(m_currentInstanceId, nameEdit->text());
            m_pushingValue = false;
        });
        m_propsForm->addRow(new QLabel(tr("name")), nameEdit);
    }

    // 按 group 把属性分桶（保留首次出现顺序）
    QStringList groupOrder;
    QHash<QString, QList<PropertyMeta>> normal;
    QHash<QString, QList<PropertyMeta>> advanced;
    for (const PropertyMeta &pm : meta.properties) {
        if (pm.hidden) continue;
        const QString g = pm.group.isEmpty() ? tr("其它") : pm.group;
        if (!groupOrder.contains(g)) groupOrder << g;
        if (pm.advanced) advanced[g].append(pm);
        else             normal[g].append(pm);
    }

    auto addPropertyRow = [this, &inst](QFormLayout *form, const PropertyMeta &pm) {
        // 几何属性：x/y/width/height 优先从 WidgetInstance 顶层字段读取实时值
        QVariant value;
        if (pm.name == QLatin1String("x"))           value = inst.x;
        else if (pm.name == QLatin1String("y"))      value = inst.y;
        else if (pm.name == QLatin1String("width"))  value = inst.width;
        else if (pm.name == QLatin1String("height")) value = inst.height;
        else value = inst.properties.value(pm.name, pm.defaultValue);

        QWidget *editor = makeEditor(pm, value);
        if (!editor) return;
        if (pm.readOnly) editor->setEnabled(false);
        if (!pm.description.isEmpty()) editor->setToolTip(pm.description);

        // 缓存几何编辑器供实时回填使用
        if (pm.name == QLatin1String("x") || pm.name == QLatin1String("y") ||
            pm.name == QLatin1String("width") || pm.name == QLatin1String("height")) {
            m_geometryEditors.insert(pm.name, editor);
        }

        QString labelText = pm.label.isEmpty() ? pm.name : pm.label;
        auto *labelW = new QLabel(labelText);
        if (!pm.description.isEmpty()) labelW->setToolTip(pm.description);

        form->addRow(labelW, editor);
    };

    for (const QString &g : std::as_const(groupOrder)) {
        auto *box  = new QGroupBox(g, m_propsBox);
        box->setStyleSheet("QGroupBox { font-weight: bold; margin-top: 6px; }");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        for (const auto &pm : normal.value(g))
            addPropertyRow(form, pm);

        // 高级属性折叠：用一个嵌套 QGroupBox + checkable 实现
        const auto advList = advanced.value(g);
        if (!advList.isEmpty()) {
            auto *adv = new QGroupBox(tr("高级"), box);
            adv->setCheckable(true);
            adv->setChecked(false);
            adv->setStyleSheet("QGroupBox { color:#aaa; }");
            auto *advForm = new QFormLayout(adv);
            advForm->setLabelAlignment(Qt::AlignRight);
            for (const auto &pm : advList)
                addPropertyRow(advForm, pm);
            // 折叠/展开切换显隐
            auto applyVis = [adv](bool checked){
                if (auto *l = adv->layout())
                    for (int i = 0; i < l->count(); ++i)
                        if (auto *w = l->itemAt(i)->widget()) w->setVisible(checked);
            };
            applyVis(false);
            connect(adv, &QGroupBox::toggled, this, [applyVis](bool c){ applyVis(c); });
            form->addRow(adv);
        }
        m_propsForm->addRow(box);
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
        m_pushingValue = true;
        m_scene->setInstanceProperty(m_currentInstanceId, key, v);
        m_pushingValue = false;
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
        const QString suffix = pm.unit.isEmpty() ? QString() : QStringLiteral(" ") + pm.unit;
        if (isInt) {
            auto *sb = new QSpinBox;
            sb->setRange(int(pm.min), int(pm.max));
            sb->setValue(value.toInt());
            if (!suffix.isEmpty()) sb->setSuffix(suffix);
            connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [pushValue](int v){ pushValue(v); });
            return sb;
        }
        auto *sb = new QDoubleSpinBox;
        sb->setRange(pm.min, pm.max);
        sb->setDecimals(3);
        sb->setValue(value.toDouble());
        if (!suffix.isEmpty()) sb->setSuffix(suffix);
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

