#include "drawHints.h"
#include <qpainterpath.h>
#include "canvasitem.h"
// ---------------------------------------------------------------------------
// 属性取值工具：优先从实例，退化到 meta default
// ---------------------------------------------------------------------------
QVariant drawHints::propValue(const WidgetInstance& inst,
    const WidgetMeta& meta,
    const QString& name)
{
    if (inst.properties.contains(name))
        return inst.properties.value(name);
    for (const PropertyMeta& pm : meta.properties)
        if (pm.name == name) return pm.defaultValue;
    return {};
}

QColor drawHints::toColorVal(const QVariant& v, const QColor& fallback)
{
    if (!v.isValid()) return fallback;
    if (v.canConvert<QColor>()) {
        QColor c = v.value<QColor>();
        if (c.isValid()) return c;
    }
    const QString s = v.toString();
    if (!s.isEmpty()) {
        QColor c(s);
        if (c.isValid()) return c;
    }
    return fallback;
}
QString drawHints::getStrHint(const QVariantMap& h, const char* key)
{
    return h.value(QLatin1String(key)).toString();
}

double drawHints::getNumProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key, double def)
{
    const QString src = getStrHint(h, key);
    if (src.isEmpty()) return def;
    const QVariant v = propValue(inst, meta, src);
    return v.isValid() ? v.toDouble() : def;
}

QString drawHints::getStrProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key)
{
    const QString src = getStrHint(h, key);
    if (src.isEmpty()) return {};
    return propValue(inst, meta, src).toString();
}

QColor drawHints::getColorProp(const QVariantMap& h, const WidgetInstance& inst, const WidgetMeta& meta, const char* key, const QColor& def)
{
    const QString src = getStrHint(h, key);
    if (src.isEmpty()) return def;
    return toColorVal(propValue(inst, meta, src), def);
}

bool drawHints::paintWithDrawHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta)
{
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString shape = h.value(QStringLiteral("shape"), QStringLiteral("rect")).toString();

    // ------- 特殊形状：canvas（自定义绘制区域：填充 + 网格 + 对角线 + 边框） -------
    if (shape == QLatin1String("canvas")) {
		return paintCanvasHints(p, body, inst, meta);
    }

    // ------- 特殊形状：list（垂直条目列表，支持分组标题与选中） -------
    if (shape == QLatin1String("list")) {
        return paintListHints(p, body, inst, meta);
    }

    // ------- 特殊形状：menu（左侧栏 + 详情区） -------
    if (shape == QLatin1String("menu")) {
        return paintMenuHints(p, body, inst, meta);
    }

    // ------- 特殊形状：tabview（标签栏 + 内容区 + 当前指示条） -------
    if (shape == QLatin1String("tabview")) {
        return paintTabviewHints(p, body, inst, meta);
    }

    // ------- 特殊形状：tileview（网格分页 + 高亮当前格 + 页指示器） -------
    if (shape == QLatin1String("tileview")) {
        return paintTileviewHints(p, body, inst, meta);
    }

    // ------- 特殊形状：msgbox（标题栏 + 正文 + 按钮组） -------
    if (shape == QLatin1String("msgbox")) {
        return paintMsgboxHints(p, body, inst, meta);
    }

    // ------- 特殊形状：table（多行多列网格 + 表头 + 斑马纹） -------
    if (shape == QLatin1String("table")) {
        return paintTableHints(p, body, inst, meta);
    }

    // ------- 特殊形状：chart（折线/柱状/散点 + 网格 + 多序列） -------
    if (shape == QLatin1String("chart")) {
        return paintChartHints(p, body, inst, meta);
    }

    // ------- 特殊形状：bar（轨道 + 指示条，支持水平/垂直/反向/对称/区间） -------
    if (shape == QLatin1String("bar")) {
       return paintBarHints(p, body, inst, meta);
    }

    // ------- 特殊形状：arc（背景弧 + 指示弧 + 可选拖动球） -------
    if (shape == QLatin1String("arc")) {
        return paintArcHints(p, body, inst, meta);
    }

    // ------- 特殊形状：led（圆形/方形指示灯，带发光效果） -------
    if (shape == QLatin1String("led")) {
        return paintLedHints(p, body, inst, meta);
    }

    // ------- 特殊形状：slider（轨道 + 指示条 + 滑块） -------
    if (shape == QLatin1String("slider")) {
        return paintSliderHints(p, body, inst, meta);
    }

    // ------- 特殊形状：textarea（边框 + 文本/占位符） -------
    if (shape == QLatin1String("textarea")) {
        return paintTextareaHints(p, body, inst, meta);
    }

    // ------- 特殊形状：dropdown（边框 + 当前选中文本 + 右侧箭头） -------
    if (shape == QLatin1String("dropdown")) {
        return paintDropdownHints(p, body, inst, meta);
    }

    // ------- 特殊形状：roller（多行选项 + 选中行高亮） -------
    if (shape == QLatin1String("roller")) {
        return paintRollerHints(p, body, inst, meta);
    }

    // ------- 特殊形状：switch（轨道 + 滑块） -------
    if (shape == QLatin1String("switch")) {
        return paintSwitchHints(p, body, inst, meta);
    }

    // ------- 特殊形状：checkbox（小方框 + 勾 + 文本） -------
    if (shape == QLatin1String("checkbox")) {
        return paintCheckboxHints(p, body, inst, meta);
    }

    // ------- 默认/自定义控件：根据 draw_hints 逐步绘制（rect/ellipse/rounded_rect + 文本） -------
    return paintCustomHints(p, body, inst, meta);
}

bool drawHints::paintCanvasHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta)
{
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QColor fill = getColorProp(h, inst, meta, "fill_color_from", QColor("#ffffff"));
    const double opa = qBound(0.0, getNumProp(h, inst, meta, "fill_opa_from", 255), 255.0);
    const bool showGrid = propValue(inst, meta, getStrHint(h, "show_grid_from")).toBool();
    const double step = qMax(2.0, getNumProp(h, inst, meta, "grid_step_from", 16));
    const QColor gridC = getColorProp(h, inst, meta, "grid_color_from", QColor("#dde2e8"));
    const bool showDiag = propValue(inst, meta, getStrHint(h, "show_diagonals_from")).toBool();
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double rad = getNumProp(h, inst, meta, "radius_from", 0);
    const QString fmt = getStrProp(h, inst, meta, "buffer_format_from");
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, rad > 0);

    // 裁剪到（可能圆角的）画布区域
    QPainterPath clip;
    if (rad > 0)
        clip.addRoundedRect(body, rad, rad);
    else 
        clip.addRect(body);
    p->setClipPath(clip);

    // 填充背景
    QColor fillCol = fill;
    fillCol.setAlpha(int(opa));
    p->setPen(Qt::NoPen);
    p->setBrush(fillCol);
    p->drawRect(body);

    // 网格
    if (showGrid) {
        p->setPen(QPen(gridC, 1));
        for (double x = body.x() + step; x < body.right(); x += step) {
            p->drawLine(QPointF(x, body.top()), QPointF(x, body.bottom()));
        }
        for (double y = body.y() + step; y < body.bottom(); y += step) {
            p->drawLine(QPointF(body.left(), y), QPointF(body.right(), y));
        }
    }

    // 对角线
    if (showDiag) {
        QPen dp(gridC.darker(120), 1, Qt::DashLine);
        p->setPen(dp);
        p->drawLine(body.topLeft(), body.bottomRight());
        p->drawLine(body.topRight(), body.bottomLeft());
    }

    // 左下角小标签：色彩格式（仅在画布够大时显示）
    if (body.width() > 90 && body.height() > 30 && !fmt.isEmpty()) {
        QFont f = p->font(); f.setPointSizeF(qMax(7.0, qMin(body.height() * 0.10, 11.0)));
        p->setFont(f);
        p->setPen(QColor(0, 0, 0, 110));
        const QString txt = QStringLiteral("canvas · ") + fmt;
        QRectF tr(body.x() + 6, body.bottom() - 18, body.width() - 12, 16);
        p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, txt);
    }

    // 解除裁剪以画边框
    p->setClipping(false);

    // 边框
    if (bw > 0) {
        p->setPen(QPen(border, bw));
        p->setBrush(Qt::NoBrush);
        if (rad > 0) p->drawRoundedRect(body, rad, rad);
        else         p->drawRect(body);
    }

    if (!enabled) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 110));
        if (rad > 0) p->drawRoundedRect(body, rad, rad);
        else         p->drawRect(body);
    }
    p->restore();
    return true;
}

bool drawHints::paintListHints    (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const QString itemsStr = getStrProp(h, inst, meta, "items_from");
    const int sel = int(getNumProp(h, inst, meta, "selected_index_from", -1));
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor itemTx = getColorProp(h, inst, meta, "item_text_color_from", QColor("#1f2933"));
    const QColor itemBg = getColorProp(h, inst, meta, "item_bg_color_from", QColor("#f4f6f9"));
    const QColor selBg = getColorProp(h, inst, meta, "selected_color_from", QColor("#1976d2"));
    const QColor selTx = getColorProp(h, inst, meta, "selected_text_from", QColor("#ffffff"));
    const QColor secC = getColorProp(h, inst, meta, "section_color_from", QColor("#7a8290"));
    const double rowH = getNumProp(h, inst, meta, "row_height_from", 36);
    const double fs = getNumProp(h, inst, meta, "font_size_from", 14);
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    const QStringList lines = itemsStr.split('\n');
    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    QFont f = p->font(); f.setPointSizeF(fs); p->setFont(f);
    const QFontMetricsF fm(f);

    double y = body.y();
    int btnIndex = 0;
    for (const QString& raw : lines) {
        if (y >= body.bottom()) break;
        const double rh = qMin(rowH, body.bottom() - y);
        const QRectF rRect(body.x(), y, body.width(), rh);

        const bool isSection = raw.startsWith("[#]");
        const QString text = isSection ? raw.mid(3) : raw;

        if (isSection) {
            p->setPen(secC);
            p->drawText(rRect.adjusted(8, 0, -8, 0),
                Qt::AlignVCenter | Qt::AlignLeft, text);
        }
        else {
            const bool isSel = (btnIndex == sel);
            p->setPen(Qt::NoPen);
            p->setBrush(isSel ? selBg : itemBg);
            p->drawRect(rRect.adjusted(2, 1, -2, -1));
            p->setPen(isSel ? selTx : itemTx);
            p->drawText(rRect.adjusted(10, 0, -10, 0),
                Qt::AlignVCenter | Qt::AlignLeft,
                fm.elidedText(text, Qt::ElideRight, int(rRect.width() - 20)));
            ++btnIndex;
        }
        y += rh;
    }

    if (bw > 0) {
        p->setPen(QPen(border, bw));
        p->setBrush(Qt::NoBrush);
        p->drawRect(body);
    }
    if (!enabled) p->fillRect(body, QColor(255, 255, 255, 110));
    p->restore();
    return true;
}
bool drawHints::paintMenuHints    (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const QString itemsStr = getStrProp(h, inst, meta, "items_from");
    const int sel = int(getNumProp(h, inst, meta, "selected_index_from", 0));
    const double sideRatio = qBound(10.0, getNumProp(h, inst, meta, "sidebar_ratio_from", 35), 80.0) / 100.0;
    const QString header = getStrProp(h, inst, meta, "header_text_from");
    const QString detail = getStrProp(h, inst, meta, "detail_text_from");
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor sideBg = getColorProp(h, inst, meta, "sidebar_bg_from", QColor("#f4f6f9"));
    const QColor itemTx = getColorProp(h, inst, meta, "item_text_color_from", QColor("#1f2933"));
    const QColor selBg = getColorProp(h, inst, meta, "selected_color_from", QColor("#1976d2"));
    const QColor selTx = getColorProp(h, inst, meta, "selected_text_from", QColor("#ffffff"));
    const QColor hdrBg = getColorProp(h, inst, meta, "header_bg_from", QColor("#e8eef7"));
    const QColor hdrTx = getColorProp(h, inst, meta, "header_text_color_from", QColor("#1f2933"));
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double rh = getNumProp(h, inst, meta, "row_height_from", 32);
    const double fs = getNumProp(h, inst, meta, "font_size_from", 13);
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    const QStringList items = itemsStr.split('\n', Qt::SkipEmptyParts);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    const double sideW = body.width() * sideRatio;
    const QRectF sideRect(body.x(), body.y(), sideW, body.height());
    const QRectF detRect(body.x() + sideW, body.y(), body.width() - sideW, body.height());

    // 侧栏背景
    p->setBrush(sideBg);
    p->drawRect(sideRect);

    QFont f = p->font(); f.setPointSizeF(fs); p->setFont(f);
    const QFontMetricsF fm(f);
    double y = sideRect.y() + 4;
    for (int i = 0; i < items.size(); ++i) {
        if (y >= sideRect.bottom()) break;
        const QRectF r(sideRect.x() + 2, y, sideRect.width() - 4, qMin(rh, sideRect.bottom() - y));
        const bool isSel = (i == sel);
        if (isSel) {
            p->setPen(Qt::NoPen);
            p->setBrush(selBg);
            p->drawRect(r);
        }
        p->setPen(isSel ? selTx : itemTx);
        p->drawText(r.adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
            fm.elidedText(items.at(i), Qt::ElideRight, int(r.width() - 18)));
        y += rh;
    }

    // 详情区标题
    const double headerH = rh + 4;
    const QRectF hdrRect(detRect.x(), detRect.y(), detRect.width(), headerH);
    p->setPen(Qt::NoPen);
    p->setBrush(hdrBg);
    p->drawRect(hdrRect);
    QFont hf = f; hf.setPointSizeF(fs + 2); hf.setBold(true);
    p->setFont(hf);
    p->setPen(hdrTx);
    p->drawText(hdrRect.adjusted(12, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, header);

    // 详情区文字
    p->setFont(f);
    p->setPen(itemTx);
    const QRectF detBody = detRect.adjusted(12, headerH + 8, -12, -8);
    if (detBody.width() > 0 && detBody.height() > 0) {
        p->drawText(detBody, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, detail);
    }

    // 边框
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(border, 1));
    p->drawLine(QPointF(detRect.x(), body.y()), QPointF(detRect.x(), body.bottom()));
    p->drawRect(body);

    if (!enabled) p->fillRect(body, QColor(255, 255, 255, 110));
    p->restore();
    return true;
}
bool drawHints::paintTabviewHints (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const QStringList tabs = getStrProp(h, inst, meta, "tabs_from").split('\n', Qt::SkipEmptyParts);
    const int cur = qBound(0, int(getNumProp(h, inst, meta, "current_tab_from", 0)),
        qMax(0, tabs.size() - 1));
    const QString pos = getStrProp(h, inst, meta, "tab_position_from");
    const double tsize = getNumProp(h, inst, meta, "tab_size_from", 36);
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor tabBg = getColorProp(h, inst, meta, "tabbar_bg_from", QColor("#f4f6f9"));
    const QColor tabTx = getColorProp(h, inst, meta, "tab_text_color_from", QColor("#5b6470"));
    const QColor actTx = getColorProp(h, inst, meta, "active_text_color_from", QColor("#1976d2"));
    const QColor indC = getColorProp(h, inst, meta, "indicator_color_from", QColor("#1976d2"));
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double fs = getNumProp(h, inst, meta, "font_size_from", 13);
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    // 计算标签栏与内容区
    QRectF tabBar, content;
    const bool horiz = (pos == QLatin1String("top") || pos == QLatin1String("bottom") || pos.isEmpty());
    if (pos == QLatin1String("bottom")) {
        content = QRectF(body.x(), body.y(), body.width(), body.height() - tsize);
        tabBar = QRectF(body.x(), body.bottom() - tsize, body.width(), tsize);
    }
    else if (pos == QLatin1String("left")) {
        tabBar = QRectF(body.x(), body.y(), tsize, body.height());
        content = QRectF(body.x() + tsize, body.y(), body.width() - tsize, body.height());
    }
    else if (pos == QLatin1String("right")) {
        content = QRectF(body.x(), body.y(), body.width() - tsize, body.height());
        tabBar = QRectF(body.right() - tsize, body.y(), tsize, body.height());
    }
    else { // top
        tabBar = QRectF(body.x(), body.y(), body.width(), tsize);
        content = QRectF(body.x(), body.y() + tsize, body.width(), body.height() - tsize);
    }

    // 标签栏背景
    p->setBrush(tabBg);
    p->drawRect(tabBar);

    QFont f = p->font(); f.setPointSizeF(fs); p->setFont(f);
    const QFontMetricsF fm(f);
    const int n = qMax(1, tabs.size());

    // 绘制标签
    for (int i = 0; i < tabs.size(); ++i) {
        QRectF r;
        if (horiz) {
            r = QRectF(tabBar.x() + tabBar.width() * i / n, tabBar.y(),
                tabBar.width() / n, tabBar.height());
        }
        else {
            r = QRectF(tabBar.x(), tabBar.y() + tabBar.height() * i / n,
                tabBar.width(), tabBar.height() / n);
        }
        p->setPen(i == cur ? actTx : tabTx);
        p->drawText(r, Qt::AlignCenter,
            fm.elidedText(tabs.at(i), Qt::ElideRight, int(qMax(8.0, r.width() - 6))));

        if (i == cur) {
            // 指示条
            p->setPen(Qt::NoPen);
            p->setBrush(indC);
            const double t = 3.0;
            if (pos == QLatin1String("bottom")) {
                p->drawRect(QRectF(r.x() + 6, r.y(), r.width() - 12, t));
            }
            else if (pos == QLatin1String("left")) {
                p->drawRect(QRectF(r.right() - t, r.y() + 6, t, r.height() - 12));
            }
            else if (pos == QLatin1String("right")) {
                p->drawRect(QRectF(r.x(), r.y() + 6, t, r.height() - 12));
            }
            else {
                p->drawRect(QRectF(r.x() + 6, r.bottom() - t, r.width() - 12, t));
            }
        }
    }

    // 内容区占位文字
    p->setPen(QColor(0, 0, 0, 100));
    QFont cf = f; cf.setPointSizeF(fs + 1); p->setFont(cf);
    if (cur < tabs.size())
        p->drawText(content, Qt::AlignCenter, tabs.at(cur));

    // 边框
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(border, 1));
    p->drawRect(body);

    if (!enabled) p->fillRect(body, QColor(255, 255, 255, 110));
    p->restore();
    return true;
}
bool drawHints::paintTileviewHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const int cols = qMax(1, int(getNumProp(h, inst, meta, "cols_from", 3)));
    const int rows = qMax(1, int(getNumProp(h, inst, meta, "rows_from", 2)));
    const int curC = qBound(0, int(getNumProp(h, inst, meta, "current_col_from", 0)), cols - 1);
    const int curR = qBound(0, int(getNumProp(h, inst, meta, "current_row_from", 0)), rows - 1);
    const QString labelsStr = getStrProp(h, inst, meta, "labels_from");
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor tileBg = getColorProp(h, inst, meta, "tile_bg_from", QColor("#f4f6f9"));
    const QColor actBg = getColorProp(h, inst, meta, "active_bg_from", QColor("#1976d2"));
    const QColor tileTx = getColorProp(h, inst, meta, "tile_text_from", QColor("#1f2933"));
    const QColor actTx = getColorProp(h, inst, meta, "active_text_from", QColor("#ffffff"));
    const QColor indC = getColorProp(h, inst, meta, "indicator_color_from", QColor("#1976d2"));
    const bool showInd = propValue(inst, meta, getStrHint(h, "show_indicator_from")).toBool();
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double gap = getNumProp(h, inst, meta, "gap_from", 4);
    const double fs = getNumProp(h, inst, meta, "font_size_from", 13);
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    // 解析标签：行 -> 换行；列 -> 逗号
    QVector<QStringList> grid(rows);
    {
        const QStringList lines = labelsStr.split('\n');
        for (int r = 0; r < rows && r < lines.size(); ++r) {
            grid[r] = lines.at(r).split(',');
        }
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    // 给页指示器留 12px
    const double indH = showInd ? 12.0 : 0.0;
    const QRectF gridRect(body.x() + 4, body.y() + 4,
        body.width() - 8,
        body.height() - 8 - indH);
    const double cellW = (gridRect.width() - gap * (cols - 1)) / cols;
    const double cellH = (gridRect.height() - gap * (rows - 1)) / rows;

    QFont f = p->font(); f.setPointSizeF(fs); p->setFont(f);
    const QFontMetricsF fm(f);

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const QRectF cell(gridRect.x() + (cellW + gap) * c,
                gridRect.y() + (cellH + gap) * r,
                cellW, cellH);
            const bool act = (r == curR && c == curC);
            p->setBrush(act ? actBg : tileBg);
            if (bw > 0) p->setPen(QPen(border, bw));
            else        p->setPen(Qt::NoPen);
            p->drawRect(cell);

            p->setPen(act ? actTx : tileTx);
            QString txt = (r < grid.size() && c < grid[r].size())
                ? grid[r].at(c).trimmed() : QString();
            if (!txt.isEmpty()) {
                p->drawText(cell, Qt::AlignCenter,
                    fm.elidedText(txt, Qt::ElideRight, int(cell.width() - 6)));
            }
        }
    }

    // 页指示器（点序列，列方向）
    if (showInd) {
        const double dotR = 3.0;
        const double total = cols * (dotR * 2 + 6);
        double x = body.center().x() - total / 2.0;
        const double y = body.bottom() - indH / 2.0;
        for (int c = 0; c < cols; ++c) {
            p->setPen(Qt::NoPen);
            if (c == curC) {
                p->setBrush(indC);
                p->drawEllipse(QPointF(x + dotR, y), dotR + 1, dotR + 1);
            }
            else {
                QColor d = indC; d.setAlphaF(0.35);
                p->setBrush(d);
                p->drawEllipse(QPointF(x + dotR, y), dotR, dotR);
            }
            x += dotR * 2 + 6;
        }
    }

    if (!enabled) p->fillRect(body, QColor(255, 255, 255, 110));
    p->restore();
    return true;
}
bool drawHints::paintMsgboxHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const QString title = getStrProp(h, inst, meta, "title_from");
    const QString msg = getStrProp(h, inst, meta, "message_from");
    const QStringList btns = getStrProp(h, inst, meta, "buttons_from").split(',', Qt::SkipEmptyParts);
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor titleC = getColorProp(h, inst, meta, "title_color_from", QColor("#1f2933"));
    const QColor titleBg = getColorProp(h, inst, meta, "title_bg_from", QColor("#e8eef7"));
    const QColor msgC = getColorProp(h, inst, meta, "message_color_from", QColor("#3c4451"));
    const QColor btnBg = getColorProp(h, inst, meta, "btn_bg_from", QColor("#1976d2"));
    const QColor btnTx = getColorProp(h, inst, meta, "btn_text_from", QColor("#ffffff"));
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#cfd6dd"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double rad = getNumProp(h, inst, meta, "radius_from", 8);
    const double tfs = getNumProp(h, inst, meta, "title_size_from", 16);
    const double fs = getNumProp(h, inst, meta, "font_size_from", 13);
    const bool showClose = propValue(inst, meta, getStrHint(h, "show_close_btn_from")).toBool();
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // 阴影
    QColor sh(0, 0, 0, 36);
    p->setPen(Qt::NoPen);
    p->setBrush(sh);
    p->drawRoundedRect(body.adjusted(2, 4, 2, 4), rad, rad);

    // 主体
    if (bw > 0) p->setPen(QPen(border, bw)); else p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRoundedRect(body, rad, rad);

    // 标题栏（顶部 ~ tfs+12）
    const double titleH = tfs + 16;
    QRectF titleRect(body.x(), body.y(), body.width(), titleH);
    QPainterPath titlePath;
    titlePath.addRoundedRect(titleRect, rad, rad);
    // 把下边裁掉的圆角通过覆盖一个矩形修正
    QPainterPath cover; cover.addRect(QRectF(body.x(), body.y() + rad, body.width(), titleH - rad));
    titlePath = titlePath.united(cover);
    p->setPen(Qt::NoPen);
    p->setBrush(titleBg);
    p->drawPath(titlePath);

    // 标题文字
    QFont tf = p->font(); tf.setPointSizeF(tfs); tf.setBold(true);
    p->setFont(tf);
    p->setPen(titleC);
    QRectF titleTextRect = titleRect.adjusted(14, 0, -14 - (showClose ? titleH : 0), 0);
    p->drawText(titleTextRect, Qt::AlignVCenter | Qt::AlignLeft, title);

    // 关闭按钮 ×
    if (showClose) {
        const QRectF cb(titleRect.right() - titleH, titleRect.y(), titleH, titleH);
        p->setPen(QPen(titleC, 1.5));
        p->setBrush(Qt::NoBrush);
        const double pad = titleH * 0.3;
        p->drawLine(QPointF(cb.x() + pad, cb.y() + pad),
            QPointF(cb.right() - pad, cb.bottom() - pad));
        p->drawLine(QPointF(cb.right() - pad, cb.y() + pad),
            QPointF(cb.x() + pad, cb.bottom() - pad));
    }

    // 按钮区
    const double btnH = btns.isEmpty() ? 0.0 : qMin(40.0, body.height() * 0.22);
    const QRectF bodyContent(body.x() + 16, body.y() + titleH + 8,
        body.width() - 32,
        body.height() - titleH - 16 - btnH - (btns.isEmpty() ? 0 : 8));

    // 正文
    QFont mf = p->font(); mf.setPointSizeF(fs); mf.setBold(false);
    p->setFont(mf);
    p->setPen(msgC);
    if (bodyContent.width() > 0 && bodyContent.height() > 0) {
        p->drawText(bodyContent, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, msg);
    }

    // 按钮组（右对齐）
    if (!btns.isEmpty() && btnH > 0) {
        const QFontMetricsF bfm(mf);
        double x = body.right() - 16;
        const double y = body.bottom() - 12 - btnH;
        for (int i = btns.size() - 1; i >= 0; --i) {
            const QString b = btns.at(i).trimmed();
            const double w = qMax(70.0, bfm.horizontalAdvance(b) + 28);
            const QRectF br(x - w, y, w, btnH);
            p->setPen(Qt::NoPen);
            p->setBrush(btnBg);
            p->drawRoundedRect(br, btnH / 4.0, btnH / 4.0);
            p->setPen(btnTx);
            p->drawText(br, Qt::AlignCenter, b);
            x -= (w + 8);
            if (x < bodyContent.x()) break;
        }
    }

    if (!enabled) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(255, 255, 255, 110));
        p->drawRoundedRect(body, rad, rad);
    }
    p->restore();
    return true;
}
bool drawHints::paintTableHints   (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const int rows = qMax(1, int(getNumProp(h, inst, meta, "row_count_from", 4)));
    const int cols = qMax(1, int(getNumProp(h, inst, meta, "col_count_from", 3)));
    const int hdrRows = qBound(0, int(getNumProp(h, inst, meta, "header_rows_from", 1)), rows);
    const QString colWidthsStr = getStrProp(h, inst, meta, "col_widths_from");
    const QString dataStr = getStrProp(h, inst, meta, "data_from");
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor hdrBg = getColorProp(h, inst, meta, "header_bg_from", QColor("#e8eef7"));
    const QColor hdrTx = getColorProp(h, inst, meta, "header_text_from", QColor("#1f2933"));
    const QColor cellTx = getColorProp(h, inst, meta, "cell_text_from", QColor("#333333"));
    const QColor grid = getColorProp(h, inst, meta, "grid_color_from", QColor("#cfd6dd"));
    const double rowH = getNumProp(h, inst, meta, "row_height_from", 28);
    const double fontSz = getNumProp(h, inst, meta, "font_size_from", 12);
    const bool stripe = propValue(inst, meta, getStrHint(h, "stripe_rows_from")).toBool();
    const QColor stripeC = getColorProp(h, inst, meta, "stripe_color_from", QColor("#f6f8fa"));
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    // 解析列宽
    QVector<double> widths(cols, 0.0);
    {
        const QStringList parts = colWidthsStr.split(',', Qt::SkipEmptyParts);
        double sum = 0;
        for (int i = 0; i < cols && i < parts.size(); ++i) {
            widths[i] = parts.at(i).trimmed().toDouble();
            sum += widths[i];
        }
        if (sum <= 0.1) {
            const double w = body.width() / double(cols);
            for (int i = 0; i < cols; ++i) widths[i] = w;
        }
        else {
            // 按比例缩放到 body.width()
            const double scale = body.width() / sum;
            for (int i = 0; i < cols; ++i) widths[i] *= scale;
        }
    }

    // 解析数据
    QVector<QStringList> cells(rows);
    {
        const QStringList lines = dataStr.split('\n');
        for (int r = 0; r < rows; ++r) {
            if (r < lines.size()) {
                const QStringList cs = lines.at(r).split(',');
                cells[r] = cs;
            }
        }
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing, false);

    // 背景
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    QFont f = p->font();
    f.setPointSizeF(fontSz);
    p->setFont(f);
    const QFontMetricsF fm(f);

    // 行
    double y = body.y();
    for (int r = 0; r < rows; ++r) {
        const bool isHdr = r < hdrRows;
        const double rh = rowH;
        if (y >= body.bottom()) break;
        const double rowH2 = qMin(rh, body.bottom() - y);
        const QRectF rRect(body.x(), y, body.width(), rowH2);

        if (isHdr) {
            p->setBrush(hdrBg);
            p->setPen(Qt::NoPen);
            p->drawRect(rRect);
        }
        else if (stripe && (r - hdrRows) % 2 == 1) {
            p->setBrush(stripeC);
            p->setPen(Qt::NoPen);
            p->drawRect(rRect);
        }

        // 单元格文字
        double x = body.x();
        p->setPen(isHdr ? hdrTx : cellTx);
        for (int c = 0; c < cols; ++c) {
            const double cw = widths[c];
            const QRectF cellRect(x + 4, y, qMax(0.0, cw - 8), rowH2);
            QString txt = (c < cells.value(r).size()) ? cells[r].at(c) : QString();
            if (cellRect.width() > 0)
                p->drawText(cellRect, Qt::AlignVCenter | Qt::AlignLeft,
                    fm.elidedText(txt, Qt::ElideRight, int(cellRect.width())));
            x += cw;
        }
        y += rowH2;
    }

    // 网格线
    p->setPen(QPen(grid, 1));
    // 行线
    double yy = body.y() + rowH;
    for (int r = 1; r < rows && yy < body.bottom(); ++r, yy += rowH) {
        p->drawLine(QPointF(body.x(), yy), QPointF(body.right(), yy));
    }
    // 列线
    double xx = body.x();
    for (int c = 0; c < cols - 1; ++c) {
        xx += widths[c];
        if (xx >= body.right()) break;
        p->drawLine(QPointF(xx, body.y()), QPointF(xx, qMin(body.bottom(), y)));
    }
    // 外框
    p->setBrush(Qt::NoBrush);
    p->drawRect(body);

    if (!enabled) {
        p->fillRect(body, QColor(255, 255, 255, 110));
    }
    p->restore();
    return true;
}
bool drawHints::paintChartHints   (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
    const QString ct = getStrProp(h, inst, meta, "chart_type_from");
    const int pc = qMax(2, int(getNumProp(h, inst, meta, "point_count_from", 8)));
    const double yMin = getNumProp(h, inst, meta, "y_min_from", 0);
    const double yMax = getNumProp(h, inst, meta, "y_max_from", 100);
    const int divX = qMax(0, int(getNumProp(h, inst, meta, "div_x_from", 4)));
    const int divY = qMax(0, int(getNumProp(h, inst, meta, "div_y_from", 4)));
    const bool showGrid = propValue(inst, meta, getStrHint(h, "show_grid_from")).toBool();
    const QColor gridC = getColorProp(h, inst, meta, "grid_color_from", QColor("#dde2e8"));
    const QColor bg = getColorProp(h, inst, meta, "bg_color_from", QColor("#ffffff"));
    const QColor axisC = getColorProp(h, inst, meta, "axis_color_from", QColor("#7a8290"));
    const double lineW = getNumProp(h, inst, meta, "line_width_from", 2);
    const double ptSz = getNumProp(h, inst, meta, "point_size_from", 3);
    const bool showPts = propValue(inst, meta, getStrHint(h, "show_points_from")).toBool();
    const bool fillArea = propValue(inst, meta, getStrHint(h, "fill_area_from")).toBool();
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    const QString s1Str = getStrProp(h, inst, meta, "series1_data_from");
    const QColor  s1C = getColorProp(h, inst, meta, "series1_color_from", QColor("#1976d2"));
    const QString s2Str = getStrProp(h, inst, meta, "series2_data_from");
    const QColor  s2C = getColorProp(h, inst, meta, "series2_color_from", QColor("#e53935"));

    auto parseSeries = [](const QString& s) {
        QVector<double> out;
        const QStringList parts = s.split(',', Qt::SkipEmptyParts);
        for (const QString& p : parts) {
            bool ok = false;
            double v = p.trimmed().toDouble(&ok);
            if (ok) out.append(v);
        }
        return out;
        };
    const QVector<double> s1 = parseSeries(s1Str);
    const QVector<double> s2 = parseSeries(s2Str);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // 内边距 → 绘图区
    const double pad = qMax(8.0, qMin(body.width(), body.height()) * 0.06);
    const QRectF plot(body.x() + pad, body.y() + pad,
        qMax(1.0, body.width() - pad * 2),
        qMax(1.0, body.height() - pad * 2));

    // 背景
    p->setPen(Qt::NoPen);
    p->setBrush(bg);
    p->drawRect(body);

    // 网格
    if (showGrid) {
        p->setPen(QPen(gridC, 1, Qt::DashLine));
        for (int i = 1; i < divX; ++i) {
            const double x = plot.x() + plot.width() * i / double(divX);
            p->drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        for (int i = 1; i < divY; ++i) {
            const double y = plot.y() + plot.height() * i / double(divY);
            p->drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
    }

    // 坐标轴
    p->setPen(QPen(axisC, 1));
    p->drawLine(plot.bottomLeft(), plot.bottomRight());
    p->drawLine(plot.topLeft(), plot.bottomLeft());

    const double range = (yMax - yMin) > 0 ? (yMax - yMin) : 1.0;
    auto valueToY = [&](double v) {
        const double f = qBound(0.0, (v - yMin) / range, 1.0);
        return plot.bottom() - f * plot.height();
        };

    auto drawLineSeries = [&](const QVector<double>& data, const QColor& col) {
        if (data.isEmpty()) return;
        const int n = qMin(pc, data.size());
        QPolygonF pts;
        const double step = (n > 1) ? plot.width() / double(n - 1) : 0.0;
        for (int i = 0; i < n; ++i) {
            pts << QPointF(plot.x() + step * i, valueToY(data[i]));
        }
        if (fillArea && pts.size() >= 2) {
            QPolygonF poly = pts;
            poly.append(QPointF(pts.last().x(), plot.bottom()));
            poly.append(QPointF(pts.first().x(), plot.bottom()));
            QColor fc = col; fc.setAlphaF(0.2);
            p->setPen(Qt::NoPen);
            p->setBrush(fc);
            p->drawPolygon(poly);
        }
        QPen pen(col, lineW);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawPolyline(pts);
        if (showPts && ptSz > 0) {
            p->setPen(Qt::NoPen);
            p->setBrush(col);
            for (const QPointF& pt : pts) {
                p->drawEllipse(pt, ptSz, ptSz);
            }
        }
        };

    auto drawBarSeries = [&](const QVector<QVector<double>>& serList,
        const QVector<QColor>& cols) {
            if (serList.isEmpty()) return;
            const int seriesN = serList.size();
            const int maxN = pc;
            const double groupW = plot.width() / double(maxN);
            const double inner = groupW * 0.7;
            const double barW = inner / double(seriesN);
            for (int i = 0; i < maxN; ++i) {
                const double gx = plot.x() + groupW * i + (groupW - inner) / 2.0;
                for (int s = 0; s < seriesN; ++s) {
                    if (i >= serList[s].size()) continue;
                    const double v = serList[s].at(i);
                    const double y = valueToY(v);
                    const QRectF r(gx + barW * s, y, qMax(1.0, barW - 1.0),
                        qMax(0.0, plot.bottom() - y));
                    p->setPen(Qt::NoPen);
                    p->setBrush(cols[s]);
                    p->drawRect(r);
                }
            }
        };

    auto drawScatter = [&](const QVector<double>& data, const QColor& col) {
        if (data.isEmpty()) return;
        const int n = qMin(pc, data.size());
        const double step = (n > 1) ? plot.width() / double(n - 1) : 0.0;
        p->setPen(Qt::NoPen);
        p->setBrush(col);
        const double r = qMax(2.0, ptSz);
        for (int i = 0; i < n; ++i) {
            const QPointF pt(plot.x() + step * i, valueToY(data[i]));
            p->drawEllipse(pt, r, r);
        }
        };

    if (ct == QLatin1String("bar")) {
        QVector<QVector<double>> sers; QVector<QColor> cols;
        sers.append(s1); cols.append(s1C);
        if (!s2.isEmpty()) { sers.append(s2); cols.append(s2C); }
        drawBarSeries(sers, cols);
    }
    else if (ct == QLatin1String("scatter")) {
        drawScatter(s1, s1C);
        if (!s2.isEmpty()) drawScatter(s2, s2C);
    }
    else {
        drawLineSeries(s1, s1C);
        if (!s2.isEmpty()) drawLineSeries(s2, s2C);
    }

    if (!enabled) {
        p->fillRect(body, QColor(255, 255, 255, 120));
    }
    p->restore();
    return true;
}
bool drawHints::paintBarHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
     const double v = getNumProp(h, inst, meta, "value_from", 50);
        const double mn = getNumProp(h, inst, meta, "min_from", 0);
        const double mx = getNumProp(h, inst, meta, "max_from", 100);
        const QString md = getStrProp(h, inst, meta, "mode_from");
        const double vS = getNumProp(h, inst, meta, "value_start_from", 0);
        const QString ori = getStrProp(h, inst, meta, "orientation_from");
        const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
        const bool enabled = enV.isValid() ? enV.toBool() : true;

        QColor track = getColorProp(h, inst, meta, "track_fill_from", QColor("#cccccc"));
        QColor ind = getColorProp(h, inst, meta, "indicator_fill_from", QColor("#007acc"));
        const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#888888"));
        const double bw = getNumProp(h, inst, meta, "border_width_from", 0);
        const double trackPct = getNumProp(h, inst, meta, "track_radius_ratio_from", 100);
        const double indPct = getNumProp(h, inst, meta, "indicator_radius_ratio_from", 100);

        if (!enabled) {
            auto desat = [](QColor c) { int hh, s, vv, a; c.getHsv(&hh, &s, &vv, &a); c.setHsv(hh, s / 3, qMin(255, vv + 20), a); return c; };
            track = desat(track); ind = desat(ind);
        }

        const double range = (mx - mn) > 0 ? (mx - mn) : 1.0;
        const double frac = qBound(0.0, (v - mn) / range, 1.0);
        const double fracS = qBound(0.0, (vS - mn) / range, 1.0);

        const bool vertical = (ori == QLatin1String("vertical") || ori == QLatin1String("vertical_ttb"));
        const double minDim = vertical ? body.width() : body.height();
        const double trackR = minDim * (trackPct / 200.0);
        const double indR = minDim * (indPct / 200.0);

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        if (bw > 0) p->setPen(QPen(border, bw)); else p->setPen(Qt::NoPen);
        p->setBrush(track);
        if (trackR > 0) p->drawRoundedRect(body, trackR, trackR);
        else            p->drawRect(body);

        // 计算指示矩形
        QRectF indRect = body;
        if (!vertical) {
            const double w = body.width();
            if (md == QLatin1String("range")) {
                double a = body.x() + fracS * w;
                double b = body.x() + frac * w;
                if (a > b) { double t = a; a = b; b = t; }
                indRect.setLeft(a); indRect.setRight(b);
            }
            else if (md == QLatin1String("symmetrical")) {
                const double mid = body.x() + w / 2.0;
                const double xv = body.x() + frac * w;
                indRect.setLeft(qMin(mid, xv));
                indRect.setRight(qMax(mid, xv));
            }
            else {
                if (ori == QLatin1String("horizontal_rtl")) {
                    indRect.setLeft(body.right() - frac * w);
                    indRect.setRight(body.right());
                }
                else {
                    indRect.setRight(body.x() + frac * w);
                }
            }
        }
        else {
            const double hgt = body.height();
            if (md == QLatin1String("range")) {
                // 垂直时把 frac 反转（值大向上）
                double a = body.bottom() - fracS * hgt;
                double b = body.bottom() - frac * hgt;
                if (a > b) { double t = a; a = b; b = t; }
                indRect.setTop(a); indRect.setBottom(b);
            }
            else if (md == QLatin1String("symmetrical")) {
                const double mid = body.y() + hgt / 2.0;
                const double yv = body.bottom() - frac * hgt;
                indRect.setTop(qMin(mid, yv));
                indRect.setBottom(qMax(mid, yv));
            }
            else {
                if (ori == QLatin1String("vertical_ttb")) {
                    indRect.setTop(body.y());
                    indRect.setBottom(body.y() + frac * hgt);
                }
                else {
                    indRect.setTop(body.bottom() - frac * hgt);
                    indRect.setBottom(body.bottom());
                }
            }
        }

        p->setPen(Qt::NoPen);
        p->setBrush(ind);
        if (indR > 0) p->drawRoundedRect(indRect, indR, indR);
        else          p->drawRect(indRect);
        p->restore();
        return true;
}
bool drawHints::paintArcHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;
     const double v = getNumProp(h, inst, meta, "value_from", 50);
        const double mn = getNumProp(h, inst, meta, "min_from", 0);
        const double mx = getNumProp(h, inst, meta, "max_from", 100);
        const double bgS = getNumProp(h, inst, meta, "bg_start_angle_from", 135);
        const double bgE = getNumProp(h, inst, meta, "bg_end_angle_from", 45);
        const double rot = getNumProp(h, inst, meta, "rotation_from", 0);
        const QString mode = getStrProp(h, inst, meta, "mode_from");
        const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
        const bool enabled = enV.isValid() ? enV.toBool() : true;

        QColor bgC = getColorProp(h, inst, meta, "bg_arc_color_from", QColor("#cccccc"));
        QColor indC = getColorProp(h, inst, meta, "ind_arc_color_from", QColor("#007acc"));
        const double aw = getNumProp(h, inst, meta, "arc_width_from", 8);
        const bool   caps = propValue(inst, meta, getStrHint(h, "rounded_caps_from")).toBool();
        const bool   showKnob = propValue(inst, meta, getStrHint(h, "show_knob_from")).toBool();
        const QColor knobC = getColorProp(h, inst, meta, "knob_color_from", QColor("#ffffff"));

        if (!enabled) {
            auto desat = [](QColor c) { int hh, s, vv, a; c.getHsv(&hh, &s, &vv, &a); c.setHsv(hh, s / 3, qMin(255, vv + 20), a); return c; };
            bgC = desat(bgC); indC = desat(indC);
        }

        // LVGL：0°=右，顺时针递增，含 rotation 偏移
        // Qt 的 drawArc：0°=右(3点钟方向)，逆时针递增，单位 1/16 度
        const double diam = qMin(body.width(), body.height()) - aw;
        if (diam <= 0) { return true; }
        const QRectF circ(body.center().x() - diam / 2.0,
            body.center().y() - diam / 2.0,
            diam, diam);

        auto lvAngleToQt = [&](double lv_deg) -> double {
            // Qt y 轴向下，要让顺时针视觉表现一致，用 -角度
            return -(lv_deg + rot);
            };

        // 背景弧 sweep 方向（LVGL 顺时针 from bgS to bgE）
        double bgStartQt = lvAngleToQt(bgS);
        double bgSpan = bgE - bgS;
        if (bgSpan < 0) bgSpan += 360.0;
        double bgSpanQt = -bgSpan; // Qt 逆时针为正，所以负值=顺时针

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        QPen pen(bgC);
        pen.setWidthF(aw);
        pen.setCapStyle(caps ? Qt::RoundCap : Qt::FlatCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p->setPen(pen);
        p->setBrush(Qt::NoBrush);
        p->drawArc(circ,
            int(bgStartQt * 16),
            int(bgSpanQt * 16));

        // 指示弧
        const double range = (mx - mn) > 0 ? (mx - mn) : 1.0;
        const double frac = qBound(0.0, (v - mn) / range, 1.0);
        double indStartLv = bgS;
        double indSweepLv = bgSpan * frac;
        if (mode == QLatin1String("reverse")) {
            indStartLv = bgS + bgSpan - bgSpan * frac;
            indSweepLv = bgSpan * frac;
        }
        else if (mode == QLatin1String("symmetrical")) {
            const double mid = bgS + bgSpan / 2.0;
            const double valDeg = bgSpan * frac; // 0..bgSpan
            const double midFrac = 0.5 * bgSpan;
            if (valDeg >= midFrac) {
                indStartLv = mid;
                indSweepLv = valDeg - midFrac;
            }
            else {
                indStartLv = bgS + valDeg;
                indSweepLv = midFrac - valDeg;
            }
        }
        pen.setColor(indC);
        p->setPen(pen);
        p->drawArc(circ,
            int(lvAngleToQt(indStartLv) * 16),
            int(-indSweepLv * 16));

        // 拖动球（可选）：画在指示弧的端点
        if (showKnob) {
            const double endLv = (mode == QLatin1String("reverse"))
                ? indStartLv
                : (indStartLv + indSweepLv);
            const double rad = (endLv + rot) * M_PI / 180.0;
            const double cx = body.center().x() + (diam / 2.0) * std::cos(rad);
            const double cy = body.center().y() + (diam / 2.0) * std::sin(rad);
            const double ks = aw * 1.6;
            p->setPen(QPen(QColor(0, 0, 0, 40), 0.5));
            p->setBrush(knobC);
            p->drawEllipse(QPointF(cx, cy), ks / 2.0, ks / 2.0);
        }

        p->restore();
        return true;
}
bool drawHints::paintLedHints     (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const bool   on = propValue(inst, meta, getStrHint(h, "on_from")).toBool();
    const double bri = getNumProp(h, inst, meta, "brightness_from", 255);
    const QColor col = getColorProp(h, inst, meta, "color_from", QColor("#22cc44"));
    const QString sh = getStrProp(h, inst, meta, "led_shape_from");
    const bool   glow = propValue(inst, meta, getStrHint(h, "glow_from")).toBool();

    // 亮度归一化
    const double t = qBound(0.0, bri / 255.0, 1.0);
    // 熄灭：暗色版本（HSV -V）
    QColor body_col = col;
    if (!on) {
        int hh, s, vv, a;
        body_col.getHsv(&hh, &s, &vv, &a);
        body_col.setHsv(hh, s / 2, qMax(40, vv / 4), a);
    }
    else if (t < 1.0) {
        int hh, s, vv, a;
        body_col.getHsv(&hh, &s, &vv, &a);
        body_col.setHsv(hh, s, int(vv * (0.4 + 0.6 * t)), a);
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // 发光晕（仅在点亮时）
    if (on && glow) {
        QRadialGradient rg(body.center(), qMax(body.width(), body.height()) * 0.7);
        QColor glowCol = col;
        glowCol.setAlphaF(0.45 * t);
        rg.setColorAt(0.0, glowCol);
        QColor glowOuter = glowCol;
        glowOuter.setAlphaF(0.0);
        rg.setColorAt(1.0, glowOuter);
        p->setPen(Qt::NoPen);
        p->setBrush(rg);
        p->drawEllipse(body.center(),
            body.width() * 0.75,
            body.height() * 0.75);
    }

    // LED 主体
    const double inset = qMax(1.0, qMin(body.width(), body.height()) * 0.06);
    const QRectF inner = body.adjusted(inset, inset, -inset, -inset);

    p->setPen(QPen(QColor(0, 0, 0, 60), 1));
    p->setBrush(body_col);
    if (sh == QLatin1String("square")) {
        p->drawRect(inner);
    }
    else {
        p->drawEllipse(inner);
    }

    // 高光
    if (on) {
        const QRectF hl(inner.x() + inner.width() * 0.18,
            inner.y() + inner.height() * 0.12,
            inner.width() * 0.45,
            inner.height() * 0.32);
        QColor hlCol(255, 255, 255, int(140 * t));
        p->setPen(Qt::NoPen);
        p->setBrush(hlCol);
        if (sh == QLatin1String("square")) p->drawRect(hl);
        else                                p->drawEllipse(hl);
    }
    p->restore();
    return true;
}
bool drawHints::paintSliderHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const double v = getNumProp(h, inst, meta, "value_from", 50);
    const double mn = getNumProp(h, inst, meta, "min_from", 0);
    const double mx = getNumProp(h, inst, meta, "max_from", 100);
    const QString md = getStrProp(h, inst, meta, "mode_from");
    const double vL = getNumProp(h, inst, meta, "value_left_from", 0);
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool enabled = enV.isValid() ? enV.toBool() : true;

    QColor track = getColorProp(h, inst, meta, "track_fill_from", QColor("#cccccc"));
    QColor ind = getColorProp(h, inst, meta, "indicator_fill_from", QColor("#007acc"));
    QColor knob = getColorProp(h, inst, meta, "knob_fill_from", QColor("#ffffff"));
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#007acc"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 2);
    const double trackPct = getNumProp(h, inst, meta, "track_radius_ratio_from", 100);
    const double knobPct = getNumProp(h, inst, meta, "knob_radius_ratio_from", 100);

    if (!enabled) {
        auto desat = [](QColor c) { int hh, s, vv, a; c.getHsv(&hh, &s, &vv, &a); c.setHsv(hh, s / 3, qMin(255, vv + 20), a); return c; };
        track = desat(track); ind = desat(ind); knob = desat(knob);
    }

    // 轨道：水平细条，居中
    const double trackH = qMax(4.0, body.height() * 0.35);
    const QRectF trackRect(body.x(),
        body.y() + (body.height() - trackH) / 2.0,
        body.width(),
        trackH);
    const double trackR = trackH * (trackPct / 200.0);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(track);
    if (trackR > 0) p->drawRoundedRect(trackRect, trackR, trackR);
    else            p->drawRect(trackRect);

    // 指示条
    const double range = (mx - mn) > 0 ? (mx - mn) : 1.0;
    const double frac = qBound(0.0, (v - mn) / range, 1.0);
    QRectF indRect = trackRect;
    if (md == QLatin1String("range")) {
        const double fL = qBound(0.0, (vL - mn) / range, 1.0);
        indRect.setLeft(trackRect.x() + fL * trackRect.width());
        indRect.setRight(trackRect.x() + frac * trackRect.width());
        if (indRect.left() > indRect.right()) {
            const double t = indRect.left();
            indRect.setLeft(indRect.right());
            indRect.setRight(t);
        }
    }
    else if (md == QLatin1String("symmetrical")) {
        const double mid = trackRect.x() + trackRect.width() / 2.0;
        const double xv = trackRect.x() + frac * trackRect.width();
        indRect.setLeft(qMin(mid, xv));
        indRect.setRight(qMax(mid, xv));
    }
    else {
        indRect.setRight(trackRect.x() + frac * trackRect.width());
    }
    p->setBrush(ind);
    if (trackR > 0) p->drawRoundedRect(indRect, trackR, trackR);
    else            p->drawRect(indRect);

    // 滑块
    auto drawKnob = [&](double frac01) {
        const double knobSize = qMax(trackH + 4.0, body.height() * 0.95);
        const double cx = trackRect.x() + frac01 * trackRect.width();
        const QRectF k(cx - knobSize / 2.0,
            body.y() + (body.height() - knobSize) / 2.0,
            knobSize, knobSize);
        const double kr = knobSize * (knobPct / 200.0);
        p->setBrush(QColor(0, 0, 0, 40));
        p->setPen(Qt::NoPen);
        const QRectF sh = k.translated(0, 1);
        if (kr > 0) p->drawRoundedRect(sh, kr, kr); else p->drawRect(sh);
        p->setBrush(knob);
        if (bw > 0) p->setPen(QPen(border, bw));
        else        p->setPen(QPen(QColor(0, 0, 0, 30), 0.5));
        if (kr > 0) p->drawRoundedRect(k, kr, kr); else p->drawRect(k);
        };
    if (md == QLatin1String("range")) {
        const double fL = qBound(0.0, (vL - mn) / range, 1.0);
        drawKnob(fL);
        drawKnob(frac);
    }
    else {
        drawKnob(frac);
    }
    p->restore();
    return true;
}
bool drawHints::paintTextareaHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString txt = getStrProp(h, inst, meta, "text_from");
    const QString phd = getStrProp(h, inst, meta, "placeholder_from");
    const QColor  tcol = getColorProp(h, inst, meta, "text_color_from", QColor("#000000"));
    const QColor  pcol = getColorProp(h, inst, meta, "placeholder_color_from", QColor("#aaaaaa"));
    const QColor  fill = getColorProp(h, inst, meta, "fill_from", QColor("#ffffff"));
    const QColor  bcol = getColorProp(h, inst, meta, "border_color_from", QColor("#888888"));
    const double  bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double  rd = getNumProp(h, inst, meta, "radius_from", 4);
    const double  fsz = getNumProp(h, inst, meta, "font_size_from", 14);
    const QString fam = getStrProp(h, inst, meta, "font_family_from");
    const bool    one = propValue(inst, meta, getStrHint(h, "one_line_from")).toBool();
    const bool    pwd = propValue(inst, meta, getStrHint(h, "password_from")).toBool();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(fill);
    if (bw > 0) p->setPen(QPen(bcol, bw)); else p->setPen(Qt::NoPen);
    if (rd > 0) p->drawRoundedRect(body, rd, rd); else p->drawRect(body);

    QFont f = p->font();
    if (fsz > 0) f.setPixelSize(int(fsz));
    if (!fam.isEmpty() && fam != QLatin1String("default")) f.setFamily(fam);
    p->setFont(f);

    QString display = txt;
    bool isPlaceholder = display.isEmpty();
    if (isPlaceholder) display = phd;
    if (pwd && !isPlaceholder) display = QString(display.size(), QChar(0x2022)); // ●

    const QRectF textRect = body.adjusted(8, 4, -8, -4);
    p->setPen(isPlaceholder ? pcol : tcol);
    int flags = Qt::AlignLeft | (one ? Qt::AlignVCenter : Qt::AlignTop) | Qt::TextWordWrap;
    p->drawText(textRect, flags, display);
    p->restore();
    return true;
}
bool drawHints::paintDropdownHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString options = getStrProp(h, inst, meta, "options_from");
    const int     selIdx = int(getNumProp(h, inst, meta, "selected_from", 0));
    const QColor  tcol = getColorProp(h, inst, meta, "text_color_from", QColor("#000000"));
    const QColor  fill = getColorProp(h, inst, meta, "fill_from", QColor("#ffffff"));
    const QColor  bcol = getColorProp(h, inst, meta, "border_color_from", QColor("#888888"));
    const double  bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double  rd = getNumProp(h, inst, meta, "radius_from", 4);
    const double  fsz = getNumProp(h, inst, meta, "font_size_from", 14);
    const QString fam = getStrProp(h, inst, meta, "font_family_from");
    const QColor  arrow = getColorProp(h, inst, meta, "arrow_color_from", QColor("#666666"));

    const QStringList items = options.split(QChar('\n'));
    QString cur = (selIdx >= 0 && selIdx < items.size()) ? items[selIdx] : QString();

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(fill);
    if (bw > 0) p->setPen(QPen(bcol, bw)); else p->setPen(Qt::NoPen);
    if (rd > 0) p->drawRoundedRect(body, rd, rd); else p->drawRect(body);

    QFont f = p->font();
    if (fsz > 0) f.setPixelSize(int(fsz));
    if (!fam.isEmpty() && fam != QLatin1String("default")) f.setFamily(fam);
    p->setFont(f);
    p->setPen(tcol);
    const double arrowW = qMin(body.height(), 20.0);
    const QRectF textRect = body.adjusted(8, 0, -arrowW - 4, 0);
    p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, cur);

    // 右侧下三角箭头
    p->setPen(Qt::NoPen);
    p->setBrush(arrow);
    const double cx = body.right() - arrowW / 2.0 - 4;
    const double cy = body.center().y();
    const double ah = qMin(arrowW * 0.4, 8.0);
    QPainterPath tri;
    tri.moveTo(cx - ah, cy - ah / 2.0);
    tri.lineTo(cx + ah, cy - ah / 2.0);
    tri.lineTo(cx, cy + ah / 2.0);
    tri.closeSubpath();
    p->drawPath(tri);
    p->restore();
    return true;
}
bool drawHints::paintRollerHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString options = getStrProp(h, inst, meta, "options_from");
    const int     selIdx = int(getNumProp(h, inst, meta, "selected_from", 0));
    const int     rowCnt = qMax(1, int(getNumProp(h, inst, meta, "visible_row_count_from", 3)));
    const QColor  tcol = getColorProp(h, inst, meta, "text_color_from", QColor("#666666"));
    const QColor  stcol = getColorProp(h, inst, meta, "sel_text_color_from", QColor("#000000"));
    const QColor  fill = getColorProp(h, inst, meta, "fill_from", QColor("#ffffff"));
    const QColor  selFill = getColorProp(h, inst, meta, "sel_fill_from", QColor("#e6f2fb"));
    const QColor  bcol = getColorProp(h, inst, meta, "border_color_from", QColor("#cccccc"));
    const double  bw = getNumProp(h, inst, meta, "border_width_from", 1);
    const double  rd = getNumProp(h, inst, meta, "radius_from", 4);
    const double  fsz = getNumProp(h, inst, meta, "font_size_from", 16);
    const QString fam = getStrProp(h, inst, meta, "font_family_from");

    const QStringList items = options.split(QChar('\n'));

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(fill);
    if (bw > 0) p->setPen(QPen(bcol, bw)); else p->setPen(Qt::NoPen);
    if (rd > 0) p->drawRoundedRect(body, rd, rd); else p->drawRect(body);

    QFont f = p->font();
    if (fsz > 0) f.setPixelSize(int(fsz));
    if (!fam.isEmpty() && fam != QLatin1String("default")) f.setFamily(fam);
    p->setFont(f);

    const double rowH = body.height() / double(rowCnt);
    const int    centerRow = rowCnt / 2;

    // 选中行高亮（位于中间）
    const QRectF selRect(body.x(),
        body.y() + centerRow * rowH,
        body.width(),
        rowH);
    p->setPen(Qt::NoPen);
    p->setBrush(selFill);
    p->drawRect(selRect);

    // 绘制每一行
    p->setClipRect(body);
    for (int r = 0; r < rowCnt; ++r) {
        const int idx = selIdx + (r - centerRow);
        if (idx < 0 || idx >= items.size()) continue;
        const QRectF rowRect(body.x(),
            body.y() + r * rowH,
            body.width(),
            rowH);
        p->setPen(r == centerRow ? stcol : tcol);
        p->drawText(rowRect, Qt::AlignCenter, items[idx]);
    }
    p->setClipping(false);
    p->restore();
    return true;
}
bool drawHints::paintSwitchHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const bool   checked = propValue(inst, meta, getStrHint(h, "checked_from")).toBool();
    const QVariant enV = propValue(inst, meta, getStrHint(h, "enabled_from"));
    const bool   enabled = enV.isValid() ? enV.toBool() : true;

    QColor trackOff = getColorProp(h, inst, meta, "track_off_fill_from", QColor("#cccccc"));
    QColor trackOn = getColorProp(h, inst, meta, "track_on_fill_from", QColor("#007acc"));
    QColor knobCol = getColorProp(h, inst, meta, "knob_fill_from", QColor("#ffffff"));
    const QColor border = getColorProp(h, inst, meta, "border_color_from", QColor("#888888"));
    const double bw = getNumProp(h, inst, meta, "border_width_from", 0);
    const double trackPct = getNumProp(h, inst, meta, "track_radius_ratio_from", 100);
    const double knobPct = getNumProp(h, inst, meta, "knob_radius_ratio_from", 100);

    if (!enabled) {
        auto desat = [](QColor c) {
            int h2, s, v, a;
            c.getHsv(&h2, &s, &v, &a);
            c.setHsv(h2, s / 3, qMin(255, v + 20), a);
            return c;
            };
        trackOff = desat(trackOff);
        trackOn = desat(trackOn);
        knobCol = desat(knobCol);
    }

    const QRectF track = body;
    const double trackR = track.height() * (trackPct / 200.0);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // 轨道
    p->setBrush(checked ? trackOn : trackOff);
    if (bw > 0) p->setPen(QPen(border, bw));
    else        p->setPen(Qt::NoPen);
    if (trackR > 0) p->drawRoundedRect(track, trackR, trackR);
    else            p->drawRect(track);

    // 滑块
    const double pad = track.height() * 0.10;
    const double knobSize = qMax(2.0, track.height() - pad * 2.0);
    const double knobY = track.y() + (track.height() - knobSize) / 2.0;
    const double knobXOff = track.x() + pad;
    const double knobXOn = track.right() - pad - knobSize;
    const QRectF knob(checked ? knobXOn : knobXOff, knobY, knobSize, knobSize);
    const double knobR = knobSize * (knobPct / 200.0);

    // 滑块阴影
    p->setPen(Qt::NoPen);
    p->setBrush(QColor(0, 0, 0, 40));
    const QRectF shadow = knob.translated(0, 1);
    if (knobR > 0) p->drawRoundedRect(shadow, knobR, knobR);
    else           p->drawRect(shadow);

    // 滑块本体
    p->setBrush(knobCol);
    p->setPen(QPen(QColor(0, 0, 0, 30), 0.5));
    if (knobR > 0) p->drawRoundedRect(knob, knobR, knobR);
    else           p->drawRect(knob);

    p->restore();
    return true;
}
bool drawHints::paintCheckboxHints(QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString text = getStrProp(h, inst, meta, "text_from");
    const QColor  tcol = getColorProp(h, inst, meta, "text_color_from", QColor("#000000"));
    const double  fsz = getNumProp(h, inst, meta, "font_size_from", 14);
    const QString family = getStrProp(h, inst, meta, "font_family_from");
    const bool    checked = propValue(inst, meta,
        getStrHint(h, "checked_from")).toBool();
    const QColor  boxFill = getColorProp(h, inst, meta, "box_fill_from", QColor("#ffffff"));
    const QColor  boxChecked = getColorProp(h, inst, meta, "checked_fill_from", QColor("#007acc"));
    const QColor  boxBorder = getColorProp(h, inst, meta, "border_color_from", QColor("#888888"));
    const double  boxBW = getNumProp(h, inst, meta, "border_width_from", 1);
    const double  boxR = getNumProp(h, inst, meta, "radius_from", 2);

    // 勾选框尺寸：取行高，最大 22，最小 12
    const double boxSize = qBound(12.0, body.height() - 4.0, 22.0);
    const double boxX = body.x() + 2.0;
    const double boxY = body.y() + (body.height() - boxSize) / 2.0;
    const QRectF boxRect(boxX, boxY, boxSize, boxSize);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    // 勾选框
    p->setBrush(checked ? boxChecked : boxFill);
    if (boxBW > 0) p->setPen(QPen(boxBorder, boxBW));
    else           p->setPen(Qt::NoPen);
    if (boxR > 0)  p->drawRoundedRect(boxRect, boxR, boxR);
    else           p->drawRect(boxRect);

    // 勾号（√）
    if (checked) {
        QPen tickPen(QColor("#ffffff"));
        tickPen.setWidthF(qMax(1.5, boxSize / 8.0));
        tickPen.setCapStyle(Qt::RoundCap);
        tickPen.setJoinStyle(Qt::RoundJoin);
        p->setPen(tickPen);
        p->setBrush(Qt::NoBrush);
        const double m = boxSize * 0.18;
        QPointF p1(boxRect.x() + m, boxRect.y() + boxSize * 0.55);
        QPointF p2(boxRect.x() + boxSize * 0.42, boxRect.y() + boxSize - m);
        QPointF p3(boxRect.x() + boxSize - m, boxRect.y() + m);
        QPainterPath pp;
        pp.moveTo(p1); pp.lineTo(p2); pp.lineTo(p3);
        p->drawPath(pp);
    }

    // 文本
    if (!text.isEmpty()) {
        QFont f = p->font();
        if (fsz > 0) f.setPixelSize(int(fsz));
        if (!family.isEmpty() && family != QLatin1String("default")) f.setFamily(family);
        p->setFont(f);
        p->setPen(tcol);
        const QRectF textRect(boxRect.right() + 6,
            body.y(),
            body.right() - boxRect.right() - 6 - 2,
            body.height());
        p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    }
    p->restore();
    return true;
}
// 自定义控件：根据 draw_hints 中的 shape/fill/border/radius/text/align 逐步绘制。
// 作为 paintWithDrawHints 的默认分支调用，处理 rect/ellipse/rounded_rect 等通用形状。
bool drawHints::paintCustomHints  (QPainter* p, const QRectF& body, const WidgetInstance& inst, const WidgetMeta& meta){
    const QVariantMap& h = meta.drawHints;
    if (h.isEmpty()) return false;

    const QString shape = h.value(QStringLiteral("shape"), QStringLiteral("rect")).toString();

    // 填充色：禁用态优先，再回落到正常 fill_from
    QColor fill = getColorProp(h, inst, meta, "fill_from", QColor("#3a3a3a"));
    {
        const QVariant en = propValue(inst, meta, QStringLiteral("enabled"));
        if (en.isValid() && !en.toBool()) {
            const QColor disabled = getColorProp(h, inst, meta, "disabled_fill_from", QColor("#888888"));
            fill = disabled;
        }
    }
    const QColor borderCol = getColorProp(h, inst, meta, "border_color_from", QColor("#005a9e"));
    const double borderW = getNumProp(h, inst, meta, "border_width_from", 0);
    const double radius = getNumProp(h, inst, meta, "radius_from", 0);

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(fill);
    if (borderW > 0) p->setPen(QPen(borderCol, borderW));
    else             p->setPen(Qt::NoPen);

    // 1) 外形
    if (shape == QLatin1String("ellipse"))
        p->drawEllipse(body);
    else if (shape == QLatin1String("rounded_rect") || radius > 0)
        p->drawRoundedRect(body, radius, radius);
    else
        p->drawRect(body);

    // 2) 文本
    const QString text = getStrProp(h, inst, meta, "text_from");
    if (!text.isEmpty()) {
        const QColor tcol = getColorProp(h, inst, meta, "text_color_from", Qt::white);
        const double fsz = getNumProp(h, inst, meta, "font_size_from", 12);
        const QString align = getStrProp(h, inst, meta, "align_from");
        QFont f = p->font();
        if (fsz > 0) f.setPixelSize(int(fsz));
        const QString family = getStrProp(h, inst, meta, "font_family_from");
        if (!family.isEmpty() && family != QLatin1String("default")) f.setFamily(family);
        p->setFont(f);
        p->setPen(tcol);
        int flags = Qt::AlignVCenter;
        if (align == QLatin1String("left"))       flags |= Qt::AlignLeft;
        else if (align == QLatin1String("right")) flags |= Qt::AlignRight;
        else                                       flags |= Qt::AlignHCenter;
        p->drawText(body.adjusted(4, 0, -4, 0), flags, text);
    }
    p->restore();
    return true;
}