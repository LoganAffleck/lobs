// image://icon/<name>?c=<hex>&w=<stroke> — Lucide icons (ISC), bundled as SVGs in <root>/icons.
// The SVG's currentColor and stroke width are substituted, then it's rendered at the exact
// requested pixel size, so icons stay sharp at any size and DPI and can take any theme color.
#pragma once

#include <QFile>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QQuickImageProvider>
#include <QSvgRenderer>
#include <QUrlQuery>

class IconProvider : public QQuickImageProvider {
public:
	explicit IconProvider(const QString &dir) : QQuickImageProvider(QQuickImageProvider::Image), m_dir(dir) {}

	QImage requestImage(const QString &id, QSize *size, const QSize &requested) override
	{
		const QString name = id.section('?', 0, 0);
		const QUrlQuery q(id.section('?', 1));
		const QString color = "#" + q.queryItemValue("c", QUrl::FullyDecoded).remove('#');
		const QString stroke = q.hasQueryItem("w") ? q.queryItemValue("w") : "2";
		const QSize px = requested.isValid() && !requested.isEmpty() ? requested : QSize(24, 24);

		QByteArray svg = load(name);
		svg.replace("currentColor", color.toUtf8());
		svg.replace("stroke-width=\"2\"", ("stroke-width=\"" + stroke + "\"").toUtf8());

		QImage img(px, QImage::Format_ARGB32_Premultiplied);
		img.fill(Qt::transparent);
		QSvgRenderer r(svg);
		QPainter p(&img);
		p.setRenderHint(QPainter::Antialiasing);
		r.render(&p);
		p.end();
		if (size)
			*size = px;
		return img;
	}

private:
	QByteArray load(const QString &name)
	{
		QMutexLocker lock(&m_lock);
		auto it = m_cache.find(name);
		if (it != m_cache.end())
			return *it;
		QFile f(m_dir + "/" + name + ".svg");
		const QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
		m_cache.insert(name, data);
		return data;
	}

	QString m_dir;
	QMutex m_lock;
	QHash<QString, QByteArray> m_cache;
};
