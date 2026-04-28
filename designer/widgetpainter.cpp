#include "widgetpainter.h"

#include <QPainter>
#include <QPen>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QPolygonF>

// 避免 MSVC 下 M_PI 未定义
static constexpr double kPi = 3.14159265358979323846;

// ===========================================================================
// 工具函数
// ===========================================================================

QVariant WidgetPainter::effectiveValue(const WidgetMeta     &meta,
                                       const WidgetInstance &inst,
                                       const QString        &propName)
{
    if (inst.properties.contains(propName))
        return inst.properties.value(propName);
    for (const PropertyMeta &pm : meta.properties)
        if (pm.name == propName)
            return pm.defaultValue;
    return {};
}

QColor WidgetPainter::parseColor(const QVariant &v, const QColor &fallback)
{
    if (!v.isValid()) return fallback;
    const QColor c(v.toString());
    return c.isValid() ? c : fallback;
}

// ===========================================================================
// 入口分发
// ===========================================================================

void WidgetPainter::draw(QPainter             *p,
                         const QRectF         &rect,
                         const WidgetMeta     &meta,
                         const WidgetInstance &inst)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setClipRect(rect);

    if      (meta.id == QLatin1String("custom_button")) drawButton    (p, rect, meta, inst);
    else if (meta.id == QLatin1String("trend_chart"))   drawTrendChart(p, rect, meta, inst);
    else if (meta.id == QLatin1String("valve"))         drawValve     (p, rect, meta, inst);
    else                                                drawGeneric   (p, rect, meta, inst);

    p->restore();
}

// ===========================================================================
// Button（custom_button）
//   属性：label / bg_color / color / font_size / enabled
// ===========================================================================

void WidgetPainter::drawButton(QPainter             *p,
                                const QRectF         &rect,
                                const WidgetMeta     &meta,
                                const WidgetInstance &inst)
{
    const QColor bgColor   = parseColor(effectiveValue(meta, inst, "bg_color"), QColor("#007acc"));
    const QColor textColor = parseColor(effectiveValue(meta, inst, "color"),    QColor("#ffffff"));
    const QString label    = effectiveValue(meta, inst, "label").toString();
    const bool    enabled  = effectiveValue(meta, inst, "enabled").toBool();

    const QColor drawBg   = enabled ? bgColor   : bgColor.darker(170);
    const QColor drawText = enabled ? textColor : textColor.darker(150);
    const QRectF body     = rect.adjusted(1, 1, -1, -1);
    const qreal  r        = qMin(body.height() * 0.25, 7.0);

    // 阴影
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(QColor(0, 0, 0, 55), 3));
    p->drawRoundedRect(body.adjusted(1, 2, 1, 2), r, r);

    // 主体
    p->setBrush(drawBg);
    p->setPen(QPen(drawBg.lighter(150), 1));
    p->drawRoundedRect(body, r, r);

    // 上半部高光
    QLinearGradient gloss(body.topLeft(), QPointF(body.left(), body.top() + body.height() * 0.5));
    gloss.setColorAt(0, QColor(255, 255, 255, 60));
    gloss.setColorAt(1, QColor(255, 255, 255, 0));
    p->setBrush(gloss);
    p->setPen(Qt::NoPen);
    p->drawRoundedRect(body, r, r);

    // 禁用遮罩
    if (!enabled) {
        p->setBrush(QColor(0, 0, 0, 50));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(body, r, r);
    }

    // 文字
    p->setPen(drawText);
    QFont f = p->font();
    const int fs = qBound(8, qRound(body.height() * 0.40), 18);
    f.setPixelSize(fs);
    f.setBold(true);
    p->setFont(f);
    p->drawText(body, Qt::AlignCenter, label.isEmpty() ? QStringLiteral("Button") : label);
}

// ===========================================================================
// Trend Chart（trend_chart）
//   属性：range_min / range_max / point_count / auto_update
// ===========================================================================

void WidgetPainter::drawTrendChart(QPainter             *p,
                                    const QRectF         &rect,
                                    const WidgetMeta     &meta,
                                    const WidgetInstance &inst)
{
    const double rangeMin = effectiveValue(meta, inst, "range_min").toDouble();
    const double rangeMax = effectiveValue(meta, inst, "range_max").toDouble();
    const bool   autoUpd  = effectiveValue(meta, inst, "auto_update").toBool();

    // 背景
    p->setBrush(QColor("#0d1520"));
    p->setPen(QPen(QColor("#1e3a5f"), 1));
    p->drawRect(rect);

    const qreal barH = qMin(16.0, rect.height() * 0.22);
    const QRectF plotArea = rect.adjusted(1, barH, -1, -1);

    // 网格线
    p->setPen(QPen(QColor("#192840"), 1, Qt::DotLine));
    for (int i = 1; i < 4; ++i) {
        const qreal y = plotArea.top()  + plotArea.height() * i / 4.0;
        const qreal x = plotArea.left() + plotArea.width()  * i / 4.0;
        p->drawLine(QPointF(plotArea.left(),  y), QPointF(plotArea.right(), y));
        p->drawLine(QPointF(x, plotArea.top()), QPointF(x, plotArea.bottom()));
    }

    // 模拟趋势曲线
    const int pts = qMax(12, qRound(plotArea.width() / 5.0));
    QPolygonF poly;
    for (int i = 0; i < pts; ++i) {
        const double t = double(i) / (pts - 1);
        const double x = plotArea.left() + t * plotArea.width();
        const double y = plotArea.center().y()
                       - plotArea.height() * 0.30 * qSin(t * 2.0 * kPi * 1.35 + 0.4)
                       + plotArea.height() * 0.10 * qSin(t * 2.0 * kPi * 4.1);
        poly << QPointF(x, y);
    }

    // 曲线面积填充
    QPolygonF fill = poly;
    fill.append(QPointF(plotArea.right(),  plotArea.bottom()));
    fill.append(QPointF(plotArea.left(),   plotArea.bottom()));
    QLinearGradient areaGrad(plotArea.topLeft(), plotArea.bottomLeft());
    areaGrad.setColorAt(0, QColor(33, 150, 243, 90));
    areaGrad.setColorAt(1, QColor(33, 150, 243, 0));
    p->setBrush(areaGrad);
    p->setPen(Qt::NoPen);
    p->drawPolygon(fill);

    // 曲线
    p->setPen(QPen(QColor("#2196F3"), 1.5));
    p->setBrush(Qt::NoBrush);
    p->drawPolyline(poly);

    // 标题栏
    p->fillRect(QRectF(rect.left(), rect.top(), rect.width(), barH), QColor("#0a1020"));
    p->setPen(QColor("#4db8ff"));
    QFont tf; tf.setPixelSize(qBound(8, qRound(barH * 0.60), 11)); tf.setBold(true);
    p->setFont(tf);
    p->drawText(QRectF(rect.left() + 4, rect.top(), rect.width() - 8, barH),
                Qt::AlignLeft | Qt::AlignVCenter,
                QStringLiteral("Trend Chart"));

    // 自动更新指示
    if (autoUpd) {
        p->setPen(QColor("#00cc44"));
        p->drawText(QRectF(rect.left(), rect.top(), rect.width() - 4, barH),
                    Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("●"));
    }

    // Y 轴范围标注
    p->setPen(QColor("#446688"));
    QFont rf; rf.setPixelSize(qBound(7, qRound(plotArea.height() * 0.10), 9));
    p->setFont(rf);
    p->drawText(QRectF(plotArea.left() + 2, plotArea.top(),     plotArea.width(), 10),
                Qt::AlignLeft | Qt::AlignTop,    QString::number(rangeMax, 'g', 4));
    p->drawText(QRectF(plotArea.left() + 2, plotArea.bottom() - 10, plotArea.width(), 10),
                Qt::AlignLeft | Qt::AlignBottom, QString::number(rangeMin, 'g', 4));
}

// ===========================================================================
// Valve（valve）
//   属性：angle / open_angle / close_angle / handle_color / size
// ===========================================================================

void WidgetPainter::drawValve(QPainter             *p,
                               const QRectF         &rect,
                               const WidgetMeta     &meta,
                               const WidgetInstance &inst)
{
    const double angle       = effectiveValue(meta, inst, "angle").toDouble();
    const double openAngle   = effectiveValue(meta, inst, "open_angle").toDouble();
    const QColor handleColor = parseColor(effectiveValue(meta, inst, "handle_color"), QColor("#FF5722"));

    // 取正方形区域居中
    const qreal s  = qMin(rect.width(), rect.height()) - 4.0;
    const QRectF cr(rect.left() + (rect.width()  - s) * 0.5,
                    rect.top()  + (rect.height() - s) * 0.5,
                    s, s);
    const QRectF body = cr.adjusted(2, 2, -2, -2);

    // 外圈渐变
    QRadialGradient rg(body.center() - QPointF(body.width() * 0.1, body.height() * 0.15),
                       body.width() * 0.55);
    rg.setColorAt(0, QColor("#e8e8e8"));
    rg.setColorAt(1, QColor("#b0b0b0"));
    p->setBrush(rg);
    p->setPen(QPen(QColor("#606060"), 2.0));
    p->drawEllipse(body);

    // 刻度线（每 45°）
    p->setPen(QPen(QColor("#909090"), 1));
    for (int deg = 0; deg < 360; deg += 45) {
        const double rad  = deg * kPi / 180.0;
        const double cos_ = qCos(rad);
        const double sin_ = qSin(rad);
        const double cx   = body.center().x();
        const double cy   = body.center().y();
        const double r1   = s * 0.42;
        const double r2   = s * 0.48;
        p->drawLine(QPointF(cx + r1 * cos_, cy + r1 * sin_),
                    QPointF(cx + r2 * cos_, cy + r2 * sin_));
    }

    // 把手（旋转矩形）
    {
        p->save();
        p->translate(body.center());
        p->rotate(angle);
        const qreal hw = body.width() * 0.15;
        const qreal hh = body.height() * 0.44;
        const QRectF hr(-hw * 0.5, -hh * 0.5, hw, hh);
        // 把手阴影
        p->setBrush(handleColor.darker(180));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(hr.adjusted(1, 2, 1, 2), 3, 3);
        // 把手本体
        p->setBrush(handleColor);
        p->setPen(QPen(handleColor.darker(140), 1));
        p->drawRoundedRect(hr, 3, 3);
        // 高光
        p->setBrush(QColor(255, 255, 255, 60));
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(hr.adjusted(1, 1, -1, hr.height() * 0.5 - 1), 3, 3);
        p->restore();
    }

    // 中心轴
    const qreal hs = s * 0.09;
    QRadialGradient hubGrad(body.center() - QPointF(hs * 0.3, hs * 0.3), hs);
    hubGrad.setColorAt(0, QColor("#aaaaaa"));
    hubGrad.setColorAt(1, QColor("#444444"));
    p->setBrush(hubGrad);
    p->setPen(QPen(QColor("#303030"), 1.5));
    p->drawEllipse(body.center(), hs, hs);

    // 开/关状态指示
    const bool isOpen = qAbs(angle - openAngle) < 5.0;
    const QColor stateColor = isOpen ? QColor("#00cc44") : QColor("#cc4400");
    p->setPen(Qt::NoPen);
    p->setBrush(stateColor);
    const qreal si = s * 0.06;
    p->drawEllipse(QPointF(body.right() - si - 2, body.top() + si + 2), si, si);

    // 角度文字
    QFont af; af.setPixelSize(qBound(7, qRound(s * 0.12), 12));
    p->setFont(af);
    p->setPen(QColor("#505050"));
    p->drawText(QRectF(body.left(), body.bottom() - s * 0.22, body.width(), s * 0.22),
                Qt::AlignHCenter | Qt::AlignVCenter,
                QString("%1°").arg(qRound(angle)));
}

// ===========================================================================
// Generic（通用回退）
//   使用 meta.id 哈希色 + 展示 name / description
// ===========================================================================

void WidgetPainter::drawGeneric(QPainter             *p,
                                 const QRectF         &rect,
                                 const WidgetMeta     &meta,
                                 const WidgetInstance &inst)
{
    // 尝试读取 bg_color 属性，无则用 id 哈希色
    const QVariant bgVal = effectiveValue(meta, inst, "bg_color");
    QColor accent;
    if (bgVal.isValid() && QColor(bgVal.toString()).isValid())
        accent = QColor(bgVal.toString());
    else {
        const uint h = qHash(meta.id);
        accent = QColor(80  + int((h & 0xFF)         % 100),
                        100 + int(((h >> 8)  & 0xFF) % 100),
                        130 + int(((h >> 16) & 0xFF) % 100));
    }

    // 主体
    p->setBrush(accent.darker(240));
    p->setPen(QPen(accent.darker(130), 1.5));
    p->drawRect(rect);

    // 标题栏
    const qreal barH = qMin(22.0, rect.height() * 0.30);
    QLinearGradient hg(rect.topLeft(), QPointF(rect.left(), rect.top() + barH));
    hg.setColorAt(0, accent.darker(130));
    hg.setColorAt(1, accent.darker(170));
    p->fillRect(QRectF(rect.left(), rect.top(), rect.width(), barH), hg);

    // 左侧色块标记
    p->setBrush(accent);
    p->setPen(Qt::NoPen);
    p->drawRect(QRectF(rect.left(), rect.top(), 4, barH));

    // Widget 名称
    QFont nf; nf.setPixelSize(qBound(8, qRound(barH * 0.56), 13)); nf.setBold(true);
    p->setFont(nf);
    p->setPen(Qt::white);
    p->drawText(QRectF(rect.left() + 8, rect.top(), rect.width() - 12, barH),
                Qt::AlignLeft | Qt::AlignVCenter, meta.name);

    // 描述（有足够空间时显示）
    const qreal bodyH = rect.height() - barH;
    if (bodyH > 18) {
        QFont df; df.setPixelSize(qBound(7, qRound(barH * 0.44), 11));
        p->setFont(df);
        p->setPen(QColor("#9999aa"));
        p->drawText(QRectF(rect.left() + 6, rect.top() + barH + 4,
                           rect.width() - 12, bodyH - 8),
                    Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                    meta.description);
    }
}
