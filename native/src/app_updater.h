#pragma once

#include <QFile>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QCryptographicHash;

// LOBS updates itself from its GitHub releases: a daily check, then (when asked) download the
// signed installer, check its SHA-256 and that it's signed by LOBS's publisher, and run it
// quietly over this install. Each release carries the OBS engine it was tested with.
class AppUpdater : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString current READ current CONSTANT)
	Q_PROPERTY(QString latest READ latest NOTIFY changed)
	Q_PROPERTY(QString state READ state NOTIFY changed)      // idle | checking | upToDate | available | downloading | ready | installing | error
	Q_PROPERTY(QString message READ message NOTIFY changed)
	Q_PROPERTY(double progress READ progress NOTIFY changed) // 0..1 while downloading
	Q_PROPERTY(QString notesUrl READ notesUrl NOTIFY changed)
	Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY changed)
	Q_PROPERTY(bool justUpdated READ justUpdated CONSTANT)  // this run started right after an update
public:
	explicit AppUpdater(bool justUpdated, QObject *parent = nullptr);
	QString current() const;
	QString latest() const { return m_latest; }
	QString state() const { return m_state; }
	QString message() const { return m_message; }
	double progress() const { return m_progress; }
	QString notesUrl() const { return m_notesUrl; }
	QString lastChecked() const;
	bool justUpdated() const { return m_justUpdated; }

	Q_INVOKABLE void check();
	Q_INVOKABLE void checkIfDue(); // at most once a day
	Q_INVOKABLE void download();
	Q_INVOKABLE void installAndRestart(); // runs the verified installer, then quits LOBS

signals:
	void changed();
	void quitForInstall();

private:
	void set(const QString &state, const QString &message = {});
	void downloadFinished();

	QNetworkAccessManager *m_net;
	QNetworkReply *m_reply = nullptr;
	QCryptographicHash *m_hash = nullptr;
	QFile m_file;
	QString m_state = "idle", m_message, m_latest, m_notesUrl, m_assetUrl, m_assetDigest, m_assetName;
	qint64 m_assetSize = 0;
	double m_progress = 0;
	bool m_justUpdated = false;
};

// The installer's Authenticode signature is valid and was made by LOBS's publisher.
// Returns an error message, or empty when it's good.
QString verifyPublisher(const QString &path);
