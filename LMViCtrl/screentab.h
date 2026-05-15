#pragma once
#include <QWidget>
#include "widgemeta.h"

class CanvasScene;
class CanvasView;

// 单个图页的容器：持有独立的 CanvasScene + CanvasView
class ScreenTab : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenTab(const ScreenData &screen,
                       const QSize &canvasSize,
                       const QList<WidgetMeta> &metas,
                       QWidget *parent = nullptr);

    const QString &screenId() const { return m_screenId; }

    CanvasScene *scene() const { return m_scene; }
    CanvasView  *view()  const { return m_view;  }

    // 将当前画布内容同步回 ScreenData
    void syncToScreen(ScreenData &screen) const;

private:
    QString      m_screenId;
    CanvasScene *m_scene = nullptr;
    CanvasView  *m_view  = nullptr;
};
