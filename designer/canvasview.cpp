#include "canvasview.h"

#include <QKeyEvent>
#include <QScrollBar>
#include <QWheelEvent>

CanvasView::CanvasView(QWidget *parent)
    : QGraphicsView(parent)
{
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::RubberBandDrag);   // 框选多选
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setAcceptDrops(true);

    // 允许从工具箱拖放
    viewport()->setAcceptDrops(true);
}

void CanvasView::resetZoom()
{
    resetTransform();
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const double factor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
        scale(factor, factor);
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+0 重置缩放
    if (event->modifiers() & Qt::ControlModifier && event->key() == Qt::Key_0) {
        resetZoom();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}
