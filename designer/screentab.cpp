#include "screentab.h"
#include "canvasscene.h"
#include "canvasview.h"

#include <QColor>
#include <QVBoxLayout>

ScreenTab::ScreenTab(const ScreenData &screen,
                     const QSize &canvasSize,
                     const QList<WidgetMeta> &metas,
                     QWidget *parent)
    : QWidget(parent)
    , m_screenId(screen.id)
{
    m_scene = new CanvasScene(this);
    m_scene->setCanvasSize(canvasSize.width(), canvasSize.height());
    m_scene->setCanvasBackgroundColor(QColor(screen.bgColor));
    m_scene->setWidgetMetas(metas);
    m_scene->loadInstances(screen.widgets);

    m_view = new CanvasView(this);
    m_view->setScene(m_scene);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_view);
}

void ScreenTab::syncToScreen(ScreenData &screen) const
{
    screen.widgets = m_scene->allInstances();
    screen.bgColor = m_scene->canvasBackgroundColor().name(QColor::HexRgb);
}
