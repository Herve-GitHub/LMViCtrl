#include "canvasscene.h"
#include "canvasitem.h"

#include <QGraphicsRectItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QQueue>
#include <QUndoCommand>
#include <QUndoStack>
#include <QUuid>

#include <algorithm>
#include <utility>

static constexpr char kWidgetMimeType[] = "application/x-lvgl-widget";

// ===========================================================================
// Undo 命令
// ===========================================================================

// --- 添加元素 ---------------------------------------------------------------
class AddItemCommand : public QUndoCommand
{
public:
    AddItemCommand(CanvasScene *scene, const WidgetInstance &inst, QUndoCommand *parent = nullptr)
        : QUndoCommand(QStringLiteral("添加 %1").arg(inst.widgetId), parent)
        , m_scene(scene), m_inst(inst) {}

    void undo() override { m_scene->doRemoveItem(m_inst.instanceId); }
    void redo() override { m_scene->doAddItem(m_inst); }

private:
    CanvasScene    *m_scene;
    WidgetInstance  m_inst;
};

// --- 删除元素（支持多个） ---------------------------------------------------
class RemoveItemsCommand : public QUndoCommand
{
public:
    RemoveItemsCommand(CanvasScene *scene, const QList<WidgetInstance> &insts,
                       QUndoCommand *parent = nullptr)
        : QUndoCommand(QStringLiteral("删除 %1 个元素").arg(insts.size()), parent)
        , m_scene(scene), m_insts(insts) {}

    void undo() override { for (const auto &i : m_insts) m_scene->doAddItem(i); }
    void redo() override { for (const auto &i : m_insts) m_scene->doRemoveItem(i.instanceId); }

private:
    CanvasScene            *m_scene;
    QList<WidgetInstance>   m_insts;
};

// --- 移动（支持多个） -------------------------------------------------------
struct MoveData { QString id; QPointF oldPos; QPointF newPos; };
struct ZOrderData { QString id; int oldZ; int newZ; };

class MoveItemsCommand : public QUndoCommand
{
public:
    MoveItemsCommand(CanvasScene *scene, const QList<MoveData> &moves,
                     const QString &text = QString(), QUndoCommand *parent = nullptr)
        : QUndoCommand(text.isEmpty()
              ? QStringLiteral("移动 %1 个元素").arg(moves.size())
              : text, parent)
        , m_scene(scene), m_moves(moves) {}

    void undo() override {
        QList<QPair<QString, QPointF>> list;
        for (const auto &m : m_moves) list.append({m.id, m.oldPos});
        m_scene->doSetPositions(list);
    }
    void redo() override {
        QList<QPair<QString, QPointF>> list;
        for (const auto &m : m_moves) list.append({m.id, m.newPos});
        m_scene->doSetPositions(list);
    }

private:
    CanvasScene       *m_scene;
    QList<MoveData>    m_moves;
};

// --- 层级顺序 ---------------------------------------------------------------
class ChangeZOrderCommand : public QUndoCommand
{
public:
    ChangeZOrderCommand(CanvasScene *scene, const QList<ZOrderData> &changes,
                        const QString &text, QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent)
        , m_scene(scene), m_changes(changes) {}

    void undo() override {
        QList<QPair<QString, int>> list;
        for (const auto &c : m_changes) list.append({c.id, c.oldZ});
        m_scene->doSetZOrders(list);
    }

    void redo() override {
        QList<QPair<QString, int>> list;
        for (const auto &c : m_changes) list.append({c.id, c.newZ});
        m_scene->doSetZOrders(list);
    }

private:
    CanvasScene       *m_scene;
    QList<ZOrderData>  m_changes;
};

// --- 缩放 -------------------------------------------------------------------
class ResizeItemCommand : public QUndoCommand
{
public:
    ResizeItemCommand(CanvasScene *scene, const QString &id,
                      const QRectF &before, const QRectF &after,
                      QUndoCommand *parent = nullptr)
        : QUndoCommand(QStringLiteral("缩放元素"), parent)
        , m_scene(scene), m_id(id), m_before(before), m_after(after) {}

    void undo() override { m_scene->doSetGeometry(m_id, m_before); }
    void redo() override { m_scene->doSetGeometry(m_id, m_after);  }

private:
    CanvasScene *m_scene;
    QString      m_id;
    QRectF       m_before;
    QRectF       m_after;
};

// --- 粘贴（多个） -----------------------------------------------------------
class PasteItemsCommand : public QUndoCommand
{
public:
    PasteItemsCommand(CanvasScene *scene, const QList<WidgetInstance> &insts,
                      QUndoCommand *parent = nullptr)
        : QUndoCommand(QStringLiteral("粘贴 %1 个元素").arg(insts.size()), parent)
        , m_scene(scene), m_insts(insts) {}

    void undo() override { for (const auto &i : m_insts) m_scene->doRemoveItem(i.instanceId); }
    void redo() override { for (const auto &i : m_insts) m_scene->doAddItem(i); }

private:
    CanvasScene           *m_scene;
    QList<WidgetInstance>  m_insts;
};

// --- 快照替换（组合/取消组合） ---------------------------------------------
class ReplaceInstancesCommand : public QUndoCommand
{
public:
    ReplaceInstancesCommand(CanvasScene *scene,
                            const QList<WidgetInstance> &before,
                            const QList<WidgetInstance> &after,
                            const QString &text,
                            QUndoCommand *parent = nullptr)
        : QUndoCommand(text, parent)
        , m_scene(scene), m_before(before), m_after(after) {}

    void undo() override { m_scene->doLoadInstances(m_before); }
    void redo() override { m_scene->doLoadInstances(m_after); }

private:
    CanvasScene *m_scene;
    QList<WidgetInstance> m_before;
    QList<WidgetInstance> m_after;
};

// ===========================================================================
// CanvasScene
// ===========================================================================

CanvasScene::CanvasScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_undoStack(new QUndoStack(this))
{
    // 背景——深色画布区域
    m_bgItem = addRect(0, 0, m_canvasSize.width(), m_canvasSize.height(),
                       QPen(QColor("#555555"), 1),
                       QBrush(m_canvasBgColor));
    m_bgItem->setZValue(-100);

    setBackgroundBrush(QColor("#2b2b2b"));

    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        for (QGraphicsItem *gi : items()) {
            if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
                ci->update();
        }
        if (m_adjustingGroupSelection) return;

        QSet<QString> selectedGroupIds;
        for (QGraphicsItem *gi : selectedItems()) {
            if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
                if (ci->instance().isGroup)
                    selectedGroupIds.insert(ci->instance().instanceId);
            }
        }
        if (selectedGroupIds.isEmpty()) return;

        bool changed = false;
        do {
            changed = false;
            for (QGraphicsItem *gi : items()) {
                auto *ci = qgraphicsitem_cast<CanvasItem*>(gi);
                if (!ci) continue;
                const WidgetInstance inst = ci->instance();
                if (!inst.parentId.isEmpty() && selectedGroupIds.contains(inst.parentId) && !ci->isSelected()) {
                    m_adjustingGroupSelection = true;
                    ci->setSelected(true);
                    m_adjustingGroupSelection = false;
                    changed = true;
                    if (inst.isGroup)
                        selectedGroupIds.insert(inst.instanceId);
                }
            }
        } while (changed);
    });
}

// ---------------------------------------------------------------------------
// 画布尺寸
// ---------------------------------------------------------------------------
void CanvasScene::setCanvasSize(int width, int height)
{
    const QSize oldSize = m_canvasSize;
    const bool changed = (m_canvasSize != QSize(width, height));
    m_canvasSize = QSize(width, height);
    m_bgItem->setRect(0, 0, width, height);
    setSceneRect(-50, -50, width + 100, height + 100);
    if (changed) {
        emit canvasChanged();
        if (!m_suppressOperationLog) {
            emit operationLogged(tr("修改画布大小：%1x%2 -> %3x%4")
                .arg(oldSize.width()).arg(oldSize.height()).arg(width).arg(height));
        }
    }
}

void CanvasScene::setCanvasBackgroundColor(const QColor &color)
{
    if (!color.isValid() || color == m_canvasBgColor) return;
    const QString oldColor = m_canvasBgColor.name(QColor::HexRgb).toUpper();
    m_canvasBgColor = color;
    if (m_bgItem)
        m_bgItem->setBrush(QBrush(m_canvasBgColor));
    emit canvasChanged();
    if (!m_suppressOperationLog) {
        emit operationLogged(tr("修改画布背景颜色：%1 -> %2")
            .arg(oldColor, m_canvasBgColor.name(QColor::HexRgb).toUpper()));
    }
}

// ---------------------------------------------------------------------------
// WidgetMeta 注册
// ---------------------------------------------------------------------------
void CanvasScene::setWidgetMetas(const QList<WidgetMeta> &metas)
{
    m_metaMap.clear();
    for (const auto &m : metas)
        m_metaMap.insert(m.id, m);
}

// ---------------------------------------------------------------------------
// 内部 make / find
// ---------------------------------------------------------------------------
CanvasItem *CanvasScene::makeItem(const WidgetInstance &inst)
{
    const WidgetMeta meta = m_metaMap.value(inst.widgetId);
    auto *ci = new CanvasItem(inst, meta);
    connect(ci, &CanvasItem::resizeCommitted,
            this, &CanvasScene::onResizeCommitted);
    connect(ci, &CanvasItem::geometryChanged,
            this, &CanvasScene::instanceGeometryChanged);
    return ci;
}

CanvasItem *CanvasScene::findItem(const QString &instanceId) const
{
    for (QGraphicsItem *gi : items()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            if (ci->instance().instanceId == instanceId)
                return ci;
        }
    }
    return nullptr;
}

int CanvasScene::nextZOrder() const
{
    int maxZ = -1;
    for (QGraphicsItem *gi : items()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            maxZ = qMax(maxZ, ci->instance().zOrder);
    }
    return maxZ + 1;
}

QList<CanvasItem *> CanvasScene::canvasItemsSortedByZ() const
{
    QList<CanvasItem *> result;
    for (QGraphicsItem *gi : items()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            result.append(ci);
    }
    std::stable_sort(result.begin(), result.end(), [](CanvasItem *a, CanvasItem *b) {
        return a->instance().zOrder < b->instance().zOrder;
    });
    return result;
}

QList<WidgetInstance> CanvasScene::normalizedZOrders(QList<WidgetInstance> instances) const
{
    std::stable_sort(instances.begin(), instances.end(), [](const WidgetInstance &a, const WidgetInstance &b) {
        return a.zOrder < b.zOrder;
    });
    for (int i = 0; i < instances.size(); ++i)
        instances[i].zOrder = i;
    return instances;
}

// ---------------------------------------------------------------------------
// do* — 供 undo 命令直接调用
// ---------------------------------------------------------------------------
void CanvasScene::doAddItem(const WidgetInstance &inst)
{
    CanvasItem *ci = makeItem(inst);
    addItem(ci);
    emit sceneItemsChanged();
    if (!m_suppressOperationLog) {
        emit operationLogged(tr("添加元素：%1 (%2)，位置 %3,%4，大小 %5x%6")
            .arg(inst.name.isEmpty() ? inst.widgetId : inst.name,
                 inst.widgetId)
            .arg(inst.x).arg(inst.y).arg(inst.width).arg(inst.height));
    }
}

void CanvasScene::doRemoveItem(const QString &instanceId)
{
    if (CanvasItem *ci = findItem(instanceId)) {
        const WidgetInstance inst = ci->instance();
        removeItem(ci);
        delete ci;
        emit sceneItemsChanged();
        if (!m_suppressOperationLog) {
            emit operationLogged(tr("删除元素：%1 (%2)")
                .arg(inst.name.isEmpty() ? inst.widgetId : inst.name,
                     inst.widgetId));
        }
    }
}

void CanvasScene::doSetGeometry(const QString &instanceId, const QRectF &rect)
{
    if (CanvasItem *ci = findItem(instanceId)) {
        const WidgetInstance before = ci->instance();
        ci->applyGeometry(rect);
        const WidgetInstance after = ci->instance();
        if (!m_suppressOperationLog) {
            emit operationLogged(tr("调整元素：%1，%2,%3 %4x%5 -> %6,%7 %8x%9")
                .arg(after.name.isEmpty() ? after.widgetId : after.name)
                .arg(before.x).arg(before.y).arg(before.width).arg(before.height)
                .arg(after.x).arg(after.y).arg(after.width).arg(after.height));
        }
    }
}

void CanvasScene::doSetPositions(const QList<QPair<QString, QPointF>> &moves)
{
    for (const auto &[id, pos] : moves) {
        if (CanvasItem *ci = findItem(id)) {
            const WidgetInstance before = ci->instance();
            ci->setPos(pos);
            const WidgetInstance after = ci->instance();
            if (!m_suppressOperationLog && (before.x != after.x || before.y != after.y)) {
                emit operationLogged(tr("移动元素：%1，%2,%3 -> %4,%5")
                    .arg(after.name.isEmpty() ? after.widgetId : after.name)
                    .arg(before.x).arg(before.y).arg(after.x).arg(after.y));
            }
        }
    }
}

void CanvasScene::doSetZOrders(const QList<QPair<QString, int>> &orders)
{
    bool changed = false;
    for (const auto &[id, z] : orders) {
        if (CanvasItem *ci = findItem(id)) {
            if (ci->instance().zOrder != z)
                changed = true;
            ci->setZOrder(z);
            emit instanceChanged(id);
        }
    }
    if (changed)
        emit sceneItemsChanged();
}

void CanvasScene::doLoadInstances(const QList<WidgetInstance> &instances)
{
    m_suppressOperationLog = true;
    const auto its = items();
    for (QGraphicsItem *gi : its) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            removeItem(ci);
            delete ci;
        }
    }
    m_pressPositions.clear();
    m_resizingIds.clear();
    for (const WidgetInstance &inst : instances)
        doAddItem(inst);
    m_suppressOperationLog = false;
    emit sceneItemsChanged();
}

// ---------------------------------------------------------------------------
// 公共编辑操作
// ---------------------------------------------------------------------------
void CanvasScene::deleteSelected()
{
    const auto sel = selectedItems();
    if (sel.isEmpty()) return;

    QList<WidgetInstance> insts;
    for (QGraphicsItem *gi : sel) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            insts.append(ci->instance());
    }
    m_undoStack->push(new RemoveItemsCommand(this, insts));
}

void CanvasScene::copySelected()
{
    m_clipboard = selectedInstances();
    m_pasteCount = 0;
    if (!m_clipboard.isEmpty())
        emit operationLogged(tr("复制元素：%1 个").arg(m_clipboard.size()));
}

void CanvasScene::pasteClipboard()
{
    if (m_clipboard.isEmpty()) return;

    ++m_pasteCount;
    pasteInstances(m_clipboard, m_pasteCount);
}

QList<WidgetInstance> CanvasScene::selectedInstances() const
{
    QList<WidgetInstance> roots;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            roots.append(ci->instance());
    }
    return instancesWithDescendants(roots);
}

QList<WidgetInstance> CanvasScene::instancesWithDescendants(const QList<WidgetInstance> &roots) const
{
    QList<WidgetInstance> all = allInstances();
    QHash<QString, WidgetInstance> byId;
    QHash<QString, QList<QString>> children;
    for (const WidgetInstance &inst : all) {
        byId.insert(inst.instanceId, inst);
        if (!inst.parentId.isEmpty())
            children[inst.parentId].append(inst.instanceId);
    }

    QList<WidgetInstance> result;
    QSet<QString> seen;
    QQueue<QString> queue;
    for (const WidgetInstance &inst : roots)
        queue.enqueue(inst.instanceId);

    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        if (seen.contains(id) || !byId.contains(id)) continue;
        seen.insert(id);
        result.append(byId.value(id));
        for (const QString &childId : children.value(id))
            queue.enqueue(childId);
    }
    return result;
}

QString CanvasScene::uniqueGroupName() const
{
    QSet<QString> used;
    for (const WidgetInstance &inst : allInstances())
        if (!inst.name.isEmpty()) used.insert(inst.name);

    int n = 1;
    QString name;
    do { name = QStringLiteral("Group_%1").arg(n++); }
    while (used.contains(name));
    return name;
}

void CanvasScene::expandSelectionToMoveGroups()
{
    QHash<QString, WidgetInstance> byId;
    for (const WidgetInstance &inst : allInstances())
        byId.insert(inst.instanceId, inst);

    QSet<QString> groupIds;
    for (QGraphicsItem *gi : selectedItems()) {
        auto *ci = qgraphicsitem_cast<CanvasItem*>(gi);
        if (!ci) continue;
        const WidgetInstance inst = ci->instance();
        if (inst.isGroup) {
            groupIds.insert(inst.instanceId);
        } else if (!inst.parentId.isEmpty()) {
            const WidgetInstance parent = byId.value(inst.parentId);
            if (parent.isGroup)
                groupIds.insert(parent.instanceId);
        }
    }
    if (groupIds.isEmpty()) return;

    QList<WidgetInstance> groupRoots;
    for (const QString &id : std::as_const(groupIds)) {
        if (byId.contains(id))
            groupRoots.append(byId.value(id));
    }

    QSet<QString> idsToSelect;
    for (const WidgetInstance &inst : instancesWithDescendants(groupRoots))
        idsToSelect.insert(inst.instanceId);
    if (idsToSelect.isEmpty()) return;

    bool changed = false;
    m_adjustingGroupSelection = true;
    for (QGraphicsItem *gi : items()) {
        auto *ci = qgraphicsitem_cast<CanvasItem*>(gi);
        if (!ci) continue;
        if (idsToSelect.contains(ci->instance().instanceId) && !ci->isSelected()) {
            ci->setSelected(true);
            changed = true;
        }
    }
    m_adjustingGroupSelection = false;

    if (changed) {
        for (QGraphicsItem *gi : items()) {
            if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
                ci->update();
        }
    }
}

void CanvasScene::pasteInstances(const QList<WidgetInstance> &instances, int pasteCount)
{
    if (instances.isEmpty()) return;

    const int off = pasteCount * 20;

    QSet<QString> usedNames;
    for (QGraphicsItem *gi : items()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            if (!ci->instance().name.isEmpty())
                usedNames.insert(ci->instance().name);
    }

    auto uniqueLocalName = [&usedNames](QString base) {
        base = base.trimmed();
        if (base.isEmpty()) base = QStringLiteral("widget");

        QString cleaned;
        for (QChar c : base) {
            if (c.isLetterOrNumber() || c == QLatin1Char('_')) cleaned.append(c);
            else cleaned.append(QLatin1Char('_'));
        }
        if (cleaned.isEmpty()) cleaned = QStringLiteral("widget");

        int n = 1;
        QString cand;
        do { cand = QStringLiteral("%1_%2").arg(cleaned).arg(n++); }
        while (usedNames.contains(cand));
        usedNames.insert(cand);
        return cand;
    };

    QHash<QString, QString> idMap;
    for (const WidgetInstance &inst : instances)
        idMap.insert(inst.instanceId, QUuid::createUuid().toString(QUuid::WithoutBraces));

    QList<WidgetInstance> newInsts;
    int zOrder = nextZOrder();
    for (WidgetInstance inst : instances) {
        const QString oldId = inst.instanceId;
        inst.instanceId = idMap.value(oldId);
        if (!inst.parentId.isEmpty())
            inst.parentId = idMap.value(inst.parentId, QString());
        inst.x += off;
        inst.y += off;
        QString baseName = inst.name;
        if (baseName.isEmpty() && m_metaMap.contains(inst.widgetId))
            baseName = m_metaMap.value(inst.widgetId).name;
        if (baseName.isEmpty()) baseName = inst.widgetId;

        if (m_nameGen) {
            inst.name = m_nameGen(baseName);
            while (usedNames.contains(inst.name))
                inst.name = uniqueLocalName(inst.name);
            usedNames.insert(inst.name);
        } else {
            inst.name = uniqueLocalName(baseName);
        }
        inst.zOrder = zOrder++;
        newInsts.append(inst);
    }
    m_undoStack->push(new PasteItemsCommand(this, newInsts));

    // 粘贴后选中新元素
    clearSelection();
    for (const auto &inst : newInsts) {
        if (CanvasItem *ci = findItem(inst.instanceId))
            ci->setSelected(true);
    }
}

void CanvasScene::groupSelected()
{
    QList<WidgetInstance> selected;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            const WidgetInstance inst = ci->instance();
            if (!inst.isGroup)
                selected.append(inst);
        }
    }
    if (selected.size() < 2) return;

    const QString parentId = selected.first().parentId;
    for (const WidgetInstance &inst : selected) {
        if (inst.parentId != parentId) {
            emit operationLogged(tr("组合失败：请选择同一组合层级下的元素"));
            return;
        }
    }

    QRectF bounds;
    int zOrder = selected.first().zOrder;
    QSet<QString> selectedIds;
    for (const WidgetInstance &inst : selected) {
        const QRectF itemRect(inst.x, inst.y, inst.width, inst.height);
        bounds = bounds.isNull() ? itemRect : bounds.united(itemRect);
        zOrder = qMax(zOrder, inst.zOrder + 1);
        selectedIds.insert(inst.instanceId);
    }

    WidgetInstance group;
    group.instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    group.widgetId = QStringLiteral("__group__");
    group.name = uniqueGroupName();
    group.parentId = parentId;
    group.isGroup = true;
    group.zOrder = zOrder;
    group.x = qRound(bounds.x());
    group.y = qRound(bounds.y());
    group.width = qRound(bounds.width());
    group.height = qRound(bounds.height());

    const QList<WidgetInstance> before = allInstances();
    QList<WidgetInstance> after = before;
    for (WidgetInstance &inst : after) {
        if (selectedIds.contains(inst.instanceId))
            inst.parentId = group.instanceId;
    }
    after.append(group);

    m_undoStack->push(new ReplaceInstancesCommand(this, before, after,
        tr("组合 %1 个元素").arg(selected.size())));

    clearSelection();
    if (CanvasItem *ci = findItem(group.instanceId))
        ci->setSelected(true);
    emit operationLogged(tr("组合元素：%1 个 -> %2").arg(selected.size()).arg(group.name));
}

void CanvasScene::ungroupSelected()
{
    QSet<QString> groupIds;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            if (ci->instance().isGroup)
                groupIds.insert(ci->instance().instanceId);
        }
    }
    if (groupIds.isEmpty()) return;

    const QList<WidgetInstance> before = allInstances();
    QHash<QString, QString> groupParentIds;
    for (const WidgetInstance &inst : before) {
        if (groupIds.contains(inst.instanceId))
            groupParentIds.insert(inst.instanceId, inst.parentId);
    }

    QList<WidgetInstance> after;
    QList<QString> releasedIds;
    for (WidgetInstance inst : before) {
        if (groupIds.contains(inst.instanceId))
            continue;
        if (groupIds.contains(inst.parentId)) {
            inst.parentId = groupParentIds.value(inst.parentId);
            releasedIds.append(inst.instanceId);
        }
        after.append(inst);
    }

    m_undoStack->push(new ReplaceInstancesCommand(this, before, after,
        tr("取消组合 %1 个组合").arg(groupIds.size())));

    clearSelection();
    for (const QString &id : std::as_const(releasedIds)) {
        if (CanvasItem *ci = findItem(id))
            ci->setSelected(true);
    }
    emit operationLogged(tr("取消组合：%1 个组合").arg(groupIds.size()));
}

void CanvasScene::alignSelected(AlignMode mode)
{
    QHash<QString, WidgetInstance> byId;
    for (const WidgetInstance &inst : allInstances())
        byId.insert(inst.instanceId, inst);

    QSet<QString> selectedIds;
    QList<CanvasItem *> selectedCanvasItems;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            selectedIds.insert(ci->instance().instanceId);
            selectedCanvasItems.append(ci);
        }
    }

    auto hasSelectedAncestor = [&byId, &selectedIds](const WidgetInstance &inst) {
        QString parentId = inst.parentId;
        while (!parentId.isEmpty()) {
            if (selectedIds.contains(parentId))
                return true;
            if (!byId.contains(parentId))
                break;
            parentId = byId.value(parentId).parentId;
        }
        return false;
    };

    QList<CanvasItem *> alignItems;
    for (CanvasItem *ci : std::as_const(selectedCanvasItems)) {
        if (!hasSelectedAncestor(ci->instance()))
            alignItems.append(ci);
    }
    if (alignItems.size() < 2) return;

    QRectF bounds;
    for (CanvasItem *ci : alignItems) {
        const WidgetInstance inst = ci->instance();
        const QRectF itemRect(ci->pos(), QSizeF(inst.width, inst.height));
        bounds = bounds.isNull() ? itemRect : bounds.united(itemRect);
    }

    QList<MoveData> moves;
    QSet<QString> movedIds;
    auto appendMove = [&moves, &movedIds](const QString &id, const QPointF &oldPos, const QPointF &newPos) {
        if (movedIds.contains(id) || newPos == oldPos)
            return;
        moves.append({id, oldPos, newPos});
        movedIds.insert(id);
    };

    for (CanvasItem *ci : alignItems) {
        const WidgetInstance inst = ci->instance();
        const QPointF oldPos = ci->pos();
        QPointF newPos = oldPos;

        switch (mode) {
        case AlignMode::Left:
            newPos.setX(bounds.left());
            break;
        case AlignMode::Right:
            newPos.setX(bounds.right() - inst.width);
            break;
        case AlignMode::Top:
            newPos.setY(bounds.top());
            break;
        case AlignMode::Bottom:
            newPos.setY(bounds.bottom() - inst.height);
            break;
        case AlignMode::Center:
            newPos.setX(bounds.center().x() - inst.width / 2.0);
            break;
        }

        appendMove(inst.instanceId, oldPos, newPos);

        const QPointF delta = newPos - oldPos;
        if (delta.isNull())
            continue;

        for (const WidgetInstance &descendant : instancesWithDescendants({inst})) {
            if (descendant.instanceId == inst.instanceId)
                continue;
            if (CanvasItem *descendantItem = findItem(descendant.instanceId)) {
                const QPointF descendantOldPos = descendantItem->pos();
                appendMove(descendant.instanceId, descendantOldPos, descendantOldPos + delta);
            }
        }
    }
    if (moves.isEmpty()) return;

    QString text;
    switch (mode) {
    case AlignMode::Left:   text = tr("左对齐 %1 个元素"); break;
    case AlignMode::Right:  text = tr("右对齐 %1 个元素"); break;
    case AlignMode::Top:    text = tr("顶部对齐 %1 个元素"); break;
    case AlignMode::Bottom: text = tr("底部对齐 %1 个元素"); break;
    case AlignMode::Center: text = tr("居中对齐 %1 个元素"); break;
    }
    m_undoStack->push(new MoveItemsCommand(this, moves, text.arg(alignItems.size())));
}

void CanvasScene::changeSelectedZOrder(ZOrderMode mode)
{
    if (selectedItems().isEmpty()) return;
    expandSelectionToMoveGroups();

    QSet<QString> selectedIds;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            selectedIds.insert(ci->instance().instanceId);
    }
    if (selectedIds.isEmpty()) return;

    QList<CanvasItem *> ordered = canvasItemsSortedByZ();
    if (ordered.size() < 2) return;

    auto isSelected = [&selectedIds](CanvasItem *item) {
        return selectedIds.contains(item->instance().instanceId);
    };

    switch (mode) {
    case ZOrderMode::BringToFront: {
        QList<CanvasItem *> unselected;
        QList<CanvasItem *> selected;
        for (CanvasItem *item : std::as_const(ordered)) {
            if (isSelected(item)) selected.append(item);
            else                  unselected.append(item);
        }
        ordered = unselected + selected;
        break;
    }
    case ZOrderMode::SendToBack: {
        QList<CanvasItem *> unselected;
        QList<CanvasItem *> selected;
        for (CanvasItem *item : std::as_const(ordered)) {
            if (isSelected(item)) selected.append(item);
            else                  unselected.append(item);
        }
        ordered = selected + unselected;
        break;
    }
    case ZOrderMode::BringForward:
        for (int i = ordered.size() - 2; i >= 0; --i) {
            if (isSelected(ordered[i]) && !isSelected(ordered[i + 1]))
                ordered.swapItemsAt(i, i + 1);
        }
        break;
    case ZOrderMode::SendBackward:
        for (int i = 1; i < ordered.size(); ++i) {
            if (isSelected(ordered[i]) && !isSelected(ordered[i - 1]))
                ordered.swapItemsAt(i, i - 1);
        }
        break;
    }

    QList<ZOrderData> changes;
    for (int i = 0; i < ordered.size(); ++i) {
        const WidgetInstance inst = ordered[i]->instance();
        if (inst.zOrder != i)
            changes.append({inst.instanceId, inst.zOrder, i});
    }
    if (changes.isEmpty()) return;

    QString text;
    switch (mode) {
    case ZOrderMode::BringToFront:  text = tr("置于顶层"); break;
    case ZOrderMode::SendToBack:    text = tr("置于底层"); break;
    case ZOrderMode::BringForward:  text = tr("上移一层"); break;
    case ZOrderMode::SendBackward:  text = tr("下移一层"); break;
    }
    m_undoStack->push(new ChangeZOrderCommand(this, changes, text));
    emit operationLogged(tr("调整层级：%1，影响 %2 个元素")
        .arg(text).arg(changes.size()));
}

QList<WidgetInstance> CanvasScene::allInstances() const
{
    QList<WidgetInstance> result;
    for (QGraphicsItem *gi : items()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            result.append(ci->instance());
    }
    return result;
}

QList<WidgetInstance> CanvasScene::instancesSortedByZ() const
{
    QList<WidgetInstance> result;
    const QList<CanvasItem *> sortedItems = canvasItemsSortedByZ();
    for (CanvasItem *item : sortedItems)
        result.append(item->instance());
    return result;
}

void CanvasScene::clearAllItems()
{
    bool removed = false;
    const auto its = items();
    for (QGraphicsItem *gi : its) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
            removeItem(ci);
            delete ci;
            removed = true;
        }
    }
    m_clipboard.clear();
    m_pasteCount = 0;
    m_pressPositions.clear();
    m_resizingIds.clear();
    m_undoStack->clear();
    if (removed)
        emit sceneItemsChanged();
}

void CanvasScene::loadInstances(const QList<WidgetInstance> &instances)
{
    m_suppressOperationLog = true;
    clearAllItems();
    const QList<WidgetInstance> normalized = normalizedZOrders(instances);
    for (const auto &inst : normalized)
        doAddItem(inst);
    m_suppressOperationLog = false;
}

// ---------------------------------------------------------------------------
// 拖放（从 WidgetToolbox 拖进来）
// ---------------------------------------------------------------------------
void CanvasScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kWidgetMimeType)))
        event->acceptProposedAction();
    else
        event->ignore();
}

void CanvasScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kWidgetMimeType)))
        event->acceptProposedAction();
    else
        event->ignore();
}

void CanvasScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    const QByteArray data = event->mimeData()->data(QLatin1String(kWidgetMimeType));
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    if (obj.isEmpty()) { event->ignore(); return; }

    const QString widgetId    = obj[QStringLiteral("widgetId")].toString();
    const QString luaFilePath = obj[QStringLiteral("luaFilePath")].toString();

    // 默认宽高：优先使用 meta.defaultSize；其次 width/height 属性默认值
    int defW = 120, defH = 60;

    WidgetInstance inst;
    if (m_metaMap.contains(widgetId)) {
        const auto &meta = m_metaMap.value(widgetId);
        if (meta.defaultSize.isValid()) {
            defW = meta.defaultSize.width();
            defH = meta.defaultSize.height();
        }
        for (const auto &p : meta.properties) {
            if (p.name == QLatin1String("width"))  defW = p.defaultValue.toInt();
            if (p.name == QLatin1String("height")) defH = p.defaultValue.toInt();
            inst.properties[p.name] = p.defaultValue;
        }
        // 兜底：min/max 限制
        if (meta.minSize.isValid()) {
            defW = qMax(defW, meta.minSize.width());
            defH = qMax(defH, meta.minSize.height());
        }
        if (meta.maxSize.isValid()) {
            defW = qMin(defW, meta.maxSize.width());
            defH = qMin(defH, meta.maxSize.height());
        }
    }
    inst.instanceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    inst.widgetId   = widgetId;
    inst.x          = qRound(event->scenePos().x()) - defW / 2;
    inst.y          = qRound(event->scenePos().y()) - defH / 2;
    inst.width      = defW;
    inst.height     = defH;
    inst.zOrder     = nextZOrder();

    // 生成工程内唯一名字（fallback 到本场景内部唯一）
    QString baseName = m_metaMap.contains(widgetId)
        ? m_metaMap.value(widgetId).name : widgetId;
    if (baseName.isEmpty()) baseName = QStringLiteral("widget");
    if (m_nameGen) {
        inst.name = m_nameGen(baseName);
    } else {
        // 简易 fallback：在本 scene 内查找冲突
        QSet<QString> used;
        for (QGraphicsItem *gi : items())
            if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
                used.insert(ci->instance().name);
        int n = 1;
        QString cand;
        do { cand = QStringLiteral("%1_%2").arg(baseName).arg(n++); }
        while (used.contains(cand));
        inst.name = cand;
    }

    m_undoStack->push(new AddItemCommand(this, inst));
    event->acceptProposedAction();
}

void CanvasScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    CanvasItem *targetItem = nullptr;
    const QList<QGraphicsItem *> hitItems = items(event->scenePos());
    for (QGraphicsItem *gi : hitItems) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem *>(gi)) {
            targetItem = ci;
            break;
        }
    }

    if (targetItem && !targetItem->isSelected()) {
        clearSelection();
        targetItem->setSelected(true);
    }

    QList<CanvasItem *> selectedCanvasItems;
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem *>(gi))
            selectedCanvasItems.append(ci);
    }
    const bool hasSelection = !selectedCanvasItems.isEmpty();
    const bool canOpenEvents = selectedCanvasItems.size() == 1
        && !selectedCanvasItems.first()->instance().isGroup;

    QMenu menu;
    QAction *copyAction = menu.addAction(tr("复制"));
    QAction *pasteAction = menu.addAction(tr("粘贴"));
    QAction *cutAction = menu.addAction(tr("剪切"));
    QAction *deleteAction = menu.addAction(tr("删除"));
    menu.addSeparator();
    QAction *bringForwardAction = menu.addAction(tr("上移一层"));
    QAction *sendBackwardAction = menu.addAction(tr("下移一层"));
    QAction *bringToFrontAction = menu.addAction(tr("置于顶层"));
    QAction *sendToBackAction = menu.addAction(tr("置于底层"));
    menu.addSeparator();
    QAction *eventAction = menu.addAction(tr("event"));

    copyAction->setEnabled(hasSelection);
    cutAction->setEnabled(hasSelection);
    deleteAction->setEnabled(hasSelection);
    bringForwardAction->setEnabled(hasSelection);
    sendBackwardAction->setEnabled(hasSelection);
    bringToFrontAction->setEnabled(hasSelection);
    sendToBackAction->setEnabled(hasSelection);
    eventAction->setEnabled(canOpenEvents);

    QAction *chosen = menu.exec(event->screenPos());
    if (!chosen) return;

    if (chosen == copyAction) emit copyRequested();
    else if (chosen == pasteAction) emit pasteRequested();
    else if (chosen == cutAction) emit cutRequested();
    else if (chosen == deleteAction) emit deleteRequested();
    else if (chosen == bringForwardAction) emit bringForwardRequested();
    else if (chosen == sendBackwardAction) emit sendBackwardRequested();
    else if (chosen == bringToFrontAction) emit bringToFrontRequested();
    else if (chosen == sendToBackAction) emit sendToBackRequested();
    else if (chosen == eventAction && canOpenEvents) {
        emit eventPanelRequested(selectedCanvasItems.first()->instance().instanceId);
    }
    event->accept();
}

// ---------------------------------------------------------------------------
// 键盘
// ---------------------------------------------------------------------------
void CanvasScene::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelected();
        event->accept();
        return;
    }
    // 方向键微移
    if (!selectedItems().isEmpty()) {
        int dx = 0, dy = 0;
        const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
        switch (event->key()) {
        case Qt::Key_Left:  dx = -step; break;
        case Qt::Key_Right: dx =  step; break;
        case Qt::Key_Up:    dy = -step; break;
        case Qt::Key_Down:  dy =  step; break;
        default: break;
        }
        if (dx || dy) {
            expandSelectionToMoveGroups();
            QList<MoveData> moves;
            for (QGraphicsItem *gi : selectedItems()) {
                if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi)) {
                    const QPointF op = ci->pos();
                    const QPointF np = op + QPointF(dx, dy);
                    moves.append({ci->instance().instanceId, op, np});
                }
            }
            if (!moves.isEmpty()) {
                m_undoStack->push(new MoveItemsCommand(this, moves));
                // 立即应用新位置
                for (const auto &mv : moves)
                    if (CanvasItem *ci = findItem(mv.id))
                        ci->setPos(mv.newPos);
            }
            event->accept();
            return;
        }
    }
    QGraphicsScene::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// 移动 undo 追踪
// ---------------------------------------------------------------------------
void CanvasScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_pressPositions.clear();
    m_resizingIds.clear();

    QGraphicsScene::mousePressEvent(event);
    expandSelectionToMoveGroups();
    for (QGraphicsItem *gi : selectedItems()) {
        if (auto *ci = qgraphicsitem_cast<CanvasItem*>(gi))
            m_pressPositions.insert(ci->instance().instanceId, ci->pos());
    }
}

void CanvasScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsScene::mouseReleaseEvent(event);  // item 的 mouseRelease 在此内部调用

    // 收集移动了的（且不是刚完成 resize 的）items
    QList<MoveData> moves;
    for (auto it = m_pressPositions.constBegin(); it != m_pressPositions.constEnd(); ++it) {
        if (m_resizingIds.contains(it.key())) continue;
        CanvasItem *ci = findItem(it.key());
        if (!ci) continue;
        if (ci->pos() != it.value())
            moves.append({it.key(), it.value(), ci->pos()});
    }
    if (!moves.isEmpty())
        m_undoStack->push(new MoveItemsCommand(this, moves));

    m_pressPositions.clear();
    m_resizingIds.clear();
}

// ---------------------------------------------------------------------------
// resize 完成通知
// ---------------------------------------------------------------------------
void CanvasScene::onResizeCommitted(const QString &instanceId,
                                    const QRectF  &before,
                                    const QRectF  &after)
{
    m_resizingIds.insert(instanceId);
    m_undoStack->push(new ResizeItemCommand(this, instanceId, before, after));
}

// ---------------------------------------------------------------------------
// 属性面板使用：查询 / 修改实例
// ---------------------------------------------------------------------------
bool CanvasScene::instance(const QString &instanceId, WidgetInstance *out) const
{
    if (!out) return false;
    if (CanvasItem *ci = findItem(instanceId)) {
        *out = ci->instance();
        return true;
    }
    return false;
}

void CanvasScene::setInstanceName(const QString &instanceId, const QString &name)
{
    CanvasItem *ci = findItem(instanceId);
    if (!ci) return;
    if (ci->instance().name == name) return;
    const QString oldName = ci->instance().name;
    ci->setInstanceName(name);
    emit instanceChanged(instanceId);
    emit operationLogged(tr("修改元素名称：%1 -> %2").arg(oldName, name));
}

void CanvasScene::setInstanceProperty(const QString &instanceId,
                                      const QString &key,
                                      const QVariant &value)
{
    CanvasItem *ci = findItem(instanceId);
    if (!ci) return;
    const WidgetInstance before = ci->instance();
    QVariant oldValue;
    if (key == QLatin1String("x")) oldValue = before.x;
    else if (key == QLatin1String("y")) oldValue = before.y;
    else if (key == QLatin1String("width")) oldValue = before.width;
    else if (key == QLatin1String("height")) oldValue = before.height;
    else oldValue = before.properties.value(key);
    ci->setInstanceProperty(key, value);
    emit instanceChanged(instanceId);
    emit operationLogged(tr("修改属性：%1.%2，%3 -> %4")
        .arg(before.name.isEmpty() ? before.widgetId : before.name,
             key,
             oldValue.toString(),
             value.toString()));
}

QList<WidgetEventBinding> CanvasScene::instanceEventBindings(const QString &instanceId) const
{
    if (CanvasItem *ci = findItem(instanceId))
        return ci->instance().eventBindings;
    return {};
}

void CanvasScene::setInstanceEventBindings(const QString &instanceId,
                                           const QList<WidgetEventBinding> &bindings)
{
    CanvasItem *ci = findItem(instanceId);
    if (!ci) return;
    ci->setEventBindings(bindings);
    emit instanceEventsChanged(instanceId);
    emit instanceChanged(instanceId);
    emit operationLogged(tr("修改事件：%1")
        .arg(ci->instance().name.isEmpty() ? ci->instance().widgetId : ci->instance().name));
}

void CanvasScene::addInstanceEventAction(const QString &instanceId,
                                         const QString &eventName,
                                         const EventAction &action)
{
    QList<WidgetEventBinding> bindings = instanceEventBindings(instanceId);
    int index = -1;
    for (int i = 0; i < bindings.size(); ++i) {
        if (bindings[i].eventName == eventName) { index = i; break; }
    }
    if (index < 0) {
        WidgetEventBinding binding;
        binding.eventName = eventName;
        binding.actions.append(action);
        bindings.append(binding);
    } else {
        bindings[index].actions.append(action);
    }
    setInstanceEventBindings(instanceId, bindings);
}

void CanvasScene::updateInstanceEventAction(const QString &instanceId,
                                            const QString &eventName,
                                            const QString &actionId,
                                            const EventAction &action)
{
    QList<WidgetEventBinding> bindings = instanceEventBindings(instanceId);
    for (WidgetEventBinding &binding : bindings) {
        if (binding.eventName != eventName) continue;
        for (EventAction &item : binding.actions) {
            if (item.id == actionId) {
                item = action;
                setInstanceEventBindings(instanceId, bindings);
                return;
            }
        }
    }
}

void CanvasScene::removeInstanceEventAction(const QString &instanceId,
                                            const QString &eventName,
                                            const QString &actionId)
{
    QList<WidgetEventBinding> bindings = instanceEventBindings(instanceId);
    for (int i = 0; i < bindings.size(); ++i) {
        if (bindings[i].eventName != eventName) continue;
        auto &actions = bindings[i].actions;
        for (int j = actions.size() - 1; j >= 0; --j) {
            if (actions[j].id == actionId)
                actions.removeAt(j);
        }
        if (actions.isEmpty())
            bindings.removeAt(i);
        setInstanceEventBindings(instanceId, bindings);
        return;
    }
}
