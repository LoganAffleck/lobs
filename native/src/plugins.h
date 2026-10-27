// Third-party OBS plugins: one folder per plugin in the per-user plugins folder, in OBS's
// per-user layout (<name>/bin/64bit/<name>.dll, <name>/data). LOBS loads them itself, one at a
// time after its own modules, so each gets a status and a reason; and keeps which source,
// filter and transition types each one registered. See qa/PLUGINS_PLAN.md.
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

struct obs_source;

namespace lobs::plugins {

struct Plugin {
	QString name, folder, version, title;
	QString status; // loaded | failed | crashed | disabled | pending (added or changed since start) | removing
	QString reason;
	QStringList sources, filters, transitions, other; // OBS type ids it registered
};

QString folder();             // the per-user plugins folder (created on first use)
void loadAll();               // Engine::start: after LOBS's own modules, before obs_post_load_modules
QVector<Plugin> list();       // what's in the folder now, with this run's load results
bool restartNeeded();         // the folder no longer matches what loaded
QString install(const QString &path, QStringList *names); // a folder or .zip; returns an error or empty
QString remove(const QString &name);
QString setEnabled(const QString &name, bool on);

// Types registered by plugins that loaded (OBS ids).
QStringList inputTypes();
QStringList filterTypes();
bool isPluginType(const QString &obsId);

// A generic editor over obs_properties: rows for QML (see ObsProperties.qml).
QVariantList describe(obs_source *src);
bool clickButton(obs_source *src, const QString &key); // true: properties changed, list again
// Converts a value from the editor to what OBS stores for that property (e.g. "#rrggbb" -> int).
QVariant toObsValue(obs_source *src, const QString &key, const QVariant &v);
// Runs the property's "modified" callback with the new value, as OBS's properties view does
// (plugins set reload flags or change other settings there). Returns the other settings it changed.
QVariantMap modified(obs_source *src, const QString &key, const QVariant &obsValue);
// What OBS's properties view sets when it opens: the first choice of each list whose value
// isn't one of its choices (Composite Blur starts with no blur algorithm and draws nothing).
QVariantMap settleLists(obs_source *src);

} // namespace lobs::plugins
