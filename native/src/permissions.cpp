#include "permissions.h"
#include "platform.h"

#include <QGuiApplication>
#include <QHash>
#include <QVariantMap>

#include <util/base.h>

namespace {
// The last answer per permission. Read on every apply and status refresh, so it's cached and
// only re-checked on refresh() (startup, after a prompt, and whenever LOBS comes to the front).
QHash<QString, platform::Access> g_access;

platform::Access cached(const QString &permission)
{
	auto it = g_access.find(permission);
	if (it == g_access.end())
		it = g_access.insert(permission, platform::access(permission));
	return *it;
}
}

Permissions::Permissions(QObject *parent) : QObject(parent)
{
	// Coming back from System Settings (or anywhere) picks up a change made there.
	connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState s) {
		if (s == Qt::ApplicationActive)
			refresh();
	});
	refresh();
	for (const QString p : {"microphone", "camera", "screen"})
		blog(LOG_INFO, "[lobs] %s access at startup: %s", qPrintable(p),
		     cached(p) == platform::Access::Granted ? "granted" : cached(p) == platform::Access::Denied ? "denied" : "not asked yet");
}

QString Permissions::forKind(const QString &sourceKind)
{
	if (sourceKind == "mic")
		return "microphone";
	if (sourceKind == "camera")
		return "camera";
	if (sourceKind == "display" || sourceKind == "window" || sourceKind == "desktop_audio")
		return "screen"; // ScreenCaptureKit captures desktop audio too
	return {};
}

bool Permissions::allows(const QString &sourceKind)
{
	const QString p = forKind(sourceKind);
	return p.isEmpty() || cached(p) == platform::Access::Granted;
}

QString Permissions::title(const QString &permission)
{
	return permission == "screen" ? "Screen Recording" : permission == "camera" ? "Camera" : "Microphone";
}

void Permissions::setNeeded(const QStringList &sourceKinds)
{
	QSet<QString> needed;
	for (const QString &k : sourceKinds)
		if (const QString p = forKind(k); !p.isEmpty())
			needed.insert(p);
	if (needed == m_needed)
		return;
	m_needed = needed;
	emit changed();
}

QVariantList Permissions::missing() const
{
	static const QHash<QString, QString> uses = {
		{"microphone", "your mic"}, {"camera", "your camera"}, {"screen", "display, window and desktop audio capture"}};
	QVariantList out;
	for (const QString p : {"microphone", "camera", "screen"}) {
		if (!m_needed.contains(p))
			continue;
		const platform::Access a = cached(p);
		if (a == platform::Access::Granted)
			continue;
		const bool ask = a == platform::Access::NotAsked;
		out << QVariantMap{{"id", p}, {"title", title(p)}, {"action", ask ? "ask" : "settings"},
				   {"detail", ask ? "LOBS needs your permission for " + uses[p] + "."
						  : title(p) + " is off for LOBS, so " + uses[p] + " stays empty. Turn it on in System Settings."}};
	}
	return out;
}

void Permissions::askForNeeded()
{
	// Microphone and camera only: their prompt is a simple Allow / Don't Allow. Screen
	// Recording's leads to System Settings, so it waits for the bar's button.
	for (const QString p : {"microphone", "camera"})
		if (m_needed.contains(p) && cached(p) == platform::Access::NotAsked)
			request(p);
}

void Permissions::request(const QString &permission)
{
	if (cached(permission) == platform::Access::NotAsked) {
		blog(LOG_INFO, "[lobs] asking for %s access", qPrintable(permission));
		platform::requestAccess(permission, [this] { refresh(); });
	} else {
		platform::openPrivacySettings(permission);
	}
}

void Permissions::refresh()
{
	bool newlyGranted = false, any = false;
	for (const QString p : {"microphone", "camera", "screen"}) {
		const platform::Access now = platform::access(p);
		const auto it = g_access.constFind(p);
		if (it != g_access.constEnd() && *it == now)
			continue;
		any = true;
		newlyGranted |= now == platform::Access::Granted && it != g_access.constEnd();
		g_access[p] = now;
		blog(LOG_INFO, "[lobs] %s access: %s", qPrintable(p),
		     now == platform::Access::Granted ? "granted" : now == platform::Access::Denied ? "denied" : "not asked yet");
	}
	if (any)
		emit changed();
	if (newlyGranted)
		emit granted();
}
