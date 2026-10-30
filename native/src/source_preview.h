#pragma once

#include <QImage>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantMap>
#include <atomic>

struct obs_source;
struct obs_volmeter;
class Engine;

// What a source would show (or how loud it is) before it's added: the Add dialog runs a private
// input with the settings picked so far, reads its frames back and paints them here, and meters
// its audio. Nothing joins a scene or the mix; the input goes away when hidden or changed.
class SourcePreview : public QQuickPaintedItem {
	Q_OBJECT
	Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
	Q_PROPERTY(QVariantMap settings READ settings WRITE setSettings NOTIFY settingsChanged)
	Q_PROPERTY(bool live READ live WRITE setLive NOTIFY liveChanged)
	// An existing source to show instead (its device is already open there).
	Q_PROPERTY(QString sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
	Q_PROPERTY(bool ready READ ready NOTIFY readyChanged) // a first frame arrived
	Q_PROPERTY(double level READ level NOTIFY levelChanged) // audio peak, dBFS (-60 = silence)
public:
	explicit SourcePreview(QQuickItem *parent = nullptr);
	~SourcePreview() override;
	static void setEngine(Engine *e); // once the engine has started
	static void releaseAll(); // before the engine shuts down

	QString kind() const { return m_kind; }
	void setKind(const QString &k);
	QVariantMap settings() const { return m_settings; }
	void setSettings(const QVariantMap &s);
	bool live() const { return m_live; }
	QString sourceId() const { return m_sourceId; }
	void setSourceId(const QString &id);
	void setLive(bool on);
	bool ready() const { return m_ready; }
	double level() const { return m_level; }
	Q_INVOKABLE void reload(); // browser: fetch the page again, skipping the cache

	void paint(QPainter *p) override;

signals:
	void kindChanged();
	void settingsChanged();
	void liveChanged();
	void sourceIdChanged();
	void readyChanged();
	void levelChanged();

private:
	void restart();
	void release();
	void tick();
	QImage grabFrame() const;

	QString m_kind, m_sourceId;
	QVariantMap m_settings;
	bool m_live = false, m_ready = false;
	double m_level = -60;
	obs_source *m_src = nullptr;
	obs_volmeter *m_meter = nullptr;
	std::atomic<float> m_peak{-60.0f};
	QImage m_frame;
	QTimer m_timer, m_restart;
};
