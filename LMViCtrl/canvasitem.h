#pragma once
#include <QGraphicsObject>
#include <QColor>
#include <QFont>
#include <QImage>
#include "widgemeta.h"
#include "drawHints.h"
// 8 个缩放控制点方向
enum class HandlePos { None, TL, T, TR, L, R, BL, B, BR };

// 画布上的单个 Widget 实例图形元素
class CanvasItem : public QGraphicsObject
{
    Q_OBJECT

public:
    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    explicit CanvasItem(const WidgetInstance &inst,
                        const WidgetMeta     &meta,
                        QGraphicsItem        *parent = nullptr);

    // ----- 工程字体（全局，应用到所有画布元素的文字绘制）-----
    // fontFile 既可为绝对路径，也可为相对工程目录的相对路径（如 "fonts/MyFont.ttf"）。
    // 为空时将清除工程字体，回退到默认字体。
    static void  setProjectFont(const QString &fontFile,
                                int            defaultSize,
                                const QString &projectDir = QString());
    static const QFont &projectFont();
    static QString projectFontFilePath();
    static int     projectFontSize();

    const WidgetInstance &instance() const { return m_inst; }

    // 由撤销/重做命令直接设置几何（不产生新命令）
    void applyGeometry(const QRectF &sceneRect);

    // 属性面板：直接修改实例数据（不入栈）
    void setInstanceName(const QString &name);
    void setInstanceProperty(const QString &key, const QVariant &value);
    void setEventBindings(const QList<WidgetEventBinding> &bindings);

    QRectF       boundingRect() const override;
    QPainterPath shape()        const override;
    void         paint(QPainter *,
                       const QStyleOptionGraphicsItem *,
                       QWidget *) override;

    static constexpr qreal kHandle = 8.0;
    static constexpr qreal kMin    = 20.0;

signals:
    // 缩放完成后通知场景（用于 undo）
    void resizeCommitted(const QString &instanceId,
                         const QRectF  &before,
                         const QRectF  &after);

    // 拖动 / 缩放过程中的实时几何变化通知（供属性面板实时回填）
    void geometryChanged(const QString &instanceId);

protected:
    void mousePressEvent  (QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent   (QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent   (QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent  (QGraphicsSceneHoverEvent *event) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    HandlePos       hitHandle(const QPointF &localPos) const;
    QRectF          handleRect(HandlePos h) const;
    bool            isSingleSelected() const;
    Qt::CursorShape cursorForHandle(HandlePos h) const;

    WidgetInstance m_inst;
    WidgetMeta     m_meta;
    QPixmap        m_pixmap;
    QImage         m_lvglPreview;
    QString        m_lvglPreviewKey;

    HandlePos m_activeHandle   = HandlePos::None;
    QPointF   m_pressScenePos;
    QRectF    m_resizeStartRect;   // resize 开始时的场景矩形
    bool      m_inResize = false;
    drawHints m_drawHints;
};
