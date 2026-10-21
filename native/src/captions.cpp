#include "captions.h"
#include "engine.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>

#include <whisper.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {
// whisper.cpp's multilingual base model (MIT); 142 MiB. Checked against the published SHA-1.
const char *kModelUrl = "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin";
const char *kModelSha1 = "465707469ff3a37a2b9b8d8f89f2f99de7299dac";
constexpr int kRate = 16000;            // whisper's input rate
// Speech is cut into phrases at pauses: at least kMinPhrase long, at most kMaxPhrase, cut where
// the last kPause of audio is quiet. Cutting mid-word lost words at the seams.
constexpr int kMinPhrase = kRate * 2;
constexpr int kMaxPhrase = kRate * 8;
constexpr int kPause = kRate * 3 / 10;
constexpr int kChunk = kMinPhrase;      // the worker wakes once this much is queued
constexpr float kSilenceRms = 0.006f;   // quieter chunks are skipped (whisper invents words in silence)
}

Captioner::Captioner(Engine *engine, QObject *parent) : QObject(parent), m_engine(engine)
{
	whisper_log_set([](ggml_log_level, const char *, void *) {}, nullptr); // keep whisper quiet
	checkModel();
}

Captioner::~Captioner()
{
	stop();
}

QString Captioner::modelPath() const
{
	// QA points this at a local copy; users get it downloaded into LOBS's data folder.
	const QString override = qEnvironmentVariable("LOBS_WHISPER_MODEL");
	if (!override.isEmpty())
		return override;
	return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/models/ggml-base.bin";
}

void Captioner::checkModel()
{
	m_modelState = QFile::exists(modelPath()) ? "ready" : "missing";
	emit modelChanged();
}

void Captioner::downloadModel()
{
	if (m_download || m_modelState == "ready")
		return;
	if (!m_net)
		m_net = new QNetworkAccessManager(this);
	QDir().mkpath(QFileInfo(modelPath()).absolutePath());
	QNetworkRequest req{QUrl(QString::fromLatin1(kModelUrl))};
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	m_download = m_net->get(req);
	m_modelState = "downloading", m_progress = 0;
	emit modelChanged();
	connect(m_download, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
		m_progress = total > 0 ? double(got) / double(total) : 0;
		emit modelChanged();
	});
	connect(m_download, &QNetworkReply::finished, this, [this] {
		QNetworkReply *r = std::exchange(m_download, nullptr);
		r->deleteLater();
		const QByteArray data = r->readAll();
		const bool ok = r->error() == QNetworkReply::NoError && QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex() == kModelSha1;
		if (ok) {
			QSaveFile f(modelPath());
			if (f.open(QIODevice::WriteOnly) && f.write(data) == data.size() && f.commit()) {
				checkModel();
				return;
			}
		}
		m_modelState = "error";
		setStatus(r->error() != QNetworkReply::NoError ? "Couldn't download the speech model: " + r->errorString() : "The speech model download was damaged. Try again.");
		emit modelChanged();
	});
}

void Captioner::setStatus(const QString &s)
{
	m_status = s;
	emit stateChanged();
}

void Captioner::setLanguage(const QString &l)
{
	if (l == m_language)
		return;
	m_language = l;
	emit stateChanged();
}

void Captioner::setToStream(bool on)
{
	if (on == m_toStream)
		return;
	m_toStream = on;
	emit stateChanged();
}

QVariantList Captioner::languages() const
{
	QVariantList out{QVariantMap{{"name", "Detect automatically"}, {"value", "auto"}}};
	for (const char *code : {"en", "sv", "es", "fr", "de", "pt", "it", "nl", "pl", "ru", "uk", "tr", "ja", "ko", "zh", "ar", "hi", "fi", "no", "da"}) {
		const QString name = QString::fromUtf8(whisper_lang_str_full(whisper_lang_id(code)));
		out << QVariantMap{{"name", name.left(1).toUpper() + name.mid(1)}, {"value", code}};
	}
	return out;
}

bool Captioner::start(const QString &sourceId)
{
	stop();
	checkModel();
	if (m_modelState != "ready")
		return setStatus("Download the speech model first"), false;
	whisper_context_params cp = whisper_context_default_params();
	m_ctx = whisper_init_from_file_with_params(modelPath().toUtf8().constData(), cp);
	if (!m_ctx)
		return setStatus("Couldn't load the speech model"), false;
	m_inRate = m_engine->audioSampleRate();
	m_resamplePos = 0;
	{
		std::lock_guard l(m_lock);
		m_pending.clear();
	}
	if (!m_engine->addAudioTap(sourceId, &Captioner::onAudio, this)) {
		whisper_free(std::exchange(m_ctx, nullptr));
		return setStatus("That source has no audio"), false;
	}
	m_sourceId = sourceId;
	m_stop = false;
	m_thread = std::thread([this] { worker(); });
	m_running = true;
	setStatus("Listening");
	return true;
}

void Captioner::stop()
{
	if (!m_running)
		return;
	m_engine->removeAudioTap(m_sourceId, &Captioner::onAudio, this);
	m_stop = true;
	m_wake.notify_all();
	if (m_thread.joinable())
		m_thread.join();
	whisper_free(std::exchange(m_ctx, nullptr));
	m_running = false;
	m_sourceId.clear();
	setStatus("");
}

// OBS's audio thread: mix to mono, resample to 16 kHz (linear; speech needs no more), queue.
void Captioner::onAudio(void *p, const float *const *planes, int channels, int frames, bool muted)
{
	auto *self = static_cast<Captioner *>(p);
	if (muted || frames <= 0 || channels <= 0)
		return;
	const double step = double(self->m_inRate) / kRate;
	std::vector<float> mono(static_cast<size_t>(frames));
	for (int i = 0; i < frames; ++i) {
		float v = 0;
		for (int c = 0; c < channels; ++c)
			v += planes[c] ? planes[c][i] : 0.0f;
		mono[size_t(i)] = v / float(channels);
	}
	self->m_lastAudioMs = QDateTime::currentMSecsSinceEpoch();
	std::lock_guard l(self->m_lock);
	double pos = self->m_resamplePos;
	for (; pos < frames - 1; pos += step) {
		const int i = int(pos);
		const float f = float(pos - i);
		self->m_pending.push_back(mono[size_t(i)] * (1 - f) + mono[size_t(i + 1)] * f);
	}
	self->m_resamplePos = pos - frames;
	if (self->m_pending.size() >= size_t(kChunk))
		self->m_wake.notify_one();
}

void Captioner::worker()
{
	whisper_full_params wp = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
	wp.n_threads = std::max(2, int(std::thread::hardware_concurrency()) / 2);
	wp.print_progress = wp.print_realtime = wp.print_timestamps = wp.print_special = false;
	wp.no_context = true;
	wp.single_segment = false;
	wp.suppress_blank = true;
	wp.suppress_nst = true; // no "(music)" or "[applause]" style tokens
	const QByteArray lang = m_language.toUtf8();
	wp.language = lang.constData();
	while (!m_stop) {
		std::vector<float> chunk;
		{
			std::unique_lock l(m_lock);
			m_wake.wait_for(l, std::chrono::milliseconds(200), [&] { return m_stop.load() || m_pending.size() >= size_t(kMinPhrase); });
			if (m_stop)
				break;
			const bool idle = !m_pending.empty() && QDateTime::currentMSecsSinceEpoch() - m_lastAudioMs > 1000;
			if (m_pending.size() < size_t(kMinPhrase) && !idle)
				continue;
			// The first quiet stretch after kMinPhrase ends the phrase; otherwise wait, up to kMaxPhrase.
			auto quiet = [&](size_t end) {
				double e = 0;
				for (size_t i = end - kPause; i < end; ++i)
					e += double(m_pending[i]) * m_pending[i];
				return std::sqrt(e / kPause) < kSilenceRms;
			};
			size_t cut = 0;
			for (size_t end = kMinPhrase; end + kRate / 10 <= m_pending.size() && end <= size_t(kMaxPhrase); end += kRate / 10)
				if (quiet(end)) { cut = end; break; }
			if (!cut && m_pending.size() >= size_t(kMaxPhrase))
				cut = kMaxPhrase;
			if (!cut && idle)
				cut = m_pending.size(); // the audio stopped: what's left is the last phrase
			if (!cut)
				continue; // still talking: wait for a pause
			chunk.assign(m_pending.begin(), m_pending.begin() + cut);
			m_pending.erase(m_pending.begin(), m_pending.begin() + cut);
			if (m_pending.size() > size_t(kMaxPhrase) * 3) // falling behind (slow PC): drop the backlog
				m_pending.clear();
		}
		double sum = 0;
		for (float v : chunk)
			sum += double(v) * v;
		if (std::sqrt(sum / chunk.size()) < kSilenceRms)
			continue;
		if (whisper_full(m_ctx, wp, chunk.data(), int(chunk.size())) != 0)
			continue;
		QString text;
		for (int i = 0; i < whisper_full_n_segments(m_ctx); ++i)
			text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
		text = text.trimmed();
		if (text.isEmpty() || (text.startsWith('[') && text.endsWith(']')) || (text.startsWith('(') && text.endsWith(')')))
			continue;
		QMetaObject::invokeMethod(this, [this, text] { addLine(text); });
	}
}

void Captioner::addLine(const QString &text)
{
	m_lines.append({text, QDateTime::currentMSecsSinceEpoch()});
	while (m_lines.size() > 200)
		m_lines.removeFirst();
	emit linesChanged();
	if (m_toStream)
		m_engine->sendCaption(text, 5.0);
}

void Captioner::clear()
{
	m_lines.clear();
	emit linesChanged();
}

QVariantList Captioner::lines() const
{
	QVariantList out;
	for (const Line &l : m_lines)
		out << QVariantMap{{"text", l.text}, {"time", l.time}};
	return out;
}
