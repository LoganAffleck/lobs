// macOS privacy permissions, handled without ever blocking: a source whose permission isn't
// granted is simply not created yet (the engine skips it, the Sources panel says why), the
// app asks in the background for what the open show needs, and a bar lists what's missing
// with a way to fix it. When access is granted, the show is applied again and the source
// starts. On Windows everything is always allowed and none of this shows.
#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>

class Permissions : public QObject {
	Q_OBJECT
	// What the open show needs and doesn't have: [{id, title, detail, action, relaunch}], action
	// being "ask" (the system can still prompt), "settings" (only System Settings can change it)
	// or "relaunch" (granted, but only a new LOBS process gets it); relaunch: also offer that.
	Q_PROPERTY(QVariantList missing READ missing NOTIFY changed)
public:
	explicit Permissions(QObject *parent = nullptr);

	static QString forKind(const QString &sourceKind); // "microphone" | "camera" | "screen" | ""
	static bool allows(const QString &sourceKind);     // from the last check; cheap
	static QString title(const QString &permission);    // "Microphone", "Screen Recording"…

	void setNeeded(const QStringList &sourceKinds);
	void setHotkeysNeeded(bool needed); // global hotkeys are bound (Input Monitoring)
	QVariantList missing() const;
	void askForNeeded(); // the system prompts for needed permissions it never asked about

	Q_INVOKABLE void request(const QString &permission); // ask, or open System Settings
	Q_INVOKABLE void refresh();
	Q_INVOKABLE void relaunch() { emit relaunchRequested(); }

signals:
	void changed();
	void granted(); // something became allowed: apply the show again
	void relaunchRequested();

private:
	QSet<QString> m_needed; // permissions, not source kinds
	bool m_hotkeys = false;
	QSet<QString> m_grantedAtLaunch; // libobs checks Input Monitoring only when it starts
};
