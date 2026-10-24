// On macOS the OBS engine ships inside LOBS.app (libobs.framework and its dylibs in
// Contents/Frameworks, the modules in Contents/PlugIns): changing a signed app bundle breaks
// its signature, so the engine updates with LOBS and there is only ever the bundled one.
#include "engine_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace engine_store {

QString enginesDir() { return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../Frameworks"); }

Selected select()
{
	Selected s;
	if (QFile::exists(enginesDir() + "/libobs.framework/libobs"))
		s = {LOBS_ENGINE_VERSION, enginesDir()};
	return s;
}

void markGood(const QString &) {}
QStringList installed() { return {LOBS_ENGINE_VERSION}; }
QString previous() { return {}; }
bool activate(const QString &, bool) { return false; }
void adoptBundled(const QString &) {}

QStringList moduleAllowlist()
{
	QFile f(QCoreApplication::applicationDirPath() + "/../Resources/engine-modules.txt");
	QStringList out;
	if (f.open(QIODevice::ReadOnly | QIODevice::Text))
		for (const QString &line : QString::fromUtf8(f.readAll()).split('\n'))
			if (const QString t = line.trimmed(); !t.isEmpty() && !t.startsWith('#'))
				out << t;
	return out;
}

QStringList missingExports(const QString &) { return {}; }
QStringList lobsImports() { return {}; }

} // namespace engine_store

EngineUpdater::EngineUpdater(const engine_store::Selected &running, QObject *parent)
	: QObject(parent), m_running(running), m_net(nullptr)
{
}

bool EngineUpdater::beta() const { return false; }
void EngineUpdater::setBeta(bool) {}
QString EngineUpdater::lastChecked() const { return {}; }
void EngineUpdater::check() {}
void EngineUpdater::checkIfDue() {}
void EngineUpdater::install() {}
void EngineUpdater::rollback() {}
void EngineUpdater::restartLobs() {}
void EngineUpdater::installLocal(const QString &, const QString &) {}
void EngineUpdater::set(const QString &state, const QString &message)
{
	m_state = state;
	m_message = message;
	emit changed();
}
void EngineUpdater::downloadFinished() {}
void EngineUpdater::extract(const QString &) {}
void EngineUpdater::verify(const QString &) {}
