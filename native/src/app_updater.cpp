#include "app_updater.h"
#include "engine_manager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

#include <util/base.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#endif

namespace {
#ifdef Q_OS_WIN
// The publisher every LOBS installer is signed by (Azure Artifact Signing, see BUILDING.md).
const wchar_t *kPublisher = L"Logan Affleck";
#endif
const char *kFeed = "https://api.github.com/repos/LoganAffleck/lobs/releases/latest";
}

#ifdef Q_OS_WIN

QString verifyPublisher(const QString &path)
{
	const std::wstring file = QDir::toNativeSeparators(path).toStdWString();
	WINTRUST_FILE_INFO fi{sizeof(fi), file.c_str(), nullptr, nullptr};
	WINTRUST_DATA wd{};
	wd.cbStruct = sizeof(wd);
	wd.dwUIChoice = WTD_UI_NONE;
	wd.fdwRevocationChecks = WTD_REVOKE_NONE;
	wd.dwUnionChoice = WTD_CHOICE_FILE;
	wd.pFile = &fi;
	wd.dwStateAction = WTD_STATEACTION_VERIFY;
	GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	const LONG trust = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
	QString err;
	if (trust != ERROR_SUCCESS) {
		err = QString("the download isn't validly signed (0x%1)").arg(quint32(trust), 8, 16, QChar('0'));
	} else {
		// Trusted by Windows; now make sure it's LOBS's publisher and not just anyone's valid signature.
		CRYPT_PROVIDER_DATA *pd = WTHelperProvDataFromStateData(wd.hWVTStateData);
		CRYPT_PROVIDER_SGNR *signer = pd ? WTHelperGetProvSignerFromChain(pd, 0, FALSE, 0) : nullptr;
		CRYPT_PROVIDER_CERT *cert = signer ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
		wchar_t name[256] = {};
		if (cert)
			CertGetNameStringW(cert->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, name, 256);
		if (wcscmp(name, kPublisher) != 0)
			err = "the download is signed by \"" + QString::fromWCharArray(name) + "\", not LOBS's publisher";
	}
	wd.dwStateAction = WTD_STATEACTION_CLOSE;
	WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
	return err;
}
#else
// Releases only carry the Windows installer so far; macOS updates come with Sparkle (PLAN-MACOS.md, Phase 4).
QString verifyPublisher(const QString &) { return "updates aren't available on macOS yet"; }
#endif

AppUpdater::AppUpdater(bool justUpdated, QObject *parent)
	: QObject(parent), m_net(new QNetworkAccessManager(this)), m_justUpdated(justUpdated)
{
	if (justUpdated) {
		m_message = "Updated to LOBS " + current() + ".";
		blog(LOG_INFO, "[lobs] started after updating to %s", LOBS_VERSION);
	}
	// Installers from earlier updates: done with once they've run.
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads";
	for (const QString &f : QDir(dir).entryList({"LOBS-Setup-*.exe"}, QDir::Files))
		QFile::remove(dir + "/" + f);
}

QString AppUpdater::current() const
{
	// Self-tests pretend to be older to exercise an update (only together with a test feed).
	if (const QString v = qEnvironmentVariable("LOBS_UPDATE_PRETEND_VERSION"); !v.isEmpty() && qEnvironmentVariableIsSet("LOBS_UPDATE_FEED"))
		return v;
	return LOBS_VERSION;
}

QString AppUpdater::lastChecked() const
{
	const QDateTime t = QSettings().value("update/lastChecked").toDateTime();
	return t.isValid() ? t.toString("MMM d, h:mm ap") : QString();
}

void AppUpdater::set(const QString &state, const QString &message)
{
	m_state = state;
	m_message = message;
	emit changed();
}

void AppUpdater::checkIfDue()
{
	const QDateTime t = QSettings().value("update/lastChecked").toDateTime();
	if (!t.isValid() || t.secsTo(QDateTime::currentDateTime()) > 24 * 3600)
		check();
}

void AppUpdater::check()
{
	if (m_state == "checking" || m_state == "downloading" || m_state == "ready")
		return;
#ifndef Q_OS_WIN
	return; // see verifyPublisher
#endif
	set("checking");
	// Only GitHub's public "latest release" is requested; nothing about this PC is sent.
	const QString feed = qEnvironmentVariable("LOBS_UPDATE_FEED", kFeed);
	QNetworkRequest req{QUrl(feed)};
	req.setHeader(QNetworkRequest::UserAgentHeader, "LOBS");
	req.setRawHeader("Accept", "application/vnd.github+json");
	QNetworkReply *r = m_net->get(req);
	connect(r, &QNetworkReply::finished, this, [this, r]() {
		r->deleteLater();
		if (r->error() != QNetworkReply::NoError)
			return set("error", "Couldn't reach GitHub to check for updates.");
		QSettings().setValue("update/lastChecked", QDateTime::currentDateTime());
		const QJsonObject rel = QJsonDocument::fromJson(r->readAll()).object();
		const QString ver = rel["tag_name"].toString().remove(QRegularExpression("^v"));
		if (ver.isEmpty() || rel["draft"].toBool() || rel["prerelease"].toBool())
			return set("upToDate");
		if (engine_store::compareVersions(ver, current()) <= 0)
			return set("upToDate");
		for (const QJsonValue av : rel["assets"].toArray()) {
			const QJsonObject a = av.toObject();
			const QString name = a["name"].toString();
			if (!name.startsWith("LOBS-Setup-") || !name.endsWith(".exe"))
				continue;
			m_latest = ver;
			m_notesUrl = rel["html_url"].toString();
			m_assetUrl = a["browser_download_url"].toString();
			m_assetDigest = a["digest"].toString();
			m_assetName = name;
			m_assetSize = a["size"].toInteger();
			blog(LOG_INFO, "[lobs] update available: %s", qPrintable(ver));
			return set("available");
		}
		set("upToDate"); // a release without an installer yet (still uploading)
	});
}

void AppUpdater::download()
{
	if (m_state != "available" || m_assetUrl.isEmpty())
		return;
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/downloads";
	QDir().mkpath(dir);
	m_file.setFileName(dir + "/" + m_assetName);
	if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return set("error", "Couldn't save the download to " + dir);
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
	connect(m_reply, &QNetworkReply::finished, this, &AppUpdater::downloadFinished);
}

void AppUpdater::downloadFinished()
{
	m_reply->deleteLater();
	const QByteArray rest = m_reply->readAll();
	m_hash->addData(rest);
	m_file.write(rest);
	m_file.close();
	const bool ok = m_reply->error() == QNetworkReply::NoError;
	m_reply = nullptr;
	const auto fail = [this](const QString &why) {
		m_file.remove();
		m_state = "available"; // the button offers a retry
		m_message = why;
		emit changed();
	};
	if (!ok)
		return fail("The download didn't finish. Try again.");
	// GitHub publishes each asset's SHA-256; older releases may not have one, and the
	// signature check below covers those.
	if (m_assetDigest.startsWith("sha256:") && "sha256:" + m_hash->result().toHex() != m_assetDigest.toLatin1())
		return fail("The download was damaged (checksum mismatch). Try again.");
	if (const QString err = verifyPublisher(m_file.fileName()); !err.isEmpty()) {
		blog(LOG_WARNING, "[lobs] update rejected: %s", qPrintable(err));
		return fail("Not installed: " + err + ".");
	}
	set("ready");
}

void AppUpdater::installAndRestart()
{
	if (m_state != "ready")
		return;
	// Check again right before running it: the file sat on disk while LOBS was open.
	if (const QString err = verifyPublisher(m_file.fileName()); !err.isEmpty())
		return set("error", "Not installed: " + err + ".");
	// Quiet install over this one; /LOBSUPDATE makes the installer start LOBS again afterwards.
	QStringList args{"/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS", "/LOBSUPDATE=1"};
	if (const QString dir = qEnvironmentVariable("LOBS_UPDATE_DIR"); !dir.isEmpty()) // self-test install
		args << "/DIR=" + QDir::toNativeSeparators(dir);
	if (!QProcess::startDetached(m_file.fileName(), args))
		return set("error", "Couldn't start the installer.");
	set("installing");
	blog(LOG_INFO, "[lobs] installing update %s", qPrintable(m_latest));
	emit quitForInstall(); // the window closes as usual (saves the layout); the installer waits for LOBS to exit
}
