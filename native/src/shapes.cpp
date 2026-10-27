#include "shapes.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QVariantList>
#include <algorithm>
#include <cmath>

static QColor colorOf(const QVariant &v)
{
	const QString s = v.toString();
	return QColor::isValidColorName(s) ? QColor(s) : QColor();
}

static QVector<QPointF> pointsOf(const QVariantMap &st, double w, double h)
{
	QVector<QPointF> out;
	for (const QVariant &v : st.value("points").toList()) {
		const QVariantList p = v.toList();
		if (p.size() >= 2)
			out << QPointF(p[0].toDouble() * w, p[1].toDouble() * h);
	}
	return out;
}

// A smooth curve through the points: quadratic segments between their midpoints.
static QPainterPath smoothPath(const QVector<QPointF> &pts)
{
	QPainterPath path;
	if (pts.isEmpty())
		return path;
	path.moveTo(pts[0]);
	if (pts.size() < 3) {
		for (int i = 1; i < pts.size(); ++i)
			path.lineTo(pts[i]);
		if (pts.size() == 1)
			path.lineTo(pts[0] + QPointF(0.01, 0)); // a dot
		return path;
	}
	for (int i = 1; i < pts.size() - 1; ++i)
		path.quadTo(pts[i], (pts[i] + pts[i + 1]) / 2);
	path.lineTo(pts.last());
	return path;
}

QImage renderText(const QVariantMap &st, int width, int height, double lineHeight)
{
	const QStringList lines = st.value("text").toString().split('\n');
	const int n = int(std::max<qsizetype>(1, lines.size()));
	const double linePx = std::max(4.0, lineHeight > 0 ? lineHeight : double(height) / n);
	QFont f(st.value("font").toString());
	f.setPixelSize(std::max(1, int(std::lround(linePx))));
	f.setBold(st.value("bold", true).toBool());
	f.setItalic(st.value("italic").toBool());
	const QFontMetricsF fm(f);
	const double k = linePx / std::max(1.0, fm.lineSpacing());
	double wmax = 0;
	for (const QString &l : lines)
		wmax = std::max(wmax, fm.horizontalAdvance(l));
	if (width <= 0)
		width = int(std::ceil(wmax * k));
	// The text fills the box's width, as text scaled to its layer does on Windows: a box laid out
	// with another machine's font (a little narrower or wider) never cuts the text off.
	const double kx = wmax > 0 ? width / wmax : k;
	if (height <= 0)
		height = int(std::lround(linePx * n));
	QImage img(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
	p.scale(kx, k);
	p.setFont(f);
	p.setPen(QColor(st.value("textColor", "#ffffff").toString()));
	for (int i = 0; i < lines.size(); ++i)
		p.drawText(QPointF(0, i * fm.lineSpacing() + fm.ascent()), lines[i]);
	p.end();
	return img.convertToFormat(QImage::Format_ARGB32); // straight alpha for the GPU, as shapes
}

QImage renderShape(const QVariantMap &st, int width, int height)
{
	const int w = std::clamp(width, 1, 4096), h = std::clamp(height, 1, 4096);
	QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHint(QPainter::Antialiasing);
	const QString shape = st.value("shape", "rect").toString();
	const QColor fill = colorOf(st.value("fill")), stroke = colorOf(st.value("stroke"));
	const double sw = std::max(0.0, st.value("strokeWidth", 6).toDouble());
	QPen pen = stroke.isValid() && sw > 0 ? QPen(stroke, sw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin) : QPen(Qt::NoPen);
	const double in = pen.style() == Qt::NoPen ? 0 : sw / 2; // no stroke: the shape fills its box
	const QRectF box = QRectF(0, 0, w, h).adjusted(in, in, -in, -in);
	if (shape == "rect" || shape == "ellipse") {
		p.setPen(pen);
		p.setBrush(fill.isValid() ? QBrush(fill) : QBrush(Qt::NoBrush));
		if (shape == "rect") {
			const double r = std::min(st.value("radius", 12).toDouble(), std::min(box.width(), box.height()) / 2);
			p.drawRoundedRect(box, r, r);
		} else {
			p.drawEllipse(box);
		}
	} else if (shape == "arrow") {
		const QVector<QPointF> pts = pointsOf(st, w, h);
		if (pts.size() >= 2 && stroke.isValid()) {
			const QPointF a = pts[0], b = pts[1];
			const double head = std::max(20.0, sw * 3.8), ang = std::atan2(b.y() - a.y(), b.x() - a.x());
			const QPointF back = b - QPointF(std::cos(ang), std::sin(ang)) * head * 0.8;
			p.setPen(QPen(stroke, std::max(1.0, sw), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.drawLine(a, back);
			QPolygonF tip;
			tip << b << b - QPointF(std::cos(ang - 0.45), std::sin(ang - 0.45)) * head << b - QPointF(std::cos(ang + 0.45), std::sin(ang + 0.45)) * head;
			p.setPen(QPen(stroke, std::max(1.0, sw * 0.6), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.setBrush(stroke);
			p.drawPolygon(tip);
		}
	} else if (shape == "path") {
		p.setPen(QPen(stroke.isValid() ? stroke : QColor(Qt::white), std::max(1.0, sw), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		p.setBrush(Qt::NoBrush);
		p.drawPath(smoothPath(pointsOf(st, w, h)));
	}
	p.end();
	return img.convertToFormat(QImage::Format_ARGB32); // straight alpha for the GPU
}

QImage renderLaser(const QVector<LaserPoint> &pts, qint64 now, int lifeMs, int cw, int ch, bool *anyLeft, QColor color)
{
	*anyLeft = false;
	for (const LaserPoint &lp : pts)
		if (now - lp.timeMs < lifeMs)
			*anyLeft = true;
	if (!*anyLeft)
		return {};
	const QColor glowC = color.isValid() ? color : QColor(255, 60, 60);
	const QColor coreC = QColor::fromRgbF(glowC.redF() * 0.55f + 0.45f, glowC.greenF() * 0.55f + 0.45f, glowC.blueF() * 0.55f + 0.45f);
	// Each pass is drawn fully opaque into its own layer, then the layer is laid down at one
	// opacity. Overlapping round caps of a translucent pen would bead up into dots otherwise.
	// The trail fades by tapering: older parts get thinner until they vanish.
	auto pass = [&](const QColor &c, double maxW) {
		QImage layer(cw, ch, QImage::Format_ARGB32_Premultiplied);
		layer.fill(Qt::transparent);
		QPainter p(&layer);
		p.setRenderHint(QPainter::Antialiasing);
		for (int i = 1; i < pts.size(); ++i) {
			if (pts[i].newStroke)
				continue;
			const qint64 age = now - pts[i].timeMs;
			if (age >= lifeMs)
				continue;
			const double k = std::pow(1.0 - double(age) / lifeMs, 0.6);
			// Smooth: a quadratic from the previous midpoint to this one, bent through point i-1.
			const QPointF a = pts[i - 1].at, b = pts[i].at;
			const bool hasPrev = i >= 2 && !pts[i - 1].newStroke;
			const QPointF from = hasPrev ? (pts[i - 2].at + a) / 2 : a;
			const QPointF to = (a + b) / 2;
			QPainterPath seg(from);
			seg.quadTo(a, to);
			if (i == pts.size() - 1 || pts[i + 1].newStroke)
				seg.lineTo(b); // the head reaches the pointer
			p.setPen(QPen(c, std::max(0.5, maxW * k), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			p.drawPath(seg);
		}
		return layer;
	};
	QImage img(cw, ch, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setOpacity(0.32);
	p.drawImage(0, 0, pass(glowC, 20));
	p.setOpacity(1);
	p.drawImage(0, 0, pass(coreC, 6));
	// The pointer itself, while the latest point is fresh.
	if (!pts.isEmpty() && now - pts.last().timeMs < 150) {
		p.setRenderHint(QPainter::Antialiasing);
		p.setPen(Qt::NoPen);
		QColor g = glowC;
		g.setAlphaF(0.35f);
		p.setBrush(g);
		p.drawEllipse(pts.last().at, 14, 14);
		p.setBrush(coreC.lighter(115));
		p.drawEllipse(pts.last().at, 5, 5);
	}
	p.end();
	return img.convertToFormat(QImage::Format_ARGB32);
}
