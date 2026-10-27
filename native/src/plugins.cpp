#include "plugins.h"

#include <obs.h>

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QtEndian>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif

namespace lobs::plugins {

// This run: what loaded (or didn't), and the folder as it was when it did.
static QVector<Plugin> g_run;
static QStringList g_inputs, g_filters;
static QSet<QString> g_types;
static QString g_startSig;

QString folder()
{
	// LOBS_PLUGINS_DIR points elsewhere (QA keeps one across runs, whose AppData starts empty).
	static const QString f = [] {
		const QString env = qEnvironmentVariable("LOBS_PLUGINS_DIR");
		const QString p = !env.isEmpty() ? QDir(env).absolutePath() : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
		QDir().mkpath(p + "/.lobs");
		return p;
	}();
	return f;
}

static QString state(const QString &file) { return folder() + "/.lobs/" + file; }

static QStringList readList(const QString &file)
{
	QFile f(state(file));
	if (!f.open(QIODevice::ReadOnly))
		return {};
	QStringList out = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
	for (QString &s : out)
		s = s.trimmed();
	out.removeAll(QString());
	out.removeDuplicates();
	return out;
}

static void writeList(const QString &file, const QStringList &items)
{
	QFile f(state(file));
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(items.join('\n').toUtf8());
}

static void trash(const QString &path)
{
	if (!QFileInfo::exists(path))
		return;
	if (!QFile::moveToTrash(path))
		QDir(path).removeRecursively();
}

static QStringList pluginDirs()
{
	QStringList out = QDir(folder()).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
	out.erase(std::remove_if(out.begin(), out.end(), [](const QString &n) { return n.startsWith('.'); }), out.end());
	return out;
}

#ifdef Q_OS_WIN
static const char *kLibExt = ".dll";
#else
static const char *kLibExt = ".so";
#endif

// The plugin's library: bin/64bit/<name>.dll, else the only library there.
static QString libraryOf(const QString &dir, const QString &name)
{
	const QString bin = dir + "/bin/64bit/";
	if (QFile::exists(bin + name + kLibExt))
		return bin + name + kLibExt;
	const QStringList libs = QDir(bin).entryList({QString("*") + kLibExt}, QDir::Files);
	return libs.size() == 1 ? bin + libs.first() : QString();
}

// Everything in the folder that decides what a start would load.
static QString folderSig()
{
	QStringList parts;
	for (const QString &n : pluginDirs()) {
		const QString lib = libraryOf(folder() + "/" + n, n);
		parts << n + "@" + QString::number(lib.isEmpty() ? 0 : QFileInfo(lib).lastModified().toMSecsSinceEpoch());
	}
	parts << "d:" + readList("disabled.txt").join(',') << "r:" + readList("remove.txt").join(',');
	parts << "p:" + QDir(state("pending")).entryList(QDir::Dirs | QDir::NoDotAndDotDot).join(',');
	return parts.join('|');
}

static QString fileVersion(const QString &path)
{
#ifdef Q_OS_WIN
	const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
	DWORD handle = 0;
	const DWORD size = GetFileVersionInfoSizeW(w.c_str(), &handle);
	if (!size)
		return {};
	QByteArray buf(int(size), 0);
	if (!GetFileVersionInfoW(w.c_str(), 0, size, buf.data()))
		return {};
	VS_FIXEDFILEINFO *fi = nullptr;
	UINT len = 0;
	if (!VerQueryValueW(buf.data(), L"\\", reinterpret_cast<void **>(&fi), &len) || !fi || !fi->dwFileVersionMS)
		return {};
	QString v = QString("%1.%2.%3").arg(HIWORD(fi->dwFileVersionMS)).arg(LOWORD(fi->dwFileVersionMS)).arg(HIWORD(fi->dwFileVersionLS));
	if (LOWORD(fi->dwFileVersionLS))
		v += "." + QString::number(LOWORD(fi->dwFileVersionLS));
	return v;
#else
	Q_UNUSED(path);
	return {};
#endif
}

#ifdef Q_OS_WIN
// The DLL names a PE32+ library imports directly.
static QStringList importsOf(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return {};
	const QByteArray b = f.readAll();
	const auto u32 = [&](qsizetype o) -> quint32 { return o >= 0 && o + 4 <= b.size() ? qFromLittleEndian<quint32>(b.constData() + o) : 0; };
	const auto u16 = [&](qsizetype o) -> quint16 { return o >= 0 && o + 2 <= b.size() ? qFromLittleEndian<quint16>(b.constData() + o) : 0; };
	const qsizetype pe = u32(0x3c);
	if (b.mid(pe, 4) != QByteArray("PE\0\0", 4))
		return {};
	const quint16 sections = u16(pe + 6), optSize = u16(pe + 20);
	const qsizetype opt = pe + 24;
	if (u16(opt) != 0x20b)
		return {}; // not 64-bit
	const quint32 importRva = u32(opt + 112 + 8);
	const qsizetype sec = opt + optSize;
	const auto offset = [&](quint32 rva) -> qsizetype {
		for (int i = 0; i < sections; ++i) {
			const qsizetype s = sec + i * 40;
			const quint32 va = u32(s + 12), size = std::max(u32(s + 8), u32(s + 16)), raw = u32(s + 20);
			if (rva >= va && rva < va + size)
				return raw + (rva - va);
		}
		return -1;
	};
	QStringList out;
	qsizetype d = importRva ? offset(importRva) : -1;
	for (int guard = 0; d >= 0 && guard < 512; ++guard, d += 20) {
		const quint32 nameRva = u32(d + 12);
		if (!nameRva)
			break;
		const qsizetype n = offset(nameRva);
		if (n < 0)
			break;
		out << QString::fromLatin1(b.constData() + n, int(qstrnlen(b.constData() + n, size_t(b.size() - n))));
	}
	return out;
}

// Why Windows couldn't load a plugin: the libraries it needs that aren't anywhere libobs looks.
static QString loadFailure(const QString &lib)
{
	const QString bin = QFileInfo(lib).absolutePath();
	QStringList missing;
	for (const QString &dep : importsOf(lib)) {
		const std::wstring w = dep.toStdWString();
		if (GetModuleHandleW(w.c_str()) || QFile::exists(bin + "/" + dep))
			continue;
		if (HMODULE h = LoadLibraryExW(w.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)) {
			FreeLibrary(h);
			continue;
		}
		missing << dep;
	}
	if (missing.contains("obs-frontend-api.dll", Qt::CaseInsensitive))
		return "Needs OBS's own window (obs-frontend-api.dll). LOBS can't load plugins that add docks or menus to OBS.";
	if (!missing.isEmpty())
		return "Needs " + missing.join(", ") + ", which " + (missing.first().startsWith("Qt6", Qt::CaseInsensitive) ? "LOBS doesn't ship." : "isn't installed.");
	return "Windows couldn't load it, maybe a library it needs is missing. The LOBS log has details.";
}
#endif

static QStringList enumTypes(bool (*fn)(size_t, const char **))
{
	QStringList out;
	const char *id = nullptr;
	for (size_t i = 0; fn(i, &id); ++i)
		out << QString::fromUtf8(id);
	return out;
}

static QStringList added(const QStringList &after, const QStringList &before)
{
	const QSet<QString> b(before.begin(), before.end());
	QStringList out;
	for (const QString &s : after)
		if (!b.contains(s))
			out << s;
	return out;
}

void loadAll()
{
	// Finish what the last run started: removals and updates of plugins that were in use.
	for (const QString &n : readList("remove.txt"))
		trash(folder() + "/" + n);
	writeList("remove.txt", {});
	for (const QString &n : QDir(state("pending")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		trash(folder() + "/" + n);
		QDir().rename(state("pending/") + n, folder() + "/" + n);
	}
	// A plugin that took LOBS down while loading stays off until it's turned back on.
	QStringList disabled = readList("disabled.txt"), crashed = readList("crashed.txt");
	if (QFile marker(state("loading")); marker.open(QIODevice::ReadOnly)) {
		const QString n = QString::fromUtf8(marker.readAll()).trimmed();
		marker.close();
		marker.remove();
		if (!n.isEmpty()) {
			blog(LOG_WARNING, "[lobs] plugin %s crashed LOBS while loading; it's off until turned on again", n.toUtf8().constData());
			disabled << n, crashed << n;
			disabled.removeDuplicates(), crashed.removeDuplicates();
			writeList("disabled.txt", disabled), writeList("crashed.txt", crashed);
		}
	}

	for (const QString &name : pluginDirs()) {
		const QString dir = folder() + "/" + name;
		Plugin p{name, dir, {}, name, {}, {}, {}, {}, {}, {}};
		const QString lib = libraryOf(dir, name);
		p.version = lib.isEmpty() ? QString() : fileVersion(lib);
		if (disabled.contains(name)) {
			p.status = crashed.contains(name) ? "crashed" : "disabled";
			p.reason = crashed.contains(name) ? "LOBS closed while loading it last time, so it's turned off." : "Turned off.";
			g_run << p;
			continue;
		}
		p.status = "failed";
		if (lib.isEmpty()) {
			p.reason = QString("No plugin library (bin/64bit/%1%2) in its folder.").arg(name, kLibExt);
			g_run << p;
			continue;
		}
		if (obs_get_module(QFileInfo(lib).completeBaseName().toUtf8().constData())) {
			p.reason = "LOBS already has a module with this name.";
			g_run << p;
			continue;
		}
		const QStringList inBefore = enumTypes(obs_enum_input_types), fBefore = enumTypes(obs_enum_filter_types),
				  tBefore = enumTypes(obs_enum_transition_types), oBefore = enumTypes(obs_enum_output_types) + enumTypes(obs_enum_encoder_types);
		if (QFile marker(state("loading")); marker.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			marker.write(name.toUtf8());
			marker.close();
		}
		blog(LOG_INFO, "[lobs] loading plugin %s", lib.toUtf8().constData());
		obs_module_t *mod = nullptr;
		const int r = obs_open_module(&mod, lib.toUtf8().constData(), (dir + "/data").toUtf8().constData());
		if (r == MODULE_SUCCESS && obs_init_module(mod)) {
			p.status = "loaded";
			if (const char *t = obs_get_module_name(mod); t && *t)
				p.title = QString::fromUtf8(t);
			p.sources = added(enumTypes(obs_enum_input_types), inBefore);
			p.filters = added(enumTypes(obs_enum_filter_types), fBefore);
			p.transitions = added(enumTypes(obs_enum_transition_types), tBefore);
			p.other = added(enumTypes(obs_enum_output_types) + enumTypes(obs_enum_encoder_types), oBefore);
			g_inputs << p.sources, g_filters << p.filters;
			for (const QStringList *l : {&p.sources, &p.filters, &p.transitions, &p.other})
				for (const QString &t : *l)
					g_types.insert(t);
		} else if (r == MODULE_SUCCESS) {
			p.reason = "The plugin's own start-up failed. The LOBS log has details.";
		} else if (r == MODULE_INCOMPATIBLE_VER) {
			p.reason = "Made for a newer OBS than this engine (" + QString(obs_get_version_string()) + ").";
		} else if (r == MODULE_MISSING_EXPORTS) {
			p.reason = "Not an OBS plugin (it doesn't export obs_module_load).";
		} else if (r == MODULE_HARDCODED_SKIP) {
			p.reason = "OBS blocks this plugin.";
		} else {
#ifdef Q_OS_WIN
			p.reason = loadFailure(lib);
#else
			p.reason = "It couldn't be opened. The LOBS log has details.";
#endif
		}
		QFile::remove(state("loading"));
		if (p.status != "loaded")
			blog(LOG_WARNING, "[lobs] plugin %s not loaded: %s", name.toUtf8().constData(), p.reason.toUtf8().constData());
		g_run << p;
	}
	g_startSig = folderSig();
}

QVector<Plugin> list()
{
	const QStringList removing = readList("remove.txt"), disabled = readList("disabled.txt");
	const QStringList pending = QDir(state("pending")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	QHash<QString, Plugin> ran;
	for (const Plugin &p : g_run)
		ran[p.name] = p;
	QVector<Plugin> out;
	QStringList names = pluginDirs();
	for (const Plugin &p : g_run)
		if (!names.contains(p.name))
			names << p.name; // removed since start, but still in use until a restart
	for (const QString &n : names) {
		Plugin p = ran.value(n, Plugin{n, folder() + "/" + n, {}, n, "pending", "Loads when LOBS restarts.", {}, {}, {}, {}});
		if (!ran.contains(n))
			p.version = fileVersion(libraryOf(p.folder, n));
		if (removing.contains(n) || !QFileInfo::exists(p.folder))
			p.status = "removing", p.reason = "Removed when LOBS restarts.";
		else if (pending.contains(n))
			p.status = "pending", p.reason = "Updated. The new version loads when LOBS restarts.";
		else if (p.status == "loaded" && disabled.contains(n))
			p.status = "pending", p.reason = "Turns off when LOBS restarts.";
		else if ((p.status == "disabled" || p.status == "crashed") && !disabled.contains(n))
			p.status = "pending", p.reason = "Turns on when LOBS restarts.";
		out << p;
	}
	return out;
}

bool restartNeeded() { return folderSig() != g_startSig; }

static bool copyTree(const QString &from, const QString &to)
{
	QDir().mkpath(to);
	QDirIterator it(from, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString src = it.next();
		const QString dst = to + "/" + QDir(from).relativeFilePath(src);
		if (it.fileInfo().isDir())
			QDir().mkpath(dst);
		else if (!QFile::copy(src, dst))
			return false;
	}
	return true;
}

static bool loadedThisRun(const QString &name)
{
	for (const Plugin &p : g_run)
		if (p.name == name && p.status == "loaded")
			return true;
	return false;
}

struct Found { QString name; QStringList binFiles; QString data; };

// Plugins in an unpacked release, in either shape OBS 32 plugins ship in.
static QVector<Found> findPlugins(const QString &root)
{
	QVector<Found> out;
	// <name>/<name>.dll and <name>/data: the flat layout some plugins ship for OBS 32's plugin folder.
	QStringList flat{root};
	for (QDirIterator d(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories); d.hasNext();)
		flat << d.next();
	for (const QString &dir : flat) {
		const QDir d(dir);
		const QString name = d.dirName();
		if (name.compare("64bit", Qt::CaseInsensitive) == 0 || !QFile::exists(d.filePath(name + kLibExt)))
			continue;
		QStringList files;
		for (const QString &f : d.entryList(QDir::Files))
			files << d.filePath(f);
		out << Found{name, files, d.filePath("data")};
	}
	QDirIterator it(root, {"64bit"}, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QDir d(it.next());
		QDir parent(d), base(d);
		parent.cdUp();
		base.cdUp(), base.cdUp();
		const QStringList libs = d.entryList({QString("*") + kLibExt}, QDir::Files);
		if (libs.isEmpty())
			continue;
		if (parent.dirName().compare("bin", Qt::CaseInsensitive) == 0) {
			// <name>/bin/64bit and <name>/data: OBS's per-user layout.
			QString name = base.dirName();
			if (libs.size() == 1 && QFileInfo(libs.first()).completeBaseName() != name && !QFile::exists(d.filePath(name + kLibExt)))
				name = QFileInfo(libs.first()).completeBaseName();
			QStringList files;
			for (const QString &f : d.entryList(QDir::Files))
				files << d.filePath(f);
			out << Found{name, files, base.filePath("data")};
		} else if (parent.dirName().compare("obs-plugins", Qt::CaseInsensitive) == 0) {
			// obs-plugins/64bit/<name>.dll and data/obs-plugins/<name>: OBS's install layout. A
			// library with no data folder is a dependency, and goes with every plugin here.
			QStringList plugins, deps;
			for (const QString &l : libs)
				(QDir(base.filePath("data/obs-plugins/" + QFileInfo(l).completeBaseName())).exists() ? plugins : deps) << l;
			if (plugins.isEmpty() && libs.size() == 1)
				plugins = libs, deps.clear();
			for (const QString &l : plugins) {
				const QString name = QFileInfo(l).completeBaseName();
				QStringList files{d.filePath(l)};
				if (QFile::exists(d.filePath(name + ".pdb")))
					files << d.filePath(name + ".pdb");
				for (const QString &dep : deps)
					files << d.filePath(dep);
				out << Found{name, files, base.filePath("data/obs-plugins/" + name)};
			}
		}
	}
	return out;
}

QString install(const QString &path, QStringList *names)
{
	const QFileInfo fi(path);
	if (!fi.exists())
		return "That file or folder isn't there.";
	QString root = fi.absoluteFilePath();
	const QString stage = state("staging-" + QString::number(QDateTime::currentMSecsSinceEpoch()));
	struct Cleanup { QString dir; ~Cleanup() { if (!dir.isEmpty()) QDir(dir).removeRecursively(); } } cleanup;
	if (fi.isFile()) {
		if (fi.suffix().compare("zip", Qt::CaseInsensitive) != 0)
			return fi.suffix().compare("exe", Qt::CaseInsensitive) == 0 || fi.suffix().compare("msi", Qt::CaseInsensitive) == 0
				       ? "That's an installer. Download the plugin's .zip for Windows instead."
				       : "Choose a plugin's .zip or its folder.";
		QDir().mkpath(stage);
		cleanup.dir = stage;
		QProcess tar;
#ifdef Q_OS_WIN
		tar.setProgram(QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/tar.exe"));
#else
		tar.setProgram("/usr/bin/tar");
#endif
		tar.setArguments({"-xf", QDir::toNativeSeparators(root), "-C", QDir::toNativeSeparators(stage)});
		tar.start();
		if (!tar.waitForFinished(120000) || tar.exitCode() != 0)
			return "Couldn't unpack the .zip: " + QString::fromLocal8Bit(tar.readAllStandardError()).trimmed();
		root = stage;
	}
	const QVector<Found> found = findPlugins(root);
	if (found.isEmpty()) {
		QDirIterator it(root, {"*.exe", "*.msi"}, QDir::Files, QDirIterator::Subdirectories);
		return it.hasNext() ? "This has an installer, not plugin files. Download the plugin's .zip for Windows instead."
				    : "No OBS plugin for 64-bit Windows in it (no obs-plugins/64bit or bin/64bit folder with a .dll).";
	}
	for (const Found &f : found) {
		// A plugin in use can't be replaced until LOBS lets go of it: it's swapped in at the next start.
		const QString target = loadedThisRun(f.name) ? state("pending/") + f.name : folder() + "/" + f.name;
		QDir(target).removeRecursively();
		QDir().mkpath(target + "/bin/64bit");
		for (const QString &file : f.binFiles)
			if (!QFile::copy(file, target + "/bin/64bit/" + QFileInfo(file).fileName()))
				return "Couldn't copy " + QFileInfo(file).fileName() + " into the plugins folder.";
		if (QDir(f.data).exists() && !copyTree(f.data, target + "/data"))
			return "Couldn't copy " + f.name + "'s data into the plugins folder.";
		QStringList removing = readList("remove.txt");
		removing.removeAll(f.name);
		writeList("remove.txt", removing);
		if (names)
			*names << f.name;
	}
	return {};
}

QString remove(const QString &name)
{
	if (name.isEmpty() || name.startsWith('.') || name.contains('/') || name.contains('\\'))
		return "No such plugin.";
	trash(state("pending/") + name);
	for (const QString &file : {"disabled.txt", "crashed.txt"}) {
		QStringList l = readList(file);
		l.removeAll(name);
		writeList(file, l);
	}
	if (loadedThisRun(name)) {
		QStringList l = readList("remove.txt");
		l << name;
		l.removeDuplicates();
		writeList("remove.txt", l);
		return {};
	}
	trash(folder() + "/" + name);
	return QFileInfo::exists(folder() + "/" + name) ? "Couldn't remove its folder. Is a file in it open?" : QString();
}

QString setEnabled(const QString &name, bool on)
{
	QStringList d = readList("disabled.txt"), c = readList("crashed.txt");
	d.removeAll(name), c.removeAll(name);
	if (!on)
		d << name;
	writeList("disabled.txt", d), writeList("crashed.txt", c);
	return {};
}

QStringList inputTypes() { return g_inputs; }
QStringList filterTypes() { return g_filters; }
bool isPluginType(const QString &obsId) { return g_types.contains(obsId); }

// ---------- generic properties editor ----------

static QString colorHex(long long v, bool alpha)
{
	const QColor c(int(v & 0xff), int((v >> 8) & 0xff), int((v >> 16) & 0xff), int((v >> 24) & 0xff));
	return alpha ? c.name(QColor::HexArgb) : c.name(QColor::HexRgb);
}

static void describeInto(obs_properties_t *props, obs_data_t *st, int depth, QVariantList &out)
{
	for (obs_property_t *p = obs_properties_first(props); p; obs_property_next(&p)) {
		const char *key = obs_property_name(p);
		QVariantMap m{{"id", QString::fromUtf8(key)}, {"key", QString::fromUtf8(key)}, {"label", QString::fromUtf8(obs_property_description(p))},
			      {"tip", QString::fromUtf8(obs_property_long_description(p))}, {"visible", obs_property_visible(p)},
			      {"enabled", obs_property_enabled(p)}, {"depth", depth}};
		obs_properties_t *children = nullptr;
		switch (obs_property_get_type(p)) {
		case OBS_PROPERTY_BOOL:
			m["type"] = "bool", m["value"] = obs_data_get_bool(st, key);
			break;
		case OBS_PROPERTY_INT:
			m["type"] = "number", m["value"] = qlonglong(obs_data_get_int(st, key));
			m["min"] = obs_property_int_min(p), m["max"] = obs_property_int_max(p), m["step"] = obs_property_int_step(p);
			m["decimals"] = 0, m["suffix"] = QString::fromUtf8(obs_property_int_suffix(p));
			m["slider"] = obs_property_int_type(p) == OBS_NUMBER_SLIDER;
			break;
		case OBS_PROPERTY_FLOAT: {
			const double step = obs_property_float_step(p);
			m["type"] = "number", m["value"] = obs_data_get_double(st, key);
			m["min"] = obs_property_float_min(p), m["max"] = obs_property_float_max(p), m["step"] = step;
			m["decimals"] = step >= 1 ? 0 : step >= 0.1 ? 1 : step >= 0.01 ? 2 : 3;
			m["suffix"] = QString::fromUtf8(obs_property_float_suffix(p));
			m["slider"] = obs_property_float_type(p) == OBS_NUMBER_SLIDER;
			break;
		}
		case OBS_PROPERTY_TEXT: {
			const obs_text_type t = obs_property_text_type(p);
			m["type"] = t == OBS_TEXT_INFO ? "info" : "text", m["value"] = QString::fromUtf8(obs_data_get_string(st, key));
			m["multiline"] = t == OBS_TEXT_MULTILINE, m["password"] = t == OBS_TEXT_PASSWORD;
			break;
		}
		case OBS_PROPERTY_PATH: {
			m["type"] = "path", m["value"] = QString::fromUtf8(obs_data_get_string(st, key));
			m["folder"] = obs_property_path_type(p) == OBS_PATH_DIRECTORY, m["save"] = obs_property_path_type(p) == OBS_PATH_FILE_SAVE;
			// OBS writes filters as "Images (*.png *.jpg);;All Files (*.*)".
			QStringList filters = QString::fromUtf8(obs_property_path_filter(p)).split(";;", Qt::SkipEmptyParts);
			m["filters"] = filters.isEmpty() ? QStringList{"All files (*)"} : filters;
			break;
		}
		case OBS_PROPERTY_LIST: {
			const obs_combo_format f = obs_property_list_format(p);
			QVariantList options;
			for (size_t i = 0, n = obs_property_list_item_count(p); i < n; ++i) {
				QVariant v;
				switch (f) {
				case OBS_COMBO_FORMAT_INT: v = QString::number(obs_property_list_item_int(p, i)); break;
				case OBS_COMBO_FORMAT_FLOAT: v = QString::number(obs_property_list_item_float(p, i)); break;
				case OBS_COMBO_FORMAT_BOOL: v = obs_property_list_item_bool(p, i) ? "true" : "false"; break;
				default: v = QString::fromUtf8(obs_property_list_item_string(p, i)); break;
				}
				if (!obs_property_list_item_disabled(p, i))
					options << QVariantMap{{"name", QString::fromUtf8(obs_property_list_item_name(p, i))}, {"value", v}};
			}
			m["type"] = "list", m["options"] = options;
			// Values go to QML as text (FSelect compares strings) and back through toObsValue.
			m["value"] = f == OBS_COMBO_FORMAT_INT ? QString::number(obs_data_get_int(st, key))
				     : f == OBS_COMBO_FORMAT_FLOAT ? QString::number(obs_data_get_double(st, key))
				     : f == OBS_COMBO_FORMAT_BOOL ? QString(obs_data_get_bool(st, key) ? "true" : "false")
								  : QString::fromUtf8(obs_data_get_string(st, key));
			m["editable"] = obs_property_list_type(p) == OBS_COMBO_TYPE_EDITABLE;
			break;
		}
		case OBS_PROPERTY_COLOR:
		case OBS_PROPERTY_COLOR_ALPHA: {
			const bool alpha = obs_property_get_type(p) == OBS_PROPERTY_COLOR_ALPHA;
			m["type"] = "color", m["value"] = colorHex(obs_data_get_int(st, key), false), m["alpha"] = alpha;
			m["opacity"] = alpha ? double((obs_data_get_int(st, key) >> 24) & 0xff) / 255.0 : 1.0;
			break;
		}
		case OBS_PROPERTY_FONT: {
			obs_data_t *font = obs_data_get_obj(st, key);
			m["type"] = "font", m["value"] = QString::fromUtf8(obs_data_get_string(font, "face"));
			m["size"] = qlonglong(obs_data_get_int(font, "size"));
			obs_data_release(font);
			break;
		}
		case OBS_PROPERTY_BUTTON:
			m["type"] = "button";
			if (obs_property_button_type(p) == OBS_BUTTON_URL)
				m["url"] = QString::fromUtf8(obs_property_button_url(p));
			break;
		case OBS_PROPERTY_GROUP:
			m["type"] = "group";
			m["checkable"] = obs_property_group_type(p) == OBS_GROUP_CHECKABLE;
			m["value"] = obs_property_group_type(p) == OBS_GROUP_CHECKABLE ? obs_data_get_bool(st, key) : true;
			children = obs_property_group_content(p);
			break;
		default: // editable lists, frame rates
			m["type"] = "unsupported";
			break;
		}
		out << m;
		if (children && (!m["checkable"].toBool() || m["value"].toBool()))
			describeInto(children, st, depth + 1, out);
	}
}

QVariantList describe(obs_source_t *src)
{
	QVariantList out;
	if (!src)
		return out;
	obs_properties_t *props = obs_source_properties(src);
	if (!props)
		return out;
	obs_data_t *st = obs_source_get_settings(src);
	obs_properties_apply_settings(props, st); // runs the "modified" callbacks that show and hide settings
	describeInto(props, st, 0, out);
	obs_data_release(st);
	obs_properties_destroy(props);
	return out;
}

bool clickButton(obs_source_t *src, const QString &key)
{
	if (!src)
		return false;
	obs_properties_t *props = obs_source_properties(src);
	bool refresh = false;
	if (obs_property_t *p = props ? obs_properties_get(props, key.toUtf8().constData()) : nullptr)
		refresh = obs_property_button_clicked(p, src);
	obs_properties_destroy(props);
	return refresh;
}

// Lists whose value isn't one of their choices, with the first choice, converted for OBS.
static void unsetLists(obs_properties_t *props, obs_data_t *st, QVariantMap &out)
{
	for (obs_property_t *p = obs_properties_first(props); p; obs_property_next(&p)) {
		const char *key = obs_property_name(p);
		if (obs_property_get_type(p) == OBS_PROPERTY_GROUP) {
			if (obs_property_group_type(p) != OBS_GROUP_CHECKABLE || obs_data_get_bool(st, key))
				unsetLists(obs_property_group_content(p), st, out);
			continue;
		}
		if (obs_property_get_type(p) != OBS_PROPERTY_LIST || !obs_property_visible(p) || obs_property_list_type(p) == OBS_COMBO_TYPE_EDITABLE)
			continue;
		const obs_combo_format f = obs_property_list_format(p);
		const size_t n = obs_property_list_item_count(p);
		if (!n)
			continue;
		bool found = false;
		for (size_t i = 0; i < n && !found; ++i) {
			switch (f) {
			case OBS_COMBO_FORMAT_INT: found = obs_property_list_item_int(p, i) == obs_data_get_int(st, key); break;
			case OBS_COMBO_FORMAT_FLOAT: found = obs_property_list_item_float(p, i) == obs_data_get_double(st, key); break;
			case OBS_COMBO_FORMAT_BOOL: found = obs_property_list_item_bool(p, i) == obs_data_get_bool(st, key); break;
			default: found = qstrcmp(obs_property_list_item_string(p, i), obs_data_get_string(st, key)) == 0; break;
			}
		}
		if (found)
			continue;
		switch (f) {
		case OBS_COMBO_FORMAT_INT: out[key] = qlonglong(obs_property_list_item_int(p, 0)); break;
		case OBS_COMBO_FORMAT_FLOAT: out[key] = obs_property_list_item_float(p, 0); break;
		case OBS_COMBO_FORMAT_BOOL: out[key] = obs_property_list_item_bool(p, 0); break;
		default: out[key] = QString::fromUtf8(obs_property_list_item_string(p, 0)); break;
		}
	}
}

QVariantMap settleLists(obs_source_t *src)
{
	QVariantMap out;
	obs_properties_t *props = src ? obs_source_properties(src) : nullptr;
	if (!props)
		return out;
	obs_data_t *st = obs_source_get_settings(src);
	obs_properties_apply_settings(props, st);
	unsetLists(props, st, out);
	obs_data_release(st);
	obs_properties_destroy(props);
	return out;
}

static QJsonObject json(obs_data_t *d)
{
	const char *j = d ? obs_data_get_json(d) : nullptr;
	return j ? QJsonDocument::fromJson(QByteArray(j)).object() : QJsonObject();
}

QVariantMap modified(obs_source_t *src, const QString &key, const QVariant &obsValue)
{
	QVariantMap out;
	obs_properties_t *props = src ? obs_source_properties(src) : nullptr;
	obs_property_t *p = props ? obs_properties_get(props, key.toUtf8().constData()) : nullptr;
	if (p) {
		obs_data_t *live = obs_source_get_settings(src);
		obs_data_t *st = obs_data_create();
		obs_data_apply(st, live);
		obs_data_t *v = obs_data_create_from_json(QJsonDocument(QJsonObject{{key, QJsonValue::fromVariant(obsValue)}}).toJson(QJsonDocument::Compact).constData());
		obs_data_apply(st, v);
		const QJsonObject before = json(st);
		obs_property_modified(p, st);
		const QJsonObject after = json(st);
		for (auto it = after.begin(); it != after.end(); ++it)
			if (it.key() != key && before.value(it.key()) != it.value())
				out[it.key()] = it.value().toVariant();
		obs_data_release(v);
		obs_data_release(st);
		obs_data_release(live);
	}
	obs_properties_destroy(props);
	return out;
}

QVariant toObsValue(obs_source_t *src, const QString &key, const QVariant &v)
{
	obs_properties_t *props = src ? obs_source_properties(src) : nullptr;
	obs_property_t *p = props ? obs_properties_get(props, key.toUtf8().constData()) : nullptr;
	QVariant out = v;
	if (p) {
		switch (obs_property_get_type(p)) {
		case OBS_PROPERTY_INT: out = qlonglong(std::llround(v.toDouble())); break;
		case OBS_PROPERTY_FLOAT: out = v.toDouble(); break;
		case OBS_PROPERTY_BOOL: out = v.toBool(); break;
		case OBS_PROPERTY_GROUP: out = v.toBool(); break;
		case OBS_PROPERTY_COLOR:
		case OBS_PROPERTY_COLOR_ALPHA: {
			// "#rrggbb" or "#aarrggbb" -> OBS's 0xAABBGGRR
			const QColor c(v.toString());
			const int a = obs_property_get_type(p) == OBS_PROPERTY_COLOR || v.toString().size() <= 7 ? 255 : c.alpha();
			out = qlonglong((quint32(a) << 24) | (quint32(c.blue()) << 16) | (quint32(c.green()) << 8) | quint32(c.red()));
			break;
		}
		case OBS_PROPERTY_LIST:
			switch (obs_property_list_format(p)) {
			case OBS_COMBO_FORMAT_INT: out = v.toString().toLongLong(); break;
			case OBS_COMBO_FORMAT_FLOAT: out = v.toString().toDouble(); break;
			case OBS_COMBO_FORMAT_BOOL: out = v.toString() == "true"; break;
			default: out = v.toString(); break;
			}
			break;
		case OBS_PROPERTY_FONT: {
			obs_data_t *st = obs_source_get_settings(src);
			obs_data_t *font = obs_data_get_obj(st, key.toUtf8().constData());
			QVariantMap f{{"face", v.toString()}, {"style", "Regular"}, {"size", qlonglong(font ? obs_data_get_int(font, "size") : 48)},
				      {"flags", qlonglong(font ? obs_data_get_int(font, "flags") : 0)}};
			obs_data_release(font);
			obs_data_release(st);
			out = f;
			break;
		}
		default: out = v.toString(); break;
		}
	}
	obs_properties_destroy(props);
	return out;
}

} // namespace lobs::plugins
