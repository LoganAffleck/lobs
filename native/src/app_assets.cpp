#include "app_internal.h"
#include "platform.h"
#include "shellmeta.h"

#include <util/base.h>

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QRegularExpression>
#include <QUrl>
#include <algorithm>
#include <cmath>

using namespace lobs;

QString assetKind(const QString &file)
{
	static const QStringList images = {"png", "jpg", "jpeg", "gif", "webp", "bmp", "tga", "psd"};
	static const QStringList videos = {"mp4", "mov", "mkv", "webm", "avi", "flv", "ts", "m4v"};
	static const QStringList audio = {"mp3", "wav", "ogg", "flac", "m4a", "aac", "opus"};
	const QString ext = QFileInfo(file).suffix().toLower();
	return images.contains(ext) ? "image" : videos.contains(ext) ? "video" : audio.contains(ext) ? "audio" : "other";
}

QString App::assetsFolder() const
{
	const QString base = m_projectFile.isEmpty() ? QFileInfo(m_docPath).absolutePath() : QFileInfo(m_projectFile).absolutePath();
	return QDir::cleanPath(base + "/assets");
}

void App::watchAssets()
{
	if (!m_assetWatch.directories().isEmpty())
		m_assetWatch.removePaths(m_assetWatch.directories());
	QDir().mkpath(assetsFolder());
	if (!QDir(currentAssetDir()).exists())
		m_assetDir.clear();
	m_assetWatch.addPath(currentAssetDir());
}

QString App::currentAssetDir() const
{
	return m_assetDir.isEmpty() ? assetsFolder() : QDir::cleanPath(assetsFolder() + "/" + m_assetDir);
}

bool App::insideAssets(const QString &path) const
{
	const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath()), r = assetsFolder();
	return p.compare(r, Qt::CaseInsensitive) == 0 || p.startsWith(r + "/", Qt::CaseInsensitive);
}

int App::filesInUse(const QString &path) const
{
	const QString p = QDir::cleanPath(path);
	int n = 0;
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		const QString f = QDir::cleanPath(s.settings.value("file").toString());
		if ((s.kind == "image" || s.kind == "media") && (f.compare(p, Qt::CaseInsensitive) == 0 || f.startsWith(p + "/", Qt::CaseInsensitive)))
			++n;
	}
	return n;
}

void App::repointFiles(const QString &from, const QString &to)
{
	const QString a = QDir::cleanPath(from), b = QDir::cleanPath(to);
	m_store.rewriteAll([&](Doc &d) {
		for (Source &s : d.sources) {
			if (s.kind != "image" && s.kind != "media")
				continue;
			const QString f = QDir::cleanPath(s.settings.value("file").toString());
			if (f.compare(a, Qt::CaseInsensitive) == 0)
				s.settings["file"] = b;
			else if (f.startsWith(a + "/", Qt::CaseInsensitive))
				s.settings["file"] = b + f.mid(a.size());
		}
	});
}

QVariantList App::assetCrumbs() const
{
	QVariantList out{QVariantMap{{"name", "Assets"}, {"path", ""}, {"dir", assetsFolder()}}};
	QString acc;
	for (const QString &part : m_assetDir.split('/', Qt::SkipEmptyParts)) {
		acc += (acc.isEmpty() ? "" : "/") + part;
		out << QVariantMap{{"name", part}, {"path", acc}, {"dir", QDir::cleanPath(assetsFolder() + "/" + acc)}};
	}
	return out;
}

void App::goToAssetPath(const QString &relative)
{
	const QString target = relative.isEmpty() ? assetsFolder() : QDir::cleanPath(assetsFolder() + "/" + relative);
	if (!QDir(target).exists() || !insideAssets(target))
		return;
	m_assetDir = relative.isEmpty() ? QString() : QDir(assetsFolder()).relativeFilePath(target);
	watchAssets();
	emit assetsChanged();
}

void App::openAssetFolder(const QString &path)
{
	if (!QFileInfo(path).isDir() || !insideAssets(path))
		return;
	goToAssetPath(QDir(assetsFolder()).relativeFilePath(QDir::cleanPath(path)));
}

void App::assetUp()
{
	if (!m_assetDir.isEmpty())
		goToAssetPath(m_assetDir.contains('/') ? m_assetDir.section('/', 0, -2) : QString());
}

static QString uniqueIn(const QString &dir, const QString &name, bool isDir)
{
	const QFileInfo fi(name);
	const QString base = isDir ? name : fi.completeBaseName(), ext = isDir || fi.suffix().isEmpty() ? QString() : "." + fi.suffix();
	QString candidate = dir + "/" + name;
	for (int n = 2; QFileInfo::exists(candidate); ++n)
		candidate = dir + "/" + base + QString(" (%1)").arg(n) + ext;
	return candidate;
}

QString App::createAssetFolder()
{
	const QString path = uniqueIn(currentAssetDir(), "New folder", true);
	if (!QDir().mkpath(path))
		return showToast("Couldn't create a folder here"), QString();
	emit assetsChanged();
	return path;
}

bool App::renameAsset(const QString &path, const QString &name)
{
	QString n = name.trimmed();
	static const QRegularExpression bad(R"([\\/:*?"<>|])");
	if (n.isEmpty() || n.contains(bad) || n == "." || n == "..")
		return showToast("Names can't be empty or contain \\ / : * ? \" < > |"), false;
	const QFileInfo fi(path);
	if (!fi.exists() || !insideAssets(path))
		return false;
	if (fi.isFile() && QFileInfo(n).suffix().isEmpty() && !fi.suffix().isEmpty())
		n += "." + fi.suffix();
	const QString target = fi.absolutePath() + "/" + n;
	if (target == QDir::cleanPath(fi.absoluteFilePath()))
		return true;
	if (QFileInfo::exists(target) && target.compare(fi.absoluteFilePath(), Qt::CaseInsensitive) != 0)
		return showToast("There's already an item with that name"), false;
	if (!QDir().rename(fi.absoluteFilePath(), target))
		return showToast("Couldn't rename " + fi.fileName()), false;
	repointFiles(fi.absoluteFilePath(), target);
	if (fi.isFile()) {
		const QString base = QFileInfo(n).completeBaseName();
		m_store.amend([&](Doc &d) {
			for (Source &s : d.sources)
				if (QDir::cleanPath(s.settings.value("file").toString()).compare(target, Qt::CaseInsensitive) == 0) {
					const QString old = s.name;
					s.name = base;
					for (Instance &i : d.instances)
						if (i.sourceId == s.id && i.name == old)
							i.name = base;
				}
		});
	}
	emit assetsChanged();
	return true;
}

static void putFilesOnClipboard(const QStringList &paths, bool cut)
{
	auto *mime = new QMimeData;
	QList<QUrl> urls;
	for (const QString &p : paths)
		urls << QUrl::fromLocalFile(p);
	mime->setUrls(urls);
	const quint32 effect = cut ? 2 : 5;
	mime->setData("Preferred DropEffect", QByteArray(reinterpret_cast<const char *>(&effect), 4));
	QGuiApplication::clipboard()->setMimeData(mime);
}

void App::copyAssets(const QStringList &paths)
{
	if (paths.isEmpty())
		return;
	putFilesOnClipboard(paths, false);
	showToast(paths.size() == 1 ? "Copied " + QFileInfo(paths.first()).fileName() : QString("Copied %1 items").arg(paths.size()));
}

void App::cutAssets(const QStringList &paths)
{
	if (paths.isEmpty())
		return;
	putFilesOnClipboard(paths, true);
	showToast(paths.size() == 1 ? "Cut " + QFileInfo(paths.first()).fileName() : QString("Cut %1 items").arg(paths.size()));
}

bool App::clipboardHasFiles() const
{
	const QMimeData *m = QGuiApplication::clipboard()->mimeData();
	if (!m || !m->hasUrls())
		return false;
	for (const QUrl &u : m->urls())
		if (u.isLocalFile() && QFileInfo::exists(u.toLocalFile()))
			return true;
	return false;
}

static bool copyRecursive(const QString &from, const QString &to)
{
	if (QFileInfo(from).isFile())
		return QFile::copy(from, to);
	if (!QDir().mkpath(to))
		return false;
	bool ok = true;
	for (const QFileInfo &fi : QDir(from).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
		ok &= copyRecursive(fi.absoluteFilePath(), to + "/" + fi.fileName());
	return ok;
}

void App::moveAssets(const QStringList &paths, const QString &destFolder)
{
	const QString dest = QDir::cleanPath(destFolder);
	if (!QFileInfo(dest).isDir() || !insideAssets(dest))
		return;
	int moved = 0;
	for (const QString &p : paths) {
		const QFileInfo fi(p);
		const QString src = QDir::cleanPath(fi.absoluteFilePath());
		if (!fi.exists() || QDir::cleanPath(fi.absolutePath()) == dest)
			continue;
		if (dest.compare(src, Qt::CaseInsensitive) == 0 || dest.startsWith(src + "/", Qt::CaseInsensitive)) {
			showToast("A folder can't go inside itself");
			continue;
		}
		const QString target = uniqueIn(dest, fi.fileName(), fi.isDir());
		bool ok = QDir().rename(src, target);
		if (!ok && copyRecursive(src, target))
			ok = fi.isDir() ? QDir(src).removeRecursively() : QFile::remove(src);
		if (!ok)
			continue;
		if (insideAssets(src))
			repointFiles(src, target);
		++moved;
	}
	if (moved)
		showToast(moved == 1 ? "Moved 1 item" : QString("Moved %1 items").arg(moved));
	emit assetsChanged();
}

void App::pasteAssets()
{
	const QMimeData *m = QGuiApplication::clipboard()->mimeData();
	if (!m || !m->hasUrls())
		return showToast("Nothing to paste");
	QStringList paths;
	for (const QUrl &u : m->urls())
		if (u.isLocalFile() && QFileInfo::exists(u.toLocalFile()))
			paths << QDir::cleanPath(u.toLocalFile());
	const QByteArray fx = m->data("Preferred DropEffect");
	const bool cut = fx.size() >= 4 && (*reinterpret_cast<const quint32 *>(fx.constData()) & 2);
	if (cut) {
		moveAssets(paths, currentAssetDir());
		QGuiApplication::clipboard()->clear();
		return;
	}
	int copied = 0;
	for (const QString &p : paths) {
		const QFileInfo fi(p);
		const QString dest = currentAssetDir();
		if (fi.isDir() && (dest.compare(p, Qt::CaseInsensitive) == 0 || dest.startsWith(p + "/", Qt::CaseInsensitive)))
			continue;
		const QString target = uniqueIn(dest, fi.fileName(), fi.isDir());
		if (copyRecursive(p, target))
			++copied;
	}
	showToast(copied == 1 ? "Pasted 1 item" : QString("Pasted %1 items").arg(copied));
	emit assetsChanged();
}

void App::trashAssets(const QStringList &paths)
{
	int trashed = 0, blocked = 0;
	for (const QString &p : paths) {
		if (!insideAssets(p) || QDir::cleanPath(p) == assetsFolder())
			continue;
		if (filesInUse(p)) {
			++blocked;
			continue;
		}
		if (QFile::moveToTrash(p))
			++trashed;
	}
	if (blocked)
		showToast(QString("Kept %1 item%2 that layers use. Remove those layers first.").arg(blocked).arg(blocked == 1 ? "" : "s"));
	else if (trashed)
		showToast(trashed == 1 ? "Moved 1 item to the Recycle Bin" : QString("Moved %1 items to the Recycle Bin").arg(trashed));
	watchAssets();
	emit assetsChanged();
}

QVariantMap App::assetEntry(const QFileInfo &fi, const QHash<QString, QStringList> &scenesByFile) const
{
	const QString path = QDir::cleanPath(fi.absoluteFilePath());
	const QString rel = QFileInfo(QDir(assetsFolder()).relativeFilePath(path)).path();
	const QString where = rel == "." ? QString() : rel;
	if (fi.isDir()) {
		const int items = int(QDir(path).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).size());
		return {{"name", fi.fileName()}, {"path", path}, {"kind", "folder"}, {"icon", "folder"}, {"folder", true}, {"color", folderColor(path)},
			{"size", items == 1 ? QString("1 item") : QString("%1 items").arg(items)}, {"bytes", 0}, {"modified", fi.lastModified().toMSecsSinceEpoch()},
			{"uses", filesInUse(path)}, {"scenes", QStringList()}, {"placeable", false}, {"where", where}};
	}
	const QString k = assetKind(path);
	const double kb = double(fi.size()) / 1024.0;
	QString dims;
	qint64 durMs = 0;
	if (k == "image" || k == "video" || k == "audio") {
		const ShellMeta m = shellMeta(path);
		if (!m.size.isEmpty())
			dims = QString("%1×%2").arg(m.size.width()).arg(m.size.height());
		durMs = m.durationMs;
	}
	const QStringList scenes = scenesByFile.value(path.toLower());
	return {{"name", fi.fileName()}, {"path", path}, {"url", QUrl::fromLocalFile(path).toString()}, {"kind", k},
		{"icon", k == "image" ? "image" : k == "video" ? "film" : k == "audio" ? "audio-lines" : "file"},
		{"size", kb > 1024 ? QString("%1 MB").arg(kb / 1024, 0, 'f', 1) : QString("%1 KB").arg(kb, 0, 'f', 0)},
		{"bytes", double(fi.size())}, {"modified", fi.lastModified().toMSecsSinceEpoch()},
		{"dims", dims}, {"durationMs", durMs}, {"ext", fi.suffix().toUpper()},
		{"uses", int(scenes.size()) ? usesOf(path) : 0}, {"scenes", scenes},
		{"thumb", (k == "image" || k == "video") ? "image://assetthumb/" + QString::fromLatin1(QUrl::toPercentEncoding(path)) + "?" + QString::number(fi.lastModified().toMSecsSinceEpoch()) : QString()},
		{"placeable", k != "other"}, {"folder", false}, {"where", where}};
}

int App::usesOf(const QString &file) const
{
	int n = 0;
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		if ((s.kind == "image" || s.kind == "media") && QDir::cleanPath(s.settings.value("file").toString()).compare(file, Qt::CaseInsensitive) == 0)
			++n;
	}
	return n;
}

QHash<QString, QStringList> App::scenesByFile() const
{
	QHash<QString, QStringList> out;
	for (const Instance &i : doc().instances) {
		const Source &s = doc().sources.value(i.sourceId);
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = QDir::cleanPath(s.settings.value("file").toString()).toLower();
		const QString sn = doc().scenes.value(i.sceneId).name;
		if (!out[f].contains(sn))
			out[f] << sn;
	}
	return out;
}

QVariantList App::assets() const
{
	const auto byFile = scenesByFile();
	QVariantList out;
	for (const QFileInfo &fi : QDir(currentAssetDir()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
		out << assetEntry(fi, byFile);
	for (const QFileInfo &fi : QDir(currentAssetDir()).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase))
		if (fi.fileName() != ".lobsfolder")
			out << assetEntry(fi, byFile);
	return out;
}

QString App::folderColor(const QString &folder)
{
	QFile f(folder + "/.lobsfolder");
	if (!f.open(QIODevice::ReadOnly))
		return {};
	return QJsonDocument::fromJson(f.readAll()).object()["color"].toString();
}

void App::setFolderColor(const QString &folder, const QString &color)
{
	if (!QFileInfo(folder).isDir() || !insideAssets(folder))
		return;
	const QString file = folder + "/.lobsfolder";
	platform::setHidden(file, false);
	if (color.isEmpty()) {
		QFile::remove(file);
	} else {
		QFile f(file);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
			f.write(QJsonDocument(QJsonObject{{"color", color}}).toJson(QJsonDocument::Compact));
		f.close();
		platform::setHidden(file, true);
	}
	emit assetsChanged();
}

QVariantList App::missingAssets() const
{
	if (m_missingValid && m_missingAge.elapsed() < 2000 && m_missingDoc == doc())
		return m_missingCache;
	QHash<QString, int> layers;
	for (const Instance &i : doc().instances)
		++layers[i.sourceId];
	QVariantList out;
	for (const Source &s : doc().sources) {
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = s.settings.value("file").toString();
		if (f.isEmpty() || !layers.value(s.id) || QFile::exists(f))
			continue;
		out << QVariantMap{{"sourceId", s.id}, {"name", QFileInfo(f).fileName()}, {"file", f}, {"layers", layers.value(s.id)}};
	}
	m_missingCache = out, m_missingDoc = doc(), m_missingValid = true;
	m_missingAge.start();
	return out;
}

void App::findMissingAssets()
{
	int fixed = 0, notFound = 0;
	for (const QVariant &v : missingAssets()) {
		const QVariantMap m = v.toMap();
		QString match;
		QDirIterator it(assetsFolder(), QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext() && match.isEmpty()) {
			const QString p = it.next();
			if (QFileInfo(p).fileName().compare(m["name"].toString(), Qt::CaseInsensitive) == 0)
				match = p;
		}
		if (match.isEmpty()) {
			++notFound;
			continue;
		}
		repointFiles(m["file"].toString(), match);
		++fixed;
	}
	if (fixed)
		showToast(QString("Found %1 missing file%2 in assets").arg(fixed).arg(fixed == 1 ? "" : "s") + (notFound ? QString("; %1 still missing").arg(notFound) : QString()));
	else if (notFound)
		showToast("Not found in assets. Use Choose file…");
	emit assetsChanged();
	emit sourceStatusChanged();
}

void App::relinkAsset(const QString &sourceId, const QString &file)
{
	if (!doc().sources.contains(sourceId) || !QFile::exists(file))
		return;
	const QString old = doc().sources[sourceId].settings.value("file").toString();
	const QString copy = insideAssets(file) ? QDir::cleanPath(file) : copyIntoAssets(file);
	if (copy.isEmpty())
		return showToast("Couldn't copy that file into assets");
	repointFiles(old, copy);
	showToast("Relinked " + QFileInfo(copy).fileName());
	emit assetsChanged();
	emit sourceStatusChanged();
}

QString App::copyIntoAssets(const QString &file, const QString &destDir)
{
	const QFileInfo src(file);
	const QString dir = destDir.isEmpty() ? currentAssetDir() : destDir;
	if (!src.isFile())
		return {};
	if (QDir::cleanPath(src.absolutePath()) == QDir::cleanPath(dir))
		return QDir::cleanPath(src.absoluteFilePath());
	QDir().mkpath(dir);
	QString dest = dir + "/" + src.fileName();
	for (int n = 2; QFile::exists(dest); ++n) {
		const QFileInfo d(dest);
		if (d.size() == src.size() && d.lastModified() == src.lastModified())
			return dest;
		dest = dir + "/" + src.completeBaseName() + QString(" (%1)").arg(n) + (src.suffix().isEmpty() ? "" : "." + src.suffix());
	}
	if (!QFile::copy(src.absoluteFilePath(), dest))
		return {};
	QFile(dest).setFileTime(src.lastModified(), QFileDevice::FileModificationTime);
	return dest;
}

void App::importAssets(const QVariantList &urls)
{
	int added = 0, skipped = 0;
	for (const QVariant &u : urls) {
		const QString file = u.toUrl().isLocalFile() ? u.toUrl().toLocalFile() : u.toString();
		if (QFileInfo(file).isDir()) {
			for (const QFileInfo &fi : QDir(file).entryInfoList(QDir::Files))
				copyIntoAssets(fi.absoluteFilePath()).isEmpty() ? ++skipped : ++added;
			continue;
		}
		copyIntoAssets(file).isEmpty() ? ++skipped : ++added;
	}
	emit assetsChanged();
	showToast(QString("Added %1 file%2 to assets").arg(added).arg(added == 1 ? "" : "s") + (skipped ? QString(" (%1 couldn't be copied)").arg(skipped) : QString()));
}

void App::adoptExternalFiles()
{
	QHash<QString, QString> moved;
	for (const Source &s : doc().sources) {
		if (s.kind != "image" && s.kind != "media")
			continue;
		const QString f = s.settings.value("file").toString();
		if (f.isEmpty() || !QFile::exists(f) || insideAssets(f))
			continue;
		if (const QString copy = copyIntoAssets(f, assetsFolder()); !copy.isEmpty())
			moved[s.id] = copy;
	}
	if (moved.isEmpty())
		return;
	m_store.amend([&](Doc &d) {
		for (auto it = moved.begin(); it != moved.end(); ++it)
			d.sources[it.key()].settings["file"] = it.value();
	});
	emit assetsChanged();
}

void App::revealAsset(const QString &path)
{
	blog(LOG_INFO, "[lobs] Show in File Explorer: %s (exists=%d)", path.toUtf8().constData(), int(QFile::exists(path)));
	if (!QFile::exists(path))
		return showToast(QFileInfo(path).fileName() + " is no longer in the assets folder");
	platform::reveal(path, true);
}

void App::openAssetsFolder()
{
	QDir().mkpath(assetsFolder());
	platform::reveal(assetsFolder(), false);
}

void App::trashAsset(const QString &path)
{
	if (QFile::moveToTrash(path))
		showToast("Moved " + QFileInfo(path).fileName() + " to the Recycle Bin");
	else
		showToast("Couldn't move " + QFileInfo(path).fileName() + " to the Recycle Bin");
	emit assetsChanged();
}

QString App::assetSource(Doc &d, const QString &path) const
{
	const QString clean = QDir::cleanPath(path);
	for (const Source &s : d.sources)
		if ((s.kind == "image" || s.kind == "media") && QDir::cleanPath(s.settings.value("file").toString()) == clean)
			return s.id;
	const QString k = assetKind(path);
	QVariantMap st{{"file", clean}};
	if (k != "image")
		st["loop"] = true;
	return d.addSource(k == "image" ? "image" : "media", QFileInfo(path).completeBaseName(), st).id;
}

Transform App::assetTransform(const QString &path, QPointF c) const
{
	QSizeF sz = assetKind(path) == "image" ? QSizeF(QImageReader(path).size()) : QSizeF(1280, 720);
	if (sz.isEmpty())
		sz = QSizeF(640, 360);
	const double W = editCanvas().width();
	const double H = editCanvas().height();
	const double k = std::min(1.0, std::min(W / sz.width(), H / sz.height()));
	Transform t;
	t.w = std::round(sz.width() * k), t.h = std::round(sz.height() * k);
	t.x = std::round(c.x() - t.w / 2), t.y = std::round(c.y() - t.h / 2);
	if (t.w >= W - 1 && t.h >= H - 1)
		t.x = 0, t.y = 0;
	return t;
}

void App::assetDragMove(const QString &path, double x, double y)
{
	const Transform t = assetTransform(path, toCanvas({x, y}));
	OverlayBox b;
	b.rect = QRectF(t.x, t.y, t.w, t.h);
	b.drop = true;
	m_dropBoxes = {b};
	updateOverlay();
}

void App::assetDrop(const QString &path, double x, double y)
{
	if (!liveSceneEditAllowed(m_editScene))
		return;
	m_dropBoxes.clear();
	if (!doc().scenes.contains(m_editScene) || assetKind(path) == "other")
		return updateOverlay();
	const QPointF at = x < 0 ? QPointF(editCanvas().width() / 2.0, editCanvas().height() / 2.0) : toCanvas({x, y});
	const Transform t = assetKind(path) == "audio" ? Transform{0, 0, 1, 1} : assetTransform(path, at);
	QString iid;
	m_store.commit("Place " + QFileInfo(path).fileName(), [&](Doc &d) {
		const QString sid = assetSource(d, path);
		iid = d.placeSource(m_editScene, sid).id;
		d.setTransform(iid, t);
	});
	m_selection = {iid};
	m_poolSelection.clear();
	updateOverlay();
	emit docChanged();
	emit assetsChanged();
}

void App::placeAsset(const QString &path)
{
	assetDrop(path, -1, -1);
}

QVariantList App::searchAssets(const QString &query) const
{
	const QString q = query.trimmed();
	QVariantList out;
	if (q.isEmpty())
		return out;
	const auto byFile = scenesByFile();
	QDirIterator it(assetsFolder(), QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
	while (it.hasNext() && out.size() < 500) {
		const QFileInfo fi(it.next());
		if (fi.fileName() != ".lobsfolder" && fi.fileName().contains(q, Qt::CaseInsensitive))
			out << assetEntry(fi, byFile);
	}
	return out;
}
