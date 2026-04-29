#include "canvasitem.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStyleOptionGraphicsItem>

// ---------------------------------------------------------------------------
// 加载 widget 预览图：img/widgets/<lua 同名>.png（带缓存）
// ---------------------------------------------------------------------------
static QPixmap loadWidgetPixmap(const QString &luaFilePath)
{
    static QHash<QString, QPixmap> cache;

    const QString key = QFileInfo(luaFilePath).completeBaseName();
    if (key.isEmpty()) return {};

    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) return it.value();

    QPixmap pix;
    // 优先与 lua 文件同目录的 ../img/widgets/<key>.png；
    // 退化到 当前工作目录/img/widgets/<key>.png
    const QFileInfo luaFi(luaFilePath);
    QString imgPath;
    if (luaFi.exists()) {
        imgPath = QDir(luaFi.absolutePath() + QStringLiteral("/../../img/widgets"))
                      .absoluteFilePath(key + QStringLiteral(".png"));
        if (!QFileInfo::exists(imgPath))
            imgPath.clear();
    }
    if (imgPath.isEmpty()) {
        imgPath = QDir::current().absoluteFilePath(
            QStringLiteral("img/widgets/%1.png").arg(key));
    }
    pix.load(imgPath);

    cache.insert(key, pix);
    return pix;
}

// ---------------------------------------------------------------------------
// 构造
// ---------------------------------------------------------------------------
CanvasItem::CanvasItem(const WidgetInstance &inst,
                       const WidgetMeta     &meta,
                       QGraphicsItem        *parent)
    : QGraphicsObject(parent)
    , m_inst(inst)
    , m_meta(meta)
{
    setPos(inst.x, inst.y);
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
    setAcceptHoverEvents(true);
    setZValue(inst.zOrder);
    m_pixmap = loadWidgetPixmap(m_meta.luaFilePath);
}

// ---------------------------------------------------------------------------
// applyGeometry — 由撤销/重做直接设置，不入栈
// ---------------------------------------------------------------------------
void CanvasItem::applyGeometry(const QRectF &r)
{
    prepareGeometryChange();
    m_inst.x      = qRound(r.x());
    m_inst.y      = qRound(r.y());
    m_inst.width  = qRound(r.width());
    m_inst.height = qRound(r.height());
    setPos(r.topLeft());
    update();
}

// ---------------------------------------------------------------------------
// 属性面板：直接修改实例数据
// ---------------------------------------------------------------------------
void CanvasItem::setInstanceName(const QString &name)
{
    m_inst.name = name;
    update();
}

void CanvasItem::setInstanceProperty(const QString &key, const QVariant &value)
{
    m_inst.properties.insert(key, value);
    update();
}

// ---------------------------------------------------------------------------
// 几何辅助
// ---------------------------------------------------------------------------
QRectF CanvasItem::boundingRect() const
{
    const qreal m = kHandle / 2.0 + 2.0;
    return QRectF(-m, -m, m_inst.width + m * 2, m_inst.height + m * 2);
}

QPainterPath CanvasItem::shape() const
{
    QPainterPath p;
    p.addRect(QRectF(0, 0, m_inst.width, m_inst.height));
    return p;
}

// 8 个 handle 的局部坐标矩形
QRectF CanvasItem::handleRect(HandlePos h) const
{
    const qreal w  = m_inst.width;
    const qreal ht = m_inst.height;
    const qreal hs = kHandle;
    const qreal ho = hs / 2.0;

    switch (h) {
    case HandlePos::TL: return QRectF(-ho,       -ho,       hs, hs);
    case HandlePos::T:  return QRectF(w/2-ho,    -ho,       hs, hs);
    case HandlePos::TR: return QRectF(w-ho,       -ho,       hs, hs);
    case HandlePos::L:  return QRectF(-ho,        ht/2-ho,  hs, hs);
    case HandlePos::R:  return QRectF(w-ho,       ht/2-ho,  hs, hs);
    case HandlePos::BL: return QRectF(-ho,        ht-ho,    hs, hs);
    case HandlePos::B:  return QRectF(w/2-ho,    ht-ho,    hs, hs);
    case HandlePos::BR: return QRectF(w-ho,       ht-ho,    hs, hs);
    default: return {};
    }
}

HandlePos CanvasItem::hitHandle(const QPointF &lp) const
{
    if (!isSingleSelected()) return HandlePos::None;
    for (auto h : {HandlePos::TL, HandlePos::T,  HandlePos::TR,
                   HandlePos::L,               HandlePos::R,
                   HandlePos::BL, HandlePos::B,  HandlePos::BR}) {
        if (handleRect(h).adjusted(-2,-2,2,2).contains(lp))
            return h;
    }
    return HandlePos::None;
}

bool CanvasItem::isSingleSelected() const
{
    if (!isSelected() || !scene()) return false;
    const auto sel = scene()->selectedItems();
    return sel.count() == 1 && sel.first() == this;
}

Qt::CursorShape CanvasItem::cursorForHandle(HandlePos h) const
{
    switch (h) {
    case HandlePos::TL: case HandlePos::BR: return Qt::SizeFDiagCursor;
    case HandlePos::TR: case HandlePos::BL: return Qt::SizeBDiagCursor;
    case HandlePos::T:  case HandlePos::B:  return Qt::SizeVerCursor;
    case HandlePos::L:  case HandlePos::R:  return Qt::SizeHorCursor;
    default:                                return Qt::SizeAllCursor;
    }
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------
void CanvasItem::paint(QPainter *p,
                       const QStyleOptionGraphicsItem * /*opt*/,
                       QWidget * /*w*/)
{
    const QRectF body(0, 0, m_inst.width, m_inst.height);

    // --- 透明矩形框 + widget 预览图 ---
    p->save();
    p->setRenderHint(QPainter::SmoothPixmapTransform, true);
    p->setRenderHint(QPainter::Antialiasing,           true);

    const QPixmap &pix = m_pixmap;
    if (!pix.isNull()) {
        p->drawPixmap(body, pix, QRectF(pix.rect()));
    } else {
        // 找不到图片时画一个浅色虚线框作为占位
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor(150, 150, 150, 180), 1, Qt::DashLine));
        p->drawRect(body.adjusted(0.5, 0.5, -0.5, -0.5));
        if (!m_meta.name.isEmpty()) {
            p->setPen(QColor(180, 180, 180));
            p->drawText(body, Qt::AlignCenter, m_meta.name);
        }
    }
    p->restore();

    // --- 选中状态叠加（在 widget 外观之上）---
    if (isSelected()) {
        p->save();
        p->setRenderHint(QPainter::Antialiasing, false);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(QColor("#4db8ff"), 1.5, Qt::DashLine));
        p->drawRect(body.adjusted(-1, -1, 1, 1));

        // 单选：绘制 8 个控制点
        if (isSingleSelected()) {
            p->setBrush(Qt::white);
            p->setPen(QPen(QColor("#007acc"), 1.0));
            for (auto h : {HandlePos::TL, HandlePos::T,  HandlePos::TR,
                           HandlePos::L,               HandlePos::R,
                           HandlePos::BL, HandlePos::B,  HandlePos::BR}) {
                p->drawRect(handleRect(h));
            }
        }
        p->restore();
    }
}

// ---------------------------------------------------------------------------
// 鼠标事件
// ---------------------------------------------------------------------------
void CanvasItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_activeHandle = hitHandle(event->pos());
        m_resizeStartRect = QRectF(pos(), QSizeF(m_inst.width, m_inst.height));

        if (m_activeHandle != HandlePos::None) {
            m_inResize = true;
            m_pressScenePos = event->scenePos();
            // 阻止场景把这次按下当作移动起点
            event->accept();
            return;
        }
    }
    m_inResize = false;
    QGraphicsObject::mousePressEvent(event);
}

void CanvasItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_inResize) {
        const QPointF d   = event->scenePos() - m_pressScenePos;
        const QRectF  orig = m_resizeStartRect;

        qreal x = orig.x(), y = orig.y(), w = orig.width(), h = orig.height();

        switch (m_activeHandle) {
        case HandlePos::TL: x += d.x(); y += d.y(); w -= d.x(); h -= d.y(); break;
        case HandlePos::T:               y += d.y();              h -= d.y(); break;
        case HandlePos::TR:              y += d.y(); w += d.x();  h -= d.y(); break;
        case HandlePos::L:  x += d.x();             w -= d.x();              break;
        case HandlePos::R:                           w += d.x();              break;
        case HandlePos::BL: x += d.x();             w -= d.x(); h += d.y();  break;
        case HandlePos::B:                                        h += d.y();  break;
        case HandlePos::BR:                          w += d.x(); h += d.y();  break;
        default: break;
        }

        w = qMax(w, kMin);
        h = qMax(h, kMin);

        prepareGeometryChange();
        setPos(x, y);
        m_inst.x      = qRound(x);
        m_inst.y      = qRound(y);
        m_inst.width  = qRound(w);
        m_inst.height = qRound(h);
        update();
        event->accept();
        return;
    }
    QGraphicsObject::mouseMoveEvent(event);
}

void CanvasItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_inResize) {
        m_inResize = false;
        const QRectF after(pos(), QSizeF(m_inst.width, m_inst.height));
        if (after != m_resizeStartRect)
            emit resizeCommitted(m_inst.instanceId, m_resizeStartRect, after);
        m_activeHandle = HandlePos::None;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
// Hover：更改光标
// ---------------------------------------------------------------------------
void CanvasItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    const HandlePos h = hitHandle(event->pos());
    if (h != HandlePos::None)
        setCursor(cursorForHandle(h));
    else
        setCursor(Qt::SizeAllCursor);
    QGraphicsObject::hoverMoveEvent(event);
}

void CanvasItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    unsetCursor();
    QGraphicsObject::hoverLeaveEvent(event);
}

// ---------------------------------------------------------------------------
// itemChange：保持 m_inst 与 pos() 同步
// ---------------------------------------------------------------------------
QVariant CanvasItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged) {
        m_inst.x = qRound(pos().x());
        m_inst.y = qRound(pos().y());
        if (scene()) scene()->update();
    }
    if (m_pixmap.isNull()) {
		//todo 如果没有预览图，尝试使用lvgl去绘制图片
    }
    return QGraphicsObject::itemChange(change, value);
}
