#pragma once
#include <QColor>
#include <QRectF>
#include "WidgetMeta.h"

class QPainter;

// 根据 WidgetMeta + WidgetInstance 的属性，在 QPainter 上绘制 LVGL 风格预览
// 属性优先级：WidgetInstance.properties > WidgetMeta.properties[i].defaultValue
class WidgetPainter
{
public:
    static void draw(QPainter             *p,
                     const QRectF         &rect,
                     const WidgetMeta     &meta,
                     const WidgetInstance &inst);

    // 工具：获取属性有效值
    static QVariant effectiveValue(const WidgetMeta     &meta,
                                   const WidgetInstance &inst,
                                   const QString        &propName);

    // 工具：解析颜色字符串（如 "#007acc"）
    static QColor   parseColor(const QVariant &v,
                                const QColor   &fallback = QColor("#607d8b"));

private:
    // 各类型绘制（按 meta.id 分发）
    static void drawButton    (QPainter *, const QRectF &, const WidgetMeta &, const WidgetInstance &);
    static void drawTrendChart(QPainter *, const QRectF &, const WidgetMeta &, const WidgetInstance &);
    static void drawValve     (QPainter *, const QRectF &, const WidgetMeta &, const WidgetInstance &);
    static void drawGeneric   (QPainter *, const QRectF &, const WidgetMeta &, const WidgetInstance &);
};
