#pragma once
// Live captions: speech to text (whisper.cpp, offline) from any audio source, shown in the
// Captions panel and sent inside the stream as closed captions (CC on Twitch and YouTube).
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class Engine;
class QNetworkAccessManager;
class QNetworkReply;
struct whisper_context;

class Captioner : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool running READ running NOTIFY stateChanged)
	Q_PROPERTY(QString sourceId READ sourceId NOTIFY stateChanged)
	Q_PROPERTY(QString status READ status NOTIFY stateChanged)
	// missing | downloading | ready | error
	Q_PROPERTY(QString modelState READ modelState NOTIFY modelChanged)
	Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY modelChanged)
	Q_PROPERTY(QVariantList lines READ lines NOTIFY linesChanged) // [{text, time}], newest last
	Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY stateChanged) // "auto" or a code like "en", "sv"
	Q_PROPERTY(bool toStream READ toStream WRITE setToStream NOTIFY stateChanged)

public:
	explicit Captioner(Engine *engine, QObject *parent = nullptr);
	~Captioner() override;

	bool running() const { return m_running; }
	QString sourceId() const { return m_sourceId; }
	QString status() const { return m_status; }
	QString modelState() const { return m_modelState; }
	double downloadProgress() const { return m_progress; }
	QVariantList lines() const;
	QString language() const { return m_language; }
	void setLanguage(const QString &l);
	bool toStream() const { return m_toStream; }
	void setToStream(bool on);

	Q_INVOKABLE bool start(const QString &sourceId);
	Q_INVOKABLE void stop();
	Q_INVOKABLE void downloadModel();
	Q_INVOKABLE void clear();
	// Languages whisper understands, for the picker: [{name, value}].
	Q_INVOKABLE QVariantList languages() const;
	Q_INVOKABLE QString modelPath() const;

	// Audio from the OBS audio thread (see Engine::AudioTap).
	static void onAudio(void *self, const float *const *planes, int channels, int frames, bool muted);

signals:
	void stateChanged();
	void modelChanged();
	void linesChanged();

private:
	void setStatus(const QString &s);
	void worker();
	void addLine(const QString &text);
	void checkModel();

	Engine *m_engine;
	QString m_sourceId, m_status, m_modelState = "missing", m_language = "auto";
	bool m_running = false, m_toStream = true;
	double m_progress = 0;
	struct Line { QString text; qint64 time; };
	QList<Line> m_lines;
	QNetworkAccessManager *m_net = nullptr;
	QNetworkReply *m_download = nullptr;

	// Audio handed from OBS's thread to the worker: 16 kHz mono.
	std::mutex m_lock;
	std::condition_variable m_wake;
	std::vector<float> m_pending;
	double m_resamplePos = 0;
	int m_inRate = 48000;
	std::atomic<bool> m_stop{false};
	std::atomic<qint64> m_lastAudioMs{0}; // when audio last arrived (a source that stops sending flushes its last phrase)
	std::thread m_thread;
	whisper_context *m_ctx = nullptr;
};
