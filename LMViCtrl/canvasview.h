#pragma once
#include <QGraphicsView>

// 画布视图：支持滚轮缩放、拖放穿透
class CanvasView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit CanvasView(QWidget *parent = nullptr);

    // 重置缩放比例为 1:1
    void resetZoom();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};
