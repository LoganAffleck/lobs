// Vector graphics for shape layers and the laser pointer, drawn with QPainter.
// Shape settings (a "shape" source): shape = rect | ellipse | arrow | path, fill ("" = none),
// stroke ("" = none), strokeWidth (canvas px), radius (rect corners), points ([[x, y]...] in
// 0..1 of the layer box: arrow start/end, or the pencil stroke).
#pragma once

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QVariantMap>
#include <QVector>

QImage renderShape(const QVariantMap &settings, int width, int height);

// A text layer (text, font, bold, italic, textColor), laid out as the editor lays it out: the box
// is (lines x line height) tall, the font's em is the line height, and the text is scaled so its
// line spacing fills one line; its width fills the box. lineHeight <= 0 takes it from height;
// width <= 0 makes the box as wide as the text.
QImage renderText(const QVariantMap &settings, int width, int height, double lineHeight = 0);

struct LaserPoint {
	QPointF at;     // canvas px
	qint64 timeMs;  // when it was drawn
	bool newStroke; // starts a new stroke (pen lifted before it)
};
// The trail, fading out over `lifeMs`; returns an empty image when nothing is left.
QImage renderLaser(const QVector<LaserPoint> &points, qint64 nowMs, int lifeMs, int canvasW, int canvasH, bool *anyLeft, QColor color = QColor(255, 60, 60));
