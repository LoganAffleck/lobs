// The OBS engine (libobs + its plugins) lives apart from the LOBS app, one folder per OBS
// version under <app>/engine/<version>, with OBS's release layout (bin/64bit, obs-plugins/64bit,
// data). LOBS never modifies libobs: engine updates are OBS's own official release builds,
// downloaded from GitHub, checked against GitHub's SHA-256, and switched in on restart.
// A newly installed engine runs on trial: if LOBS fails to start with it twice, the
// previous engine comes back automatically.
#pragma once

#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;
class QCryptographicHash;
class QProcess;

namespace engine_store {

struct Selected {
	QString version; // e.g. "32.2.2"
	QString root;    // <app>/engine/<version>
	bool rolledBack = false;
	QString rolledBackFrom;
};

QString enginesDir();              // <app>/engine
Selected select();                 // pick the engine to load (handles trial rollback)
void markGood(const QString &running); // the engine this process runs started fine
QStringList installed();           // versions on disk, newest first
QString previous();                // version to roll back to, if any
bool activate(const QString &version, bool trial);
// The engine this LOBS release ships with (and was tested with) takes over once after each
// install or update, even if an earlier engine was picked; the one it replaces stays as the fallback.
void adoptBundled(const QString &bundled);
int compareVersions(const QString &a, const QString &b); // <0, 0, >0; "33.0.0-rc2" < "33.0.0"
int supportedMajor();              // the libobs major version LOBS is built against
QStringList moduleAllowlist();     // from engine-modules.txt next to LOBS.exe
// libobs functions LOBS.exe calls that the given obs.dll doesn't export (empty = all present).
QStringList missingExports(const QString &obsDll);
QStringList lobsImports();         // what LOBS.exe imports from obs.dll

} // namespace engine_store

class EngineUpdater : public QObject {
	Q_OBJECT
	Q_PROPERTY(QString installed READ installedVersion NOTIFY changed)
	Q_PROPERTY(QString latest READ latest NOTIFY changed)
	Q_PROPERTY(QString state READ state NOTIFY changed)        // idle | checking | upToDate | available | needsLobs | downloading | installing | ready | error
	Q_PROPERTY(QString message READ message NOTIFY changed)
	Q_PROPERTY(double progress READ progress NOTIFY changed)   // 0..1 while downloading
	Q_PROPERTY(QString notesUrl READ notesUrl NOTIFY changed)
	Q_PROPERTY(QString previous READ previous NOTIFY changed)
	Q_PROPERTY(bool beta READ beta WRITE setBeta NOTIFY changed)
	Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY changed)
	Q_PROPERTY(QString rolledBackFrom READ rolledBackFrom CONSTANT)
public:
	EngineUpdater(const engine_store::Selected &running, QObject *parent = nullptr);

	QString installedVersion() const { return m_running.version; }
	QString latest() const { return m_latest; }
	QString state() const { return m_state; }
	QString message() const { return m_message; }
	double progress() const { return m_progress; }
	QString notesUrl() const { return m_notesUrl; }
	QString previous() const { return engine_store::previous(); }
	bool beta() const;
	void setBeta(bool on);
	QString lastChecked() const;
	QString rolledBackFrom() const { return m_running.rolledBack ? m_running.rolledBackFrom : QString(); }

	Q_INVOKABLE void check();
	Q_INVOKABLE void checkIfDue(); // at most once a day
	Q_INVOKABLE void install();
	Q_INVOKABLE void rollback();
	Q_INVOKABLE void restartLobs();
	void installLocal(const QString &zip, const QString &version); // self-test: install a zip on disk

signals:
	void changed();

private:
	void set(const QString &state, const QString &message = {});
	void downloadFinished();
	void extract(const QString &zip);
	void verify(const QString &version); // compatibility check + smoke test, then activate
	QProcess *m_smoke = nullptr;

	engine_store::Selected m_running;
	QNetworkAccessManager *m_net;
	QNetworkReply *m_reply = nullptr;
	QFile m_file;
	QCryptographicHash *m_hash = nullptr;
	QString m_state = "idle", m_message, m_latest, m_notesUrl, m_assetUrl, m_assetDigest, m_assetName;
	double m_progress = 0;
	qint64 m_assetSize = 0;
};
