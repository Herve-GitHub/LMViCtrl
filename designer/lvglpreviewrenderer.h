#pragma once

#include <QImage>
#include <QString>

#include "widgemeta.h"

class LvglPreviewRenderer
{
public:
    static QImage renderWidget(const WidgetMeta &meta,
                               const WidgetInstance &inst,
                               int width,
                               int height,
                               QString *error = nullptr);
};
