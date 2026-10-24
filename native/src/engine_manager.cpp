#include "engine_manager.h"

#include <obs-config.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QSet>

#include <windows.h>
#include <delayimp.h>

namespace engine_store {

static QString appDir() { return QCoreApplication::applicationDirPath(); }
QString enginesDir() { return appDir() + "/engine"; }
static QString stateFile() { return enginesDir() + "/engine.json"; }

static QJsonObject readState()
{
	QFile f(stateFile());
	return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
}

static void writeState(const QJsonObject &o)
{
	QDir().mkpath(enginesDir());
	QFile f(stateFile() + ".tmp");
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		f.write(QJsonDocument(o).toJson());
		f.close();
		QFile::remove(stateFile());
		QFile::rename(stateFile() + ".tmp", stateFile());
	}
}

static bool valid(const QString &version) { return !version.isEmpty() && QFile::exists(enginesDir() + "/" + version + "/bin/64bit/obs.dll"); }

QStringList installed()
{
	QStringList out;
	for (const QString &d : QDir(enginesDir()).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		if (valid(d))
			out << d;
	std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) { return compareVersions(a, b) > 0; });
	return out;
}

QString previous()
{
	const QString p = readState()["previous"].toString();
	return valid(p) ? p : QString();
}

bool activate(const QString &version, bool trial)
{
	if (!valid(version))
		return false;
	QJsonObject s = readState();
	const QString cur = s["active"].toString();
	if (cur != version && valid(cur))
		s["previous"] = cur;
	s["active"] = version;
	s["trial"] = trial;
	s["trialStarts"] = 0;
	writeState(s);
	return true;
}

void adoptBundled(const QString &bundled)
{
	QJsonObject s = readState();
	if (s["bundled"].toString() == bundled || !valid(bundled))
		return;
	activate(bundled, false);
	s = readState();
	s["bundled"] = bundled;
	s.remove("bad"); // a different release: give engines another chance
	writeState(s);
}

Selected select()
{
	QJsonObject s = readState();
	Selected r;
	QString active = s["active"].toString();
	if (s["trial"].toBool() && valid(active)) {
		const int starts = s["trialStarts"].toInt() + 1;
		s["trialStarts"] = starts;
		const QString prev = s["previous"].toString();
		if (starts > 2 && valid(prev)) {
			// Two starts with the new engine never got going: go back to the one that worked.
			r.rolledBack = true;
			r.rolledBackFrom = active;
			active = prev;
			s["active"] = prev;
			s["previous"] = QString();
			s["trial"] = false;
			s["trialStarts"] = 0;
			s["bad"] = r.rolledBackFrom;
		}
		writeState(s);
	}
	if (!valid(active)) {
		const QStringList all = installed();
		active = all.value(0);
		if (!active.isEmpty()) {
			s["active"] = active;
			s["trial"] = false;
			writeState(s);
		}
	}
	r.version = active;
	r.root = active.isEmpty() ? QString() : enginesDir() + "/" + active;
	return r;
}

void markGood(const QString &running)
{
	QJsonObject s = readState();
	// Only the engine on trial can pass it, and only by actually running in this process.
	if (s["trial"].toBool() && s["active"].toString() == running) {
		s["trial"] = false;
		s["trialStarts"] = 0;
		writeState(s);
	}
}

QStringList moduleAllowlist()
{
	QStringList out;
	QFile f(appDir() + "/engine-modules.txt");
	if (f.open(QIODevice::ReadOnly | QIODevice::Text))
		for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
			const QString t = line.trimmed();
			if (!t.isEmpty() && !t.startsWith('#'))
				out << t;
		}
	return out;
}

QStringList lobsImports()
{
	// obs.dll is delay-loaded, so its imports are in LOBS.exe's delay-import table.
	QStringList out;
	const auto base = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + reinterpret_cast<const IMAGE_DOS_HEADER *>(base)->e_lfanew);
	const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT];
	if (!dir.VirtualAddress)
		return out;
	for (auto d = reinterpret_cast<const ImgDelayDescr *>(base + dir.VirtualAddress); d->rvaDLLName; ++d) {
		if (_stricmp(reinterpret_cast<const char *>(base + d->rvaDLLName), "obs.dll") != 0)
			continue;
		for (auto t = reinterpret_cast<const IMAGE_THUNK_DATA *>(base + d->rvaINT); t->u1.AddressOfData; ++t)
			if (!IMAGE_SNAP_BY_ORDINAL(t->u1.Ordinal))
				out << QString::fromLatin1(static_cast<const char *>(reinterpret_cast<const IMAGE_IMPORT_BY_NAME *>(base + t->u1.AddressOfData)->Name));
	}
	return out;
}

// Names exported by a DLL on disk, read from its PE export table (without loading it).
static QSet<QString> fileExports(const QString &path)
{
	QSet<QString> out;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return out;
	const QByteArray img = f.readAll();
	const auto at = [&](qint64 off, qint64 len) { return off >= 0 && off + len <= img.size(); };
	if (!at(0, sizeof(IMAGE_DOS_HEADER)))
		return out;
	const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(img.constData());
	if (!at(dos->e_lfanew, sizeof(IMAGE_NT_HEADERS64)))
		return out;
	const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(img.constData() + dos->e_lfanew);
	const auto sections = IMAGE_FIRST_SECTION(nt);
	const auto rvaToOff = [&](DWORD rva) -> qint64 {
		for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
			const IMAGE_SECTION_HEADER &s = sections[i];
			if (rva >= s.VirtualAddress && rva < s.VirtualAddress + (std::max)(s.Misc.VirtualSize, s.SizeOfRawData))
				return qint64(rva - s.VirtualAddress) + s.PointerToRawData;
		}
		return -1;
	};
	const IMAGE_DATA_DIRECTORY &dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	const qint64 eo = rvaToOff(dir.VirtualAddress);
	if (!dir.VirtualAddress || !at(eo, sizeof(IMAGE_EXPORT_DIRECTORY)))
		return out;
	const auto ex = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(img.constData() + eo);
	const qint64 no = rvaToOff(ex->AddressOfNames);
	if (!at(no, qint64(ex->NumberOfNames) * 4))
		return out;
	const auto names = reinterpret_cast<const DWORD *>(img.constData() + no);
	for (DWORD i = 0; i < ex->NumberOfNames; ++i)
		if (const qint64 o = rvaToOff(names[i]); at(o, 1))
			out.insert(QString::fromLatin1(img.constData() + o));
	return out;
}

QStringList missingExports(const QString &obsDll)
{
	const QSet<QString> have = fileExports(obsDll);
	QStringList missing;
	if (have.isEmpty())
		return {"(couldn't read obs.dll)"};
	for (const QString &name : lobsImports())
		if (!have.contains(name))
			missing << name;
	return missing;
}

} // namespace engine_store

using namespace engine_store;

// ---------------------------------------------------------------------------

static QString fileVersion(const QString &path)
{
	const std::wstring p = QDir::toNativeSeparators(path).toStdWString();
	DWORD h = 0;
	const DWORD n = GetFileVersionInfoSizeW(p.c_str(), &h);
	if (!n)
		return {};
	QByteArray buf(int(n), 0);
	if (!GetFileVersionInfoW(p.c_str(), 0, n, buf.data()))
		return {};
	VS_FIXEDFILEINFO *fi = nullptr;
	UINT len = 0;
	if (!VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void **>(&fi), &len) || !fi)
		return {};
	return QString("%1.%2.%3").arg(HIWORD(fi->dwFileVersionMS)).arg(LOWORD(fi->dwFileVersionMS)).arg(HIWORD(fi->dwFileVersionLS));
}

// Turns an extracted OBS release (<dir>/bin, obs-plugins, data) into a LOBS engine folder:
// keeps libobs, its libraries and the allowlisted modules; drops OBS's UI, its Qt copy and
// UI-only plugins. Returns an error message, or empty on success.
QString pruneEngine(const QString &dir, QString *note)
{
	const QStringList keep = moduleAllowlist();
	if (keep.isEmpty())
		return "engine-modules.txt is missing next to LOBS.exe";
	if (!QFile::exists(dir + "/bin/64bit/obs.dll"))
		return "the download doesn't contain libobs (bin/64bit/obs.dll)";

	// OBS's Qt: LOBS ships its own. A newer Qt in the engine means Qt-based modules
	// (the browser source) may need a LOBS update to load.
	const QString engineQt = fileVersion(dir + "/bin/64bit/Qt6Core.dll");
	if (note && !engineQt.isEmpty()) {
		const QStringList e = engineQt.split('.'), mine = QString(qVersion()).split('.');
		if (e.value(0).toInt() > mine.value(0).toInt() || (e.value(0) == mine.value(0) && e.value(1).toInt() > mine.value(1).toInt()))
			*note = QString("This engine was built with Qt %1 and LOBS has Qt %2, so browser sources may not load until LOBS updates.").arg(engineQt, qVersion());
	}
	QDir bin(dir + "/bin/64bit");
	for (const QString &f : bin.entryList({"Qt6*.dll", "obs64.exe", "*.pdb", "vc_redist*.exe"}, QDir::Files))
		bin.remove(f);
	for (const QString &d : bin.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) // Qt plugin folders
		QDir(bin.filePath(d)).removeRecursively();

	// Anything at the top level besides the engine itself (e.g. OBS's own UI files) goes.
	for (const QString &d : QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		if (!QStringList{"bin", "data", "obs-plugins", "core"}.contains(d))
			QDir(dir + "/" + d).removeRecursively();
	for (const QString &d : QDir(dir + "/data").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
		if (d != "libobs" && d != "obs-plugins")
			QDir(dir + "/data/" + d).removeRecursively();
	// OBS 33+ layout: one folder per module, core/<name>/ (DLL and data together).
	QDir core(dir + "/core");
	for (const QString &name : core.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (!keep.contains(name)) {
			QDir(core.filePath(name)).removeRecursively();
			continue;
		}
		QDirIterator pdbs(core.filePath(name), {"*.pdb"}, QDir::Files, QDirIterator::Subdirectories);
		while (pdbs.hasNext())
			QFile::remove(pdbs.next());
	}
	// OBS 32 layout: a plugin is a DLL in obs-plugins/64bit with a matching data/obs-plugins/<name>.
	QDir plugins(dir + "/obs-plugins/64bit"), pdata(dir + "/data/obs-plugins");
	for (const QString &name : pdata.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (keep.contains(name))
			continue;
		QDir(pdata.filePath(name)).removeRecursively();
		plugins.remove(name + ".dll");
	}
	for (const QString &name : plugins.entryList({"*.dll"}, QDir::Files)) {
		const QString base = QFileInfo(name).completeBaseName();
		// UI-only modules without data folders (e.g. *-output-ui) also go.
		if (base.endsWith("-ui") || QStringList{"frontend-tools", "obs-websocket", "obs-vst", "decklink-captions", "decklink", "aja", "vlc-video"}.contains(base))
			plugins.remove(name);
	}
	for (const QString &f : plugins.entryList({"*.pdb"}, QDir::Files))
		plugins.remove(f);
	QDir(dir + "/data/obs-studio").removeRecursively(); // OBS's UI themes and locale
	return {};
}

// Installs an OBS release zip as <engines>/<version>. Used by the updater and by
// `LOBS.exe --install-engine <zip>` (build and installer staging). Returns an error or empty.
QString installEngineZip(const QString &zip, const QString &version, QString *note)
{
	const QString stage = enginesDir() + "/" + version + ".partial";
	QDir(stage).removeRecursively();
	QDir().mkpath(stage);
	QProcess tar;
	tar.setProgram(QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/tar.exe"));
	// The whole release; pruneEngine keeps what LOBS needs. OBS has changed its layout
	// between versions (32: obs-plugins/64bit + data/obs-plugins, 33: core/<module>/), so
	// nothing here assumes one.
	tar.setArguments({"-xf", QDir::toNativeSeparators(zip), "-C", QDir::toNativeSeparators(stage)});
	tar.start();
	if (!tar.waitForFinished(10 * 60 * 1000) || tar.exitCode() != 0) {
		QDir(stage).removeRecursively();
		return "couldn't unpack the download: " + QString::fromLocal8Bit(tar.readAllStandardError()).trimmed();
	}
	if (const QString err = pruneEngine(stage, note); !err.isEmpty()) {
		QDir(stage).removeRecursively();
		return err;
	}
	const QString dest = enginesDir() + "/" + version;
	QDir(dest).removeRecursively();
	if (!QDir().rename(stage, dest)) {
		QDir(stage).removeRecursively();
		return "couldn't move the engine into place";
	}
	return {};
}

// ---------------------------------------------------------------------------

EngineUpdater::EngineUpdater(const Selected &running, QObject *parent)
	: QObject(parent), m_running(running), m_net(new QNetworkAccessManager(this))
{
	if (running.rolledBack)
		m_message = QString("OBS engine %1 didn't start, so LOBS went back to %2.").arg(running.rolledBackFrom, running.version);
}

bool EngineUpdater::beta() const { return QSettings().value("engine/beta", false).toBool(); }
void EngineUpdater::setBeta(bool on)
{
	QSettings().setValue("engine/beta", on);
	emit changed();
	check();
}
QString EngineUpdater::lastChecked() const
{
	const QDateTime t = QSettings().value("engine/lastChecked").toDateTime();
	return t.isValid() ? t.toString("MMM d, h:mm ap") : QString();
}

void EngineUpdater::set(const QString &state, const QString &message)
{
	m_state = state;
	m_message = message;
	if (message.isEmpty() && state == "upToDate" && m_running.rolledBack) // keep explaining the rollback
		m_message = QString("OBS engine %1 didn't start, so LOBS went back to %2.").arg(m_running.rolledBackFrom, m_running.version);
	emit changed();
}

void EngineUpdater::checkIfDue()
{
	const QDateTime t = QSettings().value("engine/lastChecked").toDateTime();
	if (!t.isValid() || t.secsTo(QDateTime::currentDateTime()) > 24 * 3600)
		check();
}

void EngineUpdater::check()
{
	if (m_state == "checking" || m_state == "downloading" || m_state == "installing" || m_state == "testing")
		return;
	set("checking");
	// Only GitHub's public releases list is requested; nothing about this PC is sent.
	QNetworkRequest req(QUrl("https://api.github.com/repos/obsproject/obs-studio/releases?per_page=20"));
	req.setHeader(QNetworkRequest::UserAgentHeader, "LOBS");
	req.setRawHeader("Accept", "application/vnd.github+json");
	QNetworkReply *r = m_net->get(req);
	connect(r, &QNetworkReply::finished, this, [this, r]() {
		r->deleteLater();
		if (r->error() != QNetworkReply::NoError)
			return set("error", "Couldn't reach GitHub to check for OBS updates.");
		QSettings().setValue("engine/lastChecked", QDateTime::currentDateTime());
		const QString bad = readState()["bad"].toString();
		for (const QJsonValue v : QJsonDocument::fromJson(r->readAll()).array()) {
			const QJsonObject rel = v.toObject();
			if (rel["draft"].toBool() || (rel["prerelease"].toBool() && !beta()))
				continue;
			const QString ver = rel["tag_name"].toString();
			for (const QJsonValue av : rel["assets"].toArray()) {
				const QJsonObject a = av.toObject();
				const QString name = a["name"].toString();
				if (!name.endsWith("-Windows-x64.zip"))
					continue;
				m_latest = ver;
				m_notesUrl = rel["html_url"].toString();
				m_assetUrl = a["browser_download_url"].toString();
				m_assetDigest = a["digest"].toString();
				m_assetName = name;
				m_assetSize = a["size"].toInteger();
				const int major = ver.section('.', 0, 0).toInt();
				if (compareVersions(ver, m_running.version) <= 0)
					return set("upToDate");
				if (installed().contains(ver))
					return set("ready", QString("OBS engine %1 is installed. Restart LOBS to use it.").arg(ver));
				if (QSettings().value("engine/incompatible/" + ver).toBool())
					return set("needsLobs", QString("OBS %1 didn't pass LOBS's compatibility check, so LOBS stays on %2 until a LOBS update supports it.").arg(ver, m_running.version));
				Q_UNUSED(major);
				if (ver == bad)
					return set("upToDate", QString("OBS %1 didn't start on this PC earlier, so it isn't offered again.").arg(ver));
				return set("available");
			}
		}
		set("upToDate");
	});
}

void EngineUpdater::install()
{
	if (m_state != "available" || m_assetUrl.isEmpty())
		return;
	if (!m_assetDigest.startsWith("sha256:"))
		return set("error", "No checksum for this download. Not installed.");
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads";
	QDir().mkpath(dir);
	m_file.setFileName(dir + "/" + m_assetName);
	if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return set("error", "Couldn't write the download to " + dir);
	delete m_hash;
	m_hash = new QCryptographicHash(QCryptographicHash::Sha256);
	m_progress = 0;
	set("downloading");
	QNetworkRequest req{QUrl(m_assetUrl)};
	req.setHeader(QNetworkRequest::UserAgentHeader, "LOBS");
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	m_reply = m_net->get(req);
	connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
		const QByteArray chunk = m_reply->readAll();
		m_hash->addData(chunk);
		m_file.write(chunk);
	});
	connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
		m_progress = double(got) / double(total > 0 ? total : std::max<qint64>(1, m_assetSize));
		emit changed();
	});
	connect(m_reply, &QNetworkReply::finished, this, &EngineUpdater::downloadFinished);
}

void EngineUpdater::downloadFinished()
{
	m_reply->deleteLater();
	const QByteArray rest = m_reply->readAll();
	m_hash->addData(rest);
	m_file.write(rest);
	m_file.close();
	const bool ok = m_reply->error() == QNetworkReply::NoError;
	m_reply = nullptr;
	if (!ok) {
		m_file.remove();
		return set("error", "The download didn't finish. Try again.");
	}
	if ("sha256:" + m_hash->result().toHex() != m_assetDigest.toLatin1()) {
		m_file.remove();
		return set("error", "Checksum mismatch. Download discarded.");
	}
	extract(m_file.fileName());
}

QString installEngineZip(const QString &zip, const QString &version, QString *note);

void EngineUpdater::extract(const QString &zip)
{
	set("installing");
	const QString ver = m_latest;
	// Unpacking takes a few seconds; let the UI paint first.
	QTimer::singleShot(50, this, [this, zip, ver]() {
		QString note;
		const QString err = installEngineZip(zip, ver, &note);
		QFile::remove(zip);
		if (!err.isEmpty())
			return set("error", "Install failed: " + err);
		m_message = note;
		verify(ver);
	});
}

// Before LOBS switches to a new engine: (1) every libobs function LOBS calls must exist in it,
// (2) a hidden LOBS run on it must build a scene with sources, filters and an audio effect,
// render a correct frame and record a clip. Only then is it activated (still on trial).
void EngineUpdater::verify(const QString &ver)
{
	const QString note = m_message;
	const auto reject = [this, ver](const QString &why) {
		QDir(enginesDir() + "/" + ver).removeRecursively();
		QSettings().setValue("engine/incompatible/" + ver, true);
		set("needsLobs", QString("OBS %1 isn't compatible with this LOBS yet (%2), so LOBS stays on %3.").arg(ver, why, m_running.version));
	};
	const QStringList missing = missingExports(enginesDir() + "/" + ver + "/bin/64bit/obs.dll");
	if (!missing.isEmpty())
		return reject(QString("it no longer has %1 libobs function%2 LOBS uses: %3").arg(missing.size()).arg(missing.size() == 1 ? "" : "s").arg(missing.mid(0, 4).join(", ") + (missing.size() > 4 ? ", ..." : "")));

	set("testing", QString("Testing OBS %1 with LOBS...").arg(ver));
	const QString report = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/lobs-smoketest-" + ver + ".json";
	QFile::remove(report);
	m_smoke = new QProcess(this);
	m_smoke->setProgram(QCoreApplication::applicationFilePath());
	m_smoke->setArguments({"--use-engine", ver, "--engine-smoketest", report});
	QTimer *timeout = new QTimer(m_smoke);
	timeout->setSingleShot(true);
	connect(timeout, &QTimer::timeout, m_smoke, [this]() { m_smoke->kill(); });
	connect(m_smoke, &QProcess::finished, this, [this, ver, report, note, reject](int code, QProcess::ExitStatus status) {
		m_smoke->deleteLater();
		m_smoke = nullptr;
		QFile f(report);
		const QJsonObject r = f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
		f.close();
		QFile::remove(report);
		if (status != QProcess::NormalExit || code != 0 || !r["ok"].toBool()) {
			QStringList failed;
			for (const QJsonValue c : r["checks"].toArray())
				if (!c.toObject()["ok"].toBool())
					failed << c.toObject()["name"].toString();
			return reject(r.isEmpty() ? QString("a test run on it crashed or hung") : "the test run failed: " + failed.join(", "));
		}
		activate(ver, true);
		for (const QString &v : installed()) // keep only the running engine and the new one
			if (v != ver && v != m_running.version)
				QDir(enginesDir() + "/" + v).removeRecursively();
		set("ready", QString("OBS engine %1 passed LOBS's checks. Restart LOBS to use it.").arg(ver) + (note.isEmpty() ? QString() : " " + note));
	});
	timeout->start(120000);
	m_smoke->start();
}

void EngineUpdater::installLocal(const QString &zip, const QString &version)
{
	const QString copy = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads/selftest.zip";
	QDir().mkpath(QFileInfo(copy).absolutePath());
	QFile::remove(copy);
	QFile::copy(zip, copy);
	m_latest = version;
	extract(copy);
}

void EngineUpdater::rollback()
{
	const QString prev = previous();
	if (prev.isEmpty())
		return;
	activate(prev, false);
	set("ready", QString("LOBS will use OBS engine %1 after a restart.").arg(prev));
}

void EngineUpdater::restartLobs()
{
	QProcess::startDetached(QCoreApplication::applicationFilePath(), {});
	QCoreApplication::quit();
}
