#include "app_internal.h"
#include "permissions.h"
#include "shapes.h"

#include <obs.h>
#include <util/base.h>

#include <QElapsedTimer>
#include <QFont>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineF>
#include <QPainter>
#include <QSet>
#include <algorithm>
#include <cmath>

using namespace lobs;

static Transform dropTransform(const Doc &d, const QString &sourceId, QPointF c)
{
	const Source src = d.sources.value(sourceId);
	const PropMap p = d.defaultProps(&src);
	Transform t = p.transform;
	if (t.w >= d.canvasW && t.h >= d.canvasH)
		return t;
	t.x = std::round(c.x() - t.w / 2);
	t.y = std::round(c.y() - t.h / 2);
	return t;
}

void App::poolDragMove(const QString &sourceId, double x, double y)
{
	if (!doc().sources.contains(sourceId) || !kindInfo(doc().sources[sourceId].kind).video)
		return;
	const Transform t = dropTransform(doc(), sourceId, toCanvas({x, y}));
	OverlayBox b;
	b.rect = QRectF(t.x, t.y, t.w, t.h);
	b.drop = true;
	m_dropBoxes = {b};
	updateOverlay();
}

void App::poolDrop(const QString &sourceId, double x, double y)
{
	if (!liveSceneEditAllowed(m_editScene))
		return;
	m_dropBoxes.clear();
	if (!doc().sources.contains(sourceId) || !doc().scenes.contains(m_editScene))
		return updateOverlay();
	if (!kindInfo(doc().sources[sourceId].kind).video) {
		updateOverlay();
		return showToast("Audio sources can't go on the canvas");
	}
	if (nestsSceneInItself(sourceId, m_editScene))
		return updateOverlay();
	const Transform t = dropTransform(doc(), sourceId, toCanvas({x, y}));
	QString iid;
	m_store.commit("Place " + doc().sources[sourceId].name, [&](Doc &d) {
		iid = d.placeSource(m_editScene, sourceId).id;
		d.setTransform(iid, t);
	});
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
}

void App::poolDragCancel()
{
	m_dropBoxes.clear();
	updateOverlay();
}

void App::addGraphic(const QString &kind)
{
	if (kind == "text")
		addSource("text", "Text", "Text", true);
	else
		addSource("color", "Color", "#1d1b2e", true);
}

double App::canvasScale() const
{
	return previewLayout().scale;
}

PreviewLayout App::previewLayout() const
{
	const double d = m_previewDpr;
	const QString other = verticalOn() ? otherLayoutId() : QString();
	const QSize oc = other.isEmpty() ? QSize() : doc().canvasOf(other);
	PreviewLayout l = previewLayoutFor(m_previewSize.width() * d, m_previewSize.height() * d, editCanvas().width(), editCanvas().height(),
					   oc.width(), oc.height(), editingVertical(), 16 * d, m_pixelPerfect);
	l.scale /= d, l.ox = std::round(l.ox) / d, l.oy = std::round(l.oy) / d;
	l.other = QRectF(l.other.x() / d, l.other.y() / d, l.other.width() / d, l.other.height() / d);
	return l;
}

QPointF App::toCanvas(QPointF p) const
{
	const PreviewLayout l = previewLayout();
	return {(p.x() - l.ox) / l.scale, (p.y() - l.oy) / l.scale};
}

QPointF App::fromCanvas(QPointF c) const
{
	const PreviewLayout l = previewLayout();
	return {c.x() * l.scale + l.ox, c.y() * l.scale + l.oy};
}

QString App::topAncestor(const QString &id) const
{
	QString cur = id;
	while (const Instance *p = doc().parentOf(cur))
		cur = p->id;
	return cur;
}

bool App::isLockedLayer(const QString &id) const
{
	if (doc().instances.value(id).locked)
		return true;
	for (const Instance *p = doc().parentOf(id); p; p = doc().parentOf(p->id))
		if (p->locked)
			return true;
	return false;
}

QStringList App::layersIn(const QRectF &r) const
{
	QStringList out;
	const auto leaves = doc().flatten(m_editScene);
	for (int k = leaves.size() - 1; k >= 0; k--) {
		const Instance &i = doc().instances[leaves[k].id];
		if (!leaves[k].visible || !kindInfo(doc().sources.value(i.sourceId).kind).video || isLockedLayer(i.id))
			continue;
		const Transform t = doc().transform(i);
		if (!QRectF(t.x, t.y, t.w, t.h).intersects(r))
			continue;
		QString top = i.id;
		for (const Instance *p = doc().parentOf(top); p; p = doc().parentOf(p->id))
			top = p->id;
		if (!out.contains(top))
			out << top;
	}
	return out;
}

QString App::hitTest(QPointF c) const
{
	const auto leaves = doc().flatten(m_editScene);
	for (int k = leaves.size() - 1; k >= 0; k--) {
		const Instance &i = doc().instances[leaves[k].id];
		if (!leaves[k].visible || !kindInfo(doc().sources.value(i.sourceId).kind).video || isLockedLayer(i.id))
			continue;
		const Transform t = doc().transform(i);
		if (QRectF(t.x, t.y, t.w, t.h).contains(c))
			return i.id;
	}
	return {};
}

int App::hitHandle(QPointF c) const
{
	const QString id = singleSelection();
	if (id.isEmpty() || doc().instances[id].kind == "group")
		return -1;
	const Transform t = doc().transform(doc().instances[id]);
	const double r = 9 / canvasScale();
	if (m_cropMode) {
		const double cr = 12 / canvasScale();
		const bool nl = std::abs(c.x() - t.x) <= cr, nr = std::abs(c.x() - (t.x + t.w)) <= cr;
		const bool nt = std::abs(c.y() - t.y) <= cr, nb = std::abs(c.y() - (t.y + t.h)) <= cr;
		const bool inX = c.x() > t.x - cr && c.x() < t.x + t.w + cr, inY = c.y() > t.y - cr && c.y() < t.y + t.h + cr;
		if (nl && nt) return 14;
		if (nr && nt) return 15;
		if (nl && nb) return 16;
		if (nr && nb) return 17;
		if (nl && inY) return 10;
		if (nt && inX) return 11;
		if (nr && inY) return 12;
		if (nb && inX) return 13;
		return -1;
	}
	const QPointF corners[] = {{t.x, t.y}, {t.x + t.w, t.y}, {t.x, t.y + t.h}, {t.x + t.w, t.y + t.h}};
	for (int k = 0; k < 4; k++)
		if (std::abs(c.x() - corners[k].x()) <= r && std::abs(c.y() - corners[k].y()) <= r)
			return k;
	const double e = 6 / canvasScale();
	const bool inX = c.x() > t.x + r && c.x() < t.x + t.w - r, inY = c.y() > t.y + r && c.y() < t.y + t.h - r;
	if (inY && std::abs(c.x() - t.x) <= e) return 4;
	if (inX && std::abs(c.y() - t.y) <= e) return 5;
	if (inY && std::abs(c.x() - (t.x + t.w)) <= e) return 6;
	if (inX && std::abs(c.y() - (t.y + t.h)) <= e) return 7;
	return -1;
}

Qt::CursorShape App::cursorAt(QPointF p) const
{
	if (m_tool == "text" || textEditing())
		return Qt::IBeamCursor;
	if (m_tool != "select")
		return Qt::CrossCursor;
	const int h = hitHandle(toCanvas(p));
	if (h == 10 || h == 12 || h == 4 || h == 6) return Qt::SizeHorCursor;
	if (h == 11 || h == 13 || h == 5 || h == 7) return Qt::SizeVerCursor;
	if (h == 14 || h == 17) return Qt::SizeFDiagCursor;
	if (h == 15 || h == 16) return Qt::SizeBDiagCursor;
	if (h == 0 || h == 3) return Qt::SizeFDiagCursor;
	if (h == 1 || h == 2) return Qt::SizeBDiagCursor;
	return hitTest(toCanvas(p)).isEmpty() ? Qt::ArrowCursor : Qt::SizeAllCursor;
}

void App::canvasPress(QPointF p, Qt::KeyboardModifiers mods, bool dbl)
{
	mods |= QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier;
	endTextEdit();
	if (verticalOn() && !otherLayoutId().isEmpty() && previewLayout().other.contains(p))
		setEditingVertical(!editingVertical());
	const QPointF c = toCanvas(p);
	if (m_tool == "laser") {
		m_drag = Drag::Laser;
		m_engine->setLaserLife(m_laserTrailMs);
		if (QColor::isValidColorName(m_toolFill))
			m_engine->setLaserColor(QColor(m_toolFill));
		m_engine->laserPoint(c, true);
		return;
	}
	if (m_tool != "select") {
		if (!doc().scenes.contains(m_editScene))
			return;
		if (liveLocked() && !(mods & Qt::AltModifier) && m_editScene == m_engine->programScene())
			return showToast("On-air layers are locked. Hold Alt to add graphics.");
		m_drag = Drag::Create;
		m_createPts = {c};
		return;
	}
	m_dragStart = c;
	m_dragBase = doc();
	m_handle = hitHandle(c);
	const bool locked = liveLocked() && !(mods & Qt::AltModifier);
	if (m_handle >= 0 && locked) {
		m_drag = Drag::None;
		return showToast("On-air layers are locked. Hold Alt to resize.");
	}
	if (m_handle >= 10) {
		if (doc().instances[singleSelection()].locked)
			return showToast("Layer is locked");
		m_drag = Drag::Crop;
		m_dragIds = {singleSelection()};
		return;
	}
	if (m_cropMode && m_handle < 0 && hitTest(c) == singleSelection()) {
		if (doc().instances[singleSelection()].locked)
			return showToast("Layer is locked");
		m_drag = Drag::CropPan;
		m_dragIds = {singleSelection()};
		return;
	}
	if (m_cropMode && hitTest(c) != singleSelection())
		setCropMode(false);
	if (m_handle >= 0) {
		if (doc().instances[singleSelection()].locked)
			return showToast("Layer is locked");
		m_drag = Drag::Resize;
		m_dragIds = {singleSelection()};
		return;
	}
	const QString leaf = hitTest(c);
	if (leaf.isEmpty()) {
		m_marqueeBase = (mods & Qt::ShiftModifier) ? m_selection : QStringList();
		m_selection = m_marqueeBase;
		m_marquee = QRectF(c, c);
		m_drag = Drag::Marquee;
		updateOverlay();
		emit docChanged();
		return;
	}
	QStringList path{leaf};
	for (const Instance *p2 = doc().parentOf(leaf); p2; p2 = doc().parentOf(p2->id))
		path.prepend(p2->id);
	int level = -1;
	for (int k = 0; k < path.size(); ++k)
		if (m_selection.contains(path[k]))
			level = k;
	const QString target = dbl ? path[std::min(int(path.size()) - 1, level + 1)] : path[std::max(0, level)];
	if (dbl && level == int(path.size()) - 1 && m_selection == QStringList{leaf} && doc().instances[leaf].kind != "group") {
		setCropMode(true);
		if (m_cropMode) {
			m_drag = Drag::None;
			return;
		}
	}
	if (dbl)
		m_selection = {target};
	if (mods & Qt::ShiftModifier) {
		selectLayer(target, true);
		m_drag = Drag::None;
		return;
	}
	if (!m_selection.contains(target))
		m_selection = {target};
	m_poolSelection.clear();
	m_dragIds.clear();
	for (const QString &id : m_selection) {
		bool layerLocked = false;
		for (const QString &d : doc().descendants(id))
			layerLocked |= doc().instances[d].locked;
		if (!layerLocked)
			m_dragIds << id;
	}
	m_drag = m_dragIds.isEmpty() ? Drag::None : Drag::Move;
	if (m_dragIds.isEmpty())
		showToast("Layer is locked");
	else if (locked) {
		m_drag = Drag::None;
		showToast("On-air layers are locked. Hold Alt to move.");
	}
	updateOverlay();
	emit docChanged();
}

void App::canvasMove(QPointF p, Qt::KeyboardModifiers mods)
{
	mods |= QGuiApplication::queryKeyboardModifiers() & Qt::AltModifier;
	QElapsedTimer perf;
	perf.start();
	struct PerfScope {
		App *a; QElapsedTimer &t;
		~PerfScope() { const qint64 us = t.nsecsElapsed() / 1000; a->m_perfMoves++; a->m_perfTotalUs += us; a->m_perfMaxUs = std::max(a->m_perfMaxUs, us); }
	} perfScope{this, perf};
	if (m_drag == Drag::None)
		return;
	const QPointF c = toCanvas(p);
	if (m_drag == Drag::Laser)
		return m_engine->laserPoint(c, false);
	if (m_drag == Drag::Marquee) {
		m_marquee = QRectF(m_dragStart, c).normalized();
		QStringList sel = m_marqueeBase;
		for (const QString &id : layersIn(m_marquee))
			if (!sel.contains(id))
				sel << id;
		const bool changed = sel != m_selection;
		m_selection = sel;
		updateOverlay();
		if (changed) {
			m_poolSelection.clear();
			emit docChanged();
		}
		return;
	}
	if (m_drag == Drag::Create) {
		if (m_tool == "pencil") {
			if (QLineF(m_createPts.last(), c).length() >= 1.5)
				m_createPts << c;
			m_engine->setInk(m_createPts, m_toolStroke, std::max(2, m_toolStrokeWidth));
		} else if (m_tool == "text") {
			m_createPts = {m_createPts.first(), c};
			const QPointF a = m_createPts.first();
			const double size = std::max(12.0, std::abs(c.y() - a.y()));
			drawTextGhost(QPointF(a.x(), c.y() < a.y() ? a.y() - size : a.y()), size);
		} else {
			QPointF e = c;
			if (mods & Qt::ShiftModifier) {
				const QPointF d = c - m_createPts.first();
				if (m_tool == "arrow") {
					const double ang = std::round(std::atan2(d.y(), d.x()) / (M_PI / 4)) * (M_PI / 4), len = std::hypot(d.x(), d.y());
					e = m_createPts.first() + QPointF(std::cos(ang), std::sin(ang)) * len;
				} else {
					const double m = std::max(std::abs(d.x()), std::abs(d.y()));
					e = m_createPts.first() + QPointF(d.x() < 0 ? -m : m, d.y() < 0 ? -m : m);
				}
			}
			m_createPts = {m_createPts.first(), e};
			const double sw = m_toolStrokeWidth;
			if (m_tool == "arrow") {
				const QPointF a = m_createPts[0], b = m_createPts[1];
				const double pad = std::max(20.0, std::max(1.0, sw) * 3.8) / 2 + 2;
				const QRectF r = QRectF(a, b).normalized().adjusted(-pad, -pad, pad, pad);
				const QVariantMap st{{"shape", "arrow"}, {"stroke", m_toolStroke}, {"strokeWidth", std::max(1.0, sw)},
						     {"points", QVariantList{QVariantList{(a.x() - r.x()) / r.width(), (a.y() - r.y()) / r.height()},
									      QVariantList{(b.x() - r.x()) / r.width(), (b.y() - r.y()) / r.height()}}}};
				m_engine->setGhost(renderShape(st, int(std::ceil(r.width())), int(std::ceil(r.height()))), r.topLeft());
			} else {
				const QRectF r = QRectF(m_createPts[0], m_createPts[1]).normalized();
				if (r.width() >= 2 && r.height() >= 2) {
					const QVariantMap st{{"shape", m_tool}, {"fill", m_toolFill}, {"stroke", sw > 0 ? m_toolStroke : QString()}, {"strokeWidth", sw}, {"radius", 0}};
					m_engine->setGhost(renderShape(st, int(std::round(r.width())), int(std::round(r.height()))), QPointF(std::round(r.x()), std::round(r.y())));
				}
			}
		}
		return;
	}
	if (m_drag == Drag::Crop) {
		const QString id = m_dragIds.value(0);
		const Instance &bi = m_dragBase.instances[id];
		const QSize ns = m_engine->sourceSize(bi.sourceId);
		if (ns.isEmpty())
			return;
		const Transform bt = m_dragBase.transform(bi);
		const Crop bc = m_dragBase.crop(bi);
		const double sx = bt.w / std::max(1, ns.width() - bc.l - bc.r), sy = bt.h / std::max(1, ns.height() - bc.t - bc.b);
		const int dl = int(std::lround((c.x() - m_dragStart.x()) / sx)), dt = int(std::lround((c.y() - m_dragStart.y()) / sy));
		Crop nc = bc;
		const int h = m_handle;
		int &left = bt.flipH ? nc.r : nc.l, &right = bt.flipH ? nc.l : nc.r;
		int &top = bt.flipV ? nc.b : nc.t, &bottom = bt.flipV ? nc.t : nc.b;
		const int left0 = left, right0 = right, top0 = top, bottom0 = bottom;
		if (h == 10 || h == 14 || h == 16) left = left0 + dl;
		if (h == 12 || h == 15 || h == 17) right = right0 - dl;
		if (h == 11 || h == 14 || h == 15) top = top0 + dt;
		if (h == 13 || h == 16 || h == 17) bottom = bottom0 - dt;
		double ratio = cropAspect();
		if (mods & Qt::ShiftModifier)
			ratio = bt.w / std::max(1.0, bt.h);
		if (ratio > 0) {
			const double W = ns.width(), H = ns.height(), rs = ratio * sy / sx;
			const bool corner = h >= 14, horiz = h == 10 || h == 12;
			const bool movesLeft = h == 10 || h == 14 || h == 16, movesTop = h == 11 || h == 14 || h == 15;
			double vw = W - left - right, vh = H - top - bottom;
			if (corner ? std::abs(dl * sx) >= std::abs(dt * sy) : horiz)
				vh = vw / rs;
			else
				vw = vh * rs;
			const double roomW = corner || horiz ? W - (movesLeft ? right0 : left0) : W;
			const double roomH = corner || !horiz ? H - (movesTop ? bottom0 : top0) : H;
			if (vw > roomW) vw = roomW, vh = vw / rs;
			if (vh > roomH) vh = roomH, vw = vh * rs;
			vw = std::max(1.0, vw), vh = std::max(1.0, vh);
			auto place = [](int &a, int &b, int a0, int b0, double total, double v, int mode) {
				if (mode == 1) { b = b0; a = int(std::lround(total - b0 - v)); }
				else if (mode == 2) { a = a0; b = int(std::lround(total - a0 - v)); }
				else {
					const double mid = (a0 + (total - b0)) / 2;
					double s = std::clamp(mid - v / 2, 0.0, total - v);
					a = int(std::lround(s)); b = int(std::lround(total - s - v));
				}
			};
			place(left, right, left0, right0, W, vw, corner || horiz ? (movesLeft ? 1 : 2) : 0);
			place(top, bottom, top0, bottom0, H, vh, corner || !horiz ? (movesTop ? 1 : 2) : 0);
		}
		m_store.gesture([&](Doc &d) { cropKeepingScale(d, id, nc, ns); });
		updateOverlay();
		return;
	}
	if (m_drag == Drag::CropPan) {
		const QString id = m_dragIds.value(0);
		const Instance &bi = m_dragBase.instances[id];
		const QSize ns = m_engine->sourceSize(bi.sourceId);
		if (ns.isEmpty())
			return;
		const Transform bt = m_dragBase.transform(bi);
		const Crop bc = m_dragBase.crop(bi);
		const double sx = bt.w / std::max(1, ns.width() - bc.l - bc.r), sy = bt.h / std::max(1, ns.height() - bc.t - bc.b);
		int dl = int(std::lround((c.x() - m_dragStart.x()) / sx)), dt = int(std::lround((c.y() - m_dragStart.y()) / sy));
		Crop nc = bc;
		int &left = bt.flipH ? nc.r : nc.l, &right = bt.flipH ? nc.l : nc.r;
		int &top = bt.flipV ? nc.b : nc.t, &bottom = bt.flipV ? nc.t : nc.b;
		const int l0 = left, r0 = right, t0 = top, b0 = bottom;
		dl = std::clamp(dl, -l0, r0), dt = std::clamp(dt, -t0, b0);
		left = l0 + dl, right = r0 - dl, top = t0 + dt, bottom = b0 - dt;
		m_store.gesture([&](Doc &d) { cropKeepingScale(d, id, nc, ns); });
		updateOverlay();
		return;
	}
	const Doc &base = m_dragBase;
	m_guides.clear();
	if (m_drag == Drag::Move) {
		double dx = c.x() - m_dragStart.x(), dy = c.y() - m_dragStart.y();
		if (!(mods & Qt::ControlModifier)) {
			QSet<QString> moving;
			double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
			for (const QString &id : m_dragIds) {
				for (const QString &d : base.descendants(id))
					moving.insert(d);
				const Transform b = base.bounds(id);
				x0 = std::min(x0, b.x), y0 = std::min(y0, b.y), x1 = std::max(x1, b.x + b.w), y1 = std::max(y1, b.y + b.h);
			}
			const QSize ec = base.canvasOf(m_editScene);
			QVector<double> xs{0, ec.width() / 2.0, double(ec.width())}, ys{0, ec.height() / 2.0, double(ec.height())};
			for (const auto &leaf : base.flatten(m_editScene)) {
				if (moving.contains(leaf.id))
					continue;
				const Transform t = base.transform(base.instances[leaf.id]);
				xs << t.x << t.x + t.w / 2 << t.x + t.w;
				ys << t.y << t.y + t.h / 2 << t.y + t.h;
			}
			const double th = 8 / canvasScale();
			auto snap = [&](QVector<double> edges, const QVector<double> &lines, bool vertical) {
				double best = 1e9, at = 0;
				for (double e : edges)
					for (double l : lines)
						if (std::abs(l - e) < th && std::abs(l - e) < std::abs(best))
							best = l - e, at = l;
				if (best < 1e8) {
					m_guides << OverlayGuide{vertical, at};
					return best;
				}
				return 0.0;
			};
			dx += snap({x0 + dx, (x0 + x1) / 2 + dx, x1 + dx}, xs, true);
			dy += snap({y0 + dy, (y0 + y1) / 2 + dy, y1 + dy}, ys, false);
		}
		const QStringList ids = m_dragIds;
		m_store.gesture([&](Doc &d) {
			QSet<QString> seen;
			for (const QString &id : ids)
				d.moveBy(id, dx, dy, seen);
		});
	} else {
		const QString id = m_dragIds.first();
		const Transform t0 = base.transform(base.instances[id]);
		const bool page = pageFollowsLayer(base, id);
		const QString kind = base.sources.value(base.instances[id].sourceId).kind;
		const bool free = page || kind == "browser" || kind == "shape";
		if (m_handle >= 4) {
			const bool centered = mods & Qt::AltModifier, horiz = m_handle == 4 || m_handle == 6;
			const double aspect = t0.w / std::max(1.0, t0.h);
			Transform t = t0;
			if (horiz) {
				const double fixed = centered ? t0.x + t0.w / 2 : m_handle == 4 ? t0.x + t0.w : t0.x;
				const double w = std::max(8.0, std::abs(c.x() - fixed) * (centered ? 2 : 1));
				t.x = std::round(centered ? fixed - w / 2 : m_handle == 4 ? fixed - w : fixed), t.w = std::round(w);
				if (!free) {
					const double h = w / aspect, cy = t0.y + t0.h / 2;
					t.y = std::round(cy - h / 2), t.h = std::round(h);
				}
			} else {
				const double fixed = centered ? t0.y + t0.h / 2 : m_handle == 5 ? t0.y + t0.h : t0.y;
				const double h = std::max(8.0, std::abs(c.y() - fixed) * (centered ? 2 : 1));
				t.y = std::round(centered ? fixed - h / 2 : m_handle == 5 ? fixed - h : fixed), t.h = std::round(h);
				if (!free) {
					const double w = h * aspect, cx = t0.x + t0.w / 2;
					t.x = std::round(cx - w / 2), t.w = std::round(w);
				}
			}
			m_store.gesture([&](Doc &d) { d.setTransform(id, t); if (page) sizePage(d, id, t); });
			return;
		}
		const bool left = m_handle == 0 || m_handle == 2, top = m_handle == 0 || m_handle == 1;
		const bool centered = mods & Qt::AltModifier;
		const double ax = centered ? t0.x + t0.w / 2 : left ? t0.x + t0.w : t0.x, ay = centered ? t0.y + t0.h / 2 : top ? t0.y + t0.h : t0.y;
		const double k = centered ? 2 : 1;
		double w = std::max(8.0, std::abs(c.x() - ax) * k), h = std::max(8.0, std::abs(c.y() - ay) * k);
		if (!(mods & Qt::ShiftModifier) != free) {
			const double aspect = t0.w / std::max(1.0, t0.h);
			if (w / h > aspect) h = w / aspect;
			else w = h * aspect;
		}
		Transform t = t0;
		t.x = std::round(centered ? ax - w / 2 : left ? ax - w : ax), t.y = std::round(centered ? ay - h / 2 : top ? ay - h : ay), t.w = std::round(w), t.h = std::round(h);
		m_store.gesture([&](Doc &d) { d.setTransform(id, t); if (page) sizePage(d, id, t); });
	}
}

void App::canvasRelease()
{
	if (m_perfMoves > 5)
		blog(LOG_INFO, "[perf] drag: %d moves, avg %.2f ms, max %.2f ms per move; engine frame %.2f ms; lagged %u",
		     m_perfMoves, m_perfTotalUs / 1000.0 / m_perfMoves, m_perfMaxUs / 1000.0, obs_get_average_frame_time_ns() / 1e6, obs_get_lagged_frames());
	if (m_perfMoves > 5)
		blog(LOG_INFO, "[perf]   per move: engine apply %.2f ms, edit scene %.2f ms, hotkeys %.2f ms, UI update %.2f ms",
		     m_perfApplyUs / 1000.0 / m_perfMoves, m_perfEditUs / 1000.0 / m_perfMoves, m_perfHotkeyUs / 1000.0 / m_perfMoves, m_perfQmlUs / 1000.0 / m_perfMoves);
	m_perfApplyUs = m_perfEditUs = m_perfHotkeyUs = m_perfQmlUs = 0;
	m_perfMoves = 0, m_perfTotalUs = 0, m_perfMaxUs = 0;
	if (m_drag == Drag::Laser) {
		m_drag = Drag::None;
		return;
	}
	if (m_drag == Drag::Marquee) {
		m_drag = Drag::None;
		m_marquee = {};
		m_marqueeBase.clear();
		updateOverlay();
		return;
	}
	if (m_drag == Drag::Create) {
		m_drag = Drag::None;
		finishCreate();
		return;
	}
	if (m_drag == Drag::Move)
		m_store.endGesture(m_dragIds.size() > 1 ? QString("Move %1 layers").arg(m_dragIds.size()) : "Move " + doc().instances.value(m_dragIds.value(0)).name);
	else if (m_drag == Drag::Resize)
		m_store.endGesture("Resize " + doc().instances.value(m_dragIds.value(0)).name);
	else if (m_drag == Drag::Crop || m_drag == Drag::CropPan)
		m_store.endGesture("Crop " + doc().instances.value(m_dragIds.value(0)).name);
	m_drag = Drag::None;
	if (m_modelsStale)
		refreshModels();
	m_guides.clear();
	updateOverlay();
}

void App::setTool(const QString &t)
{
	static const QStringList tools = {"select", "text", "rect", "ellipse", "arrow", "pencil", "laser"};
	if (!tools.contains(t) || t == m_tool)
		return;
	m_tool = t;
	m_penGroup.clear();
	if (t != "select")
		m_selection.clear(), updateOverlay(), emit docChanged();
	emit toolChanged();
}

void App::setToolFill(const QString &c)
{
	m_toolFill = c;
	setUiValue("tools/fill", c);
	if (m_engine && QColor::isValidColorName(c))
		m_engine->setLaserColor(QColor(c));
	emit toolChanged();
}

void App::setToolStroke(const QString &c) { m_toolStroke = c; setUiValue("tools/stroke", c); emit toolChanged(); }

void App::setLaserTrailMs(int ms)
{
	m_laserTrailMs = std::clamp(ms, 150, 3000);
	setUiValue("tools/laserTrailMs", QString::number(m_laserTrailMs));
	if (m_engine)
		m_engine->setLaserLife(m_laserTrailMs);
	emit toolChanged();
}

void App::setToolStrokeWidth(int w) { m_toolStrokeWidth = std::clamp(w, 0, 64); setUiValue("tools/strokeWidth", QString::number(m_toolStrokeWidth)); emit toolChanged(); }

void App::finishCreate()
{
	if (!liveSceneEditAllowed(m_editScene))
		return;
	m_engine->setInk({}, "#ffffff", 1);
	m_engine->setGhost({}, {});
	m_dropBoxes.clear();
	const QVector<QPointF> pts = m_createPts;
	m_createPts.clear();
	if (pts.isEmpty() || !doc().scenes.contains(m_editScene))
		return updateOverlay();
	const double sw = m_toolStrokeWidth;
	const QPointF a = pts.first(), b = pts.last();
	const bool clicked = QLineF(a, b).length() < 6 && pts.size() < 3;
	QString kind = "shape", label;
	QVariantMap st;
	Transform t;
	auto boxAround = [&](QPointF c, double w, double h) { t.x = std::round(c.x() - w / 2), t.y = std::round(c.y() - h / 2), t.w = w, t.h = h; };
	if (m_tool == "text") {
		kind = "text";
		label = "Text";
		st = {{"text", ""}, {"textColor", m_toolStroke}, {"font", m_toolFont}, {"bold", m_toolBold}, {"italic", m_toolItalic}};
		const double size = clicked ? 96 : std::max(12.0, std::round(std::abs(b.y() - a.y())));
		const double top = clicked ? a.y() - size / 2 : (b.y() < a.y() ? a.y() - size : a.y());
		t = {std::round(a.x()), std::round(top), 4, size, 0};
		m_engine->setGhost({}, {});
	} else if (m_tool == "rect" || m_tool == "ellipse") {
		label = m_tool == "rect" ? "Rectangle" : "Ellipse";
		st = {{"shape", m_tool}, {"fill", m_toolFill}, {"stroke", sw > 0 ? m_toolStroke : QString()}, {"strokeWidth", sw}, {"radius", 0}};
		if (clicked) boxAround(a, m_tool == "rect" ? 400 : 300, m_tool == "rect" ? 240 : 300);
		else { const QRectF r = QRectF(a, b).normalized(); t = {std::round(r.x()), std::round(r.y()), std::max(4.0, std::round(r.width())), std::max(4.0, std::round(r.height())), 0}; }
	} else if (m_tool == "arrow" || m_tool == "pencil") {
		label = m_tool == "arrow" ? "Arrow" : "Drawing";
		QVector<QPointF> line = m_tool == "arrow" ? QVector<QPointF>{a, clicked ? a + QPointF(300, 0) : b} : pts;
		const double pad = (m_tool == "arrow" ? std::max(20.0, std::max(1.0, sw) * 3.8) : std::max(1.0, sw)) / 2 + 2;
		double x0 = line.first().x(), y0 = line.first().y(), x1 = x0, y1 = y0;
		for (const QPointF &q : line)
			x0 = std::min(x0, q.x()), y0 = std::min(y0, q.y()), x1 = std::max(x1, q.x()), y1 = std::max(y1, q.y());
		const QRectF r(QPointF(x0 - pad, y0 - pad), QPointF(x1 + pad, y1 + pad));
		t = {std::floor(r.x()), std::floor(r.y()), std::ceil(r.width()), std::ceil(r.height()), 0};
		QVariantList norm;
		for (const QPointF &q : line)
			norm << QVariant(QVariantList{(q.x() - t.x) / t.w, (q.y() - t.y) / t.h});
		st = {{"shape", m_tool == "arrow" ? "arrow" : "path"}, {"stroke", m_toolStroke}, {"strokeWidth", std::max(1.0, sw)}, {"points", norm}};
	} else {
		return updateOverlay();
	}
	QString iid;
	const bool pen = m_tool == "pencil";
	m_store.commit("Add " + label.toLower(), [&](Doc &d) {
		const QString sid = d.addSource(kind, label, st).id;
		iid = d.placeSource(m_editScene, sid).id;
		d.setTransform(iid, t);
		if (!pen)
			return;
		if (const Instance g = d.instances.value(m_penGroup); g.kind == "group" && g.sceneId == m_editScene) {
			d.scenes[m_editScene].layers.removeAll(iid);
			d.instances[m_penGroup].children.prepend(iid);
		} else {
			m_penGroup = d.groupLayers({iid});
			QStringList taken;
			for (const auto &i : d.instances)
				if (i.id != iid && i.id != m_penGroup)
					taken << i.name;
			d.instances[m_penGroup].name = uniqueName("Drawing", taken);
		}
	});
	const bool keepsDrawing = pen || m_tool == "arrow";
	m_selection = keepsDrawing ? QStringList() : QStringList{iid};
	m_poolSelection.clear();
	if (m_tool == "text") {
		m_textEditId = iid;
		m_caretOn = true;
		m_caretTimer.start();
		layoutEditedText();
	}
	if (!keepsDrawing)
		m_tool = "select", emit toolChanged();
	updateOverlay();
	emit docChanged();
}

static QFont textFont(const QString &face, bool bold, bool italic, double px)
{
	QFont f(face);
	f.setPixelSize(std::max(1, int(std::lround(px))));
	f.setBold(bold);
	f.setItalic(italic);
	return f;
}

void App::drawTextGhost(QPointF at, double size)
{
	const QFontMetricsF base(textFont(m_toolFont, m_toolBold, m_toolItalic, size));
	const double em = size * size / std::max(1.0, base.lineSpacing());
	const QFont f = textFont(m_toolFont, m_toolBold, m_toolItalic, em);
	const QFontMetricsF fm(f);
	const int w = int(std::ceil(fm.horizontalAdvance("A"))) + 4, h = int(std::ceil(size));
	QImage img(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	QPainter p(&img);
	p.setRenderHint(QPainter::TextAntialiasing);
	QColor c = QColor::isValidColorName(m_toolStroke) ? QColor(m_toolStroke) : QColor(Qt::white);
	c.setAlphaF(0.8f);
	p.setPen(c);
	p.setFont(f);
	p.drawText(QPointF(1, (h - fm.height()) / 2 + fm.ascent()), "A");
	p.end();
	m_engine->setGhost(img, at);
}

void App::layoutEditedText()
{
	if (!doc().instances.contains(m_textEditId))
		return;
	const Instance &inst = doc().instances[m_textEditId];
	const Source &src = doc().sources[inst.sourceId];
	const QString text = src.settings.value("text").toString();
	const QStringList lines = text.split('\n');
	Transform t = doc().transform(inst);
	const double linePx = std::max(4.0, t.h / std::max<qsizetype>(1, lines.size()));
	const QFont f = textFont(src.settings.value("font", lobs::defaultTextFont()).toString(), src.settings.value("bold", true).toBool(), src.settings.value("italic").toBool(), linePx);
	const QFontMetricsF fm(f);
	const double k = linePx / std::max(1.0, fm.lineSpacing());
	double wmax = 0;
	for (const QString &l : lines)
		wmax = std::max(wmax, fm.horizontalAdvance(l));
	Transform nt = t;
	nt.w = std::max(4.0, std::ceil(wmax * k));
	nt.h = std::round(linePx * lines.size());
	if (nt.w != t.w || nt.h != t.h) {
		const QString id = m_textEditId;
		m_store.amend([&](Doc &d) { d.setTransform(id, nt); });
		updateOverlay();
	}
	if (m_caretOn) {
		const double lastW = fm.horizontalAdvance(lines.last()) * k;
		const int cw = std::max(2, int(std::lround(linePx / 28))), ch = std::max(4, int(std::lround(linePx * 0.86)));
		QImage caret(cw, ch, QImage::Format_ARGB32);
		const QString col = src.settings.value("textColor", "#ffffff").toString();
		caret.fill(QColor::isValidColorName(col) ? QColor(col) : QColor(Qt::white));
		m_engine->setGhost(caret, QPointF(nt.x + std::min(lastW, nt.w) + 2, nt.y + linePx * (lines.size() - 1) + (linePx - ch) / 2));
	} else {
		m_engine->setGhost({}, {});
	}
}

bool App::textKey(QKeyEvent *e)
{
	if (!textEditing())
		return false;
	if (!doc().instances.contains(m_textEditId)) {
		endTextEdit();
		return false;
	}
	const bool ctrl = e->modifiers() & Qt::ControlModifier;
	if (e->key() == Qt::Key_Escape || (ctrl && (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter))) {
		endTextEdit();
		return true;
	}
	if (ctrl || (e->modifiers() & Qt::AltModifier)) {
		endTextEdit();
		return false;
	}
	const QString sid = doc().instances[m_textEditId].sourceId;
	QString text = doc().sources[sid].settings.value("text").toString();
	const QString before = text;
	if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
		text += '\n';
	else if (e->key() == Qt::Key_Backspace)
		text.chop(1);
	else if (!e->text().isEmpty() && e->text()[0].isPrint())
		text += e->text();
	else
		return true;
	if (text != before) {
		const Transform t = doc().transform(doc().instances[m_textEditId]);
		const double linePx = t.h / std::max<qsizetype>(1, before.count('\n') + 1);
		Transform nt = t;
		nt.h = std::round(linePx * (text.count('\n') + 1));
		const QString id = m_textEditId;
		m_store.amend([&](Doc &d) {
			d.sources[sid].settings["text"] = text;
			d.setTransform(id, nt);
		});
	}
	m_caretOn = true;
	m_caretTimer.start();
	layoutEditedText();
	emit docChanged();
	return true;
}

void App::endTextEdit()
{
	if (!textEditing())
		return;
	m_caretTimer.stop();
	m_caretOn = false;
	m_engine->setGhost({}, {});
	const QString id = m_textEditId;
	m_textEditId.clear();
	if (!doc().instances.contains(id))
		return;
	const QString sid = doc().instances[id].sourceId;
	if (doc().sources.value(sid).settings.value("text").toString().trimmed().isEmpty()) {
		m_store.amend([&](Doc &d) {
			d.deleteInstance(id);
			d.sources.remove(sid);
			d.sourceOrder.removeAll(sid);
		});
		m_selection.removeAll(id);
		updateOverlay();
		emit docChanged();
	}
}

void App::updateOverlay()
{
	if (m_cropMode && (m_selection.size() != 1 || !doc().instances.contains(m_selection.first())))
		m_cropMode = false;
	QVector<OverlayBox> boxes;
	for (const QString &id : m_selection) {
		if (!doc().instances.contains(id) || doc().instances[id].sceneId != m_editScene)
			continue;
		const Instance &i = doc().instances[id];
		const Transform b = doc().bounds(id);
		OverlayBox ob{QRectF(b.x, b.y, b.w, b.h), b.rot, m_selection.size() == 1 && i.kind != "group", false, i.kind == "group", i.links.contains(Group::Transform)};
		ob.crop = m_cropMode && m_selection.size() == 1;
		boxes << ob;
	}
	boxes << m_dropBoxes;
	if (m_drag == Drag::Marquee && (m_marquee.width() > 0 || m_marquee.height() > 0)) {
		OverlayBox mb{m_marquee};
		mb.drop = true, mb.marquee = true;
		boxes << mb;
	}
	m_engine->setOverlay(boxes, m_guides);
	updateBlockedLayers();
}

void App::updateBlockedLayers()
{
	QVector<BlockedLayer> blocked;
	for (const Instance &i : doc().instances) {
		if (i.sceneId != m_editScene || i.kind == "group" || !doc().visibility(i).visible)
			continue;
		const QString kind = doc().sources.value(i.sourceId).kind;
		if (Permissions::allows(kind))
			continue;
		const Transform b = doc().bounds(i.id);
		blocked << BlockedLayer{QRectF(b.x, b.y, b.w, b.h), b.rot, "Needs " + Permissions::title(Permissions::forKind(kind))};
	}
	m_engine->setBlockedLayers(blocked);
}
