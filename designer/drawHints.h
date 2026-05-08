#pragma once
#include <QPainter>
#include "widgetmeta.h"
class drawHints
{
public:
    QColor toColorVal(const QVariant& v, const QColor& fallback);
    QVariant propValue(const WidgetInstance& inst,
        const WidgetMeta& meta,
        const QString& name);
    bool paintWithDrawHints(QPainter* p,
        const QRectF& body,
        const WidgetInstance& inst,
        const WidgetMeta& meta);

     QString  getStrHint(const QVariantMap& h, const char* key);
     double   getNumProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key, double def);
     QString  getStrProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key);
     QColor   getColorProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key, const QColor& def);

private:
    bool paintCanvasHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintListHints    (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintMenuHints    (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintTabviewHints (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintTileviewHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintMsgboxHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintTableHints   (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintChartHints   (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintBarHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintArcHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintLedHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintSliderHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintTextareaHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintDropdownHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintRollerHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintSwitchHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintCheckboxHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
    bool paintCustomHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta);
};

