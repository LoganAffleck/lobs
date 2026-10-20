#include "shellmeta.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMutex>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

namespace {

struct ComScope { // COM for this thread (image providers may run off the GUI thread)
	bool ok;
	ComScope() : ok(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {}
	~ComScope() { if (ok) CoUninitialize(); }
};

QString key(const QString &path)
{
	const QFileInfo fi(path);
	return QDir::cleanPath(fi.absoluteFilePath()).toLower() + "|" + QString::number(fi.lastModified().toMSecsSinceEpoch()) + "|" + QString::number(fi.size());
}

QMutex g_lock;
QHash<QString, ShellMeta> g_meta;

} // namespace

ShellMeta shellMeta(const QString &path)
{
	const QString k = key(path);
	{
		QMutexLocker l(&g_lock);
		if (auto it = g_meta.find(k); it != g_meta.end())
			return *it;
	}
	ShellMeta m;
	QImageReader reader(path);
	if (reader.canRead())
		m.size = reader.size();
	ComScope com;
	IPropertyStore *store = nullptr;
	const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
	if (SUCCEEDED(SHGetPropertyStoreFromParsingName(w.c_str(), nullptr, GPS_DEFAULT, IID_PPV_ARGS(&store)))) {
		PROPVARIANT v;
		PropVariantInit(&v);
		if (SUCCEEDED(store->GetValue(PKEY_Media_Duration, &v))) {
			ULONGLONG d = 0; // 100 ns units
			if (SUCCEEDED(PropVariantToUInt64(v, &d)))
				m.durationMs = qint64(d / 10000);
		}
		PropVariantClear(&v);
		if (m.size.isEmpty()) {
			UINT32 fw = 0, fh = 0;
			PROPVARIANT a, b;
			PropVariantInit(&a);
			PropVariantInit(&b);
			if (SUCCEEDED(store->GetValue(PKEY_Video_FrameWidth, &a)) && SUCCEEDED(store->GetValue(PKEY_Video_FrameHeight, &b)) &&
			    SUCCEEDED(PropVariantToUInt32(a, &fw)) && SUCCEEDED(PropVariantToUInt32(b, &fh)) && fw && fh)
				m.size = QSize(int(fw), int(fh));
			PropVariantClear(&a);
			PropVariantClear(&b);
		}
		store->Release();
	}
	QMutexLocker l(&g_lock);
	g_meta.insert(k, m);
	return m;
}

QImage shellThumbnail(const QString &path, int size)
{
	ComScope com;
	IShellItemImageFactory *factory = nullptr;
	const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
	if (FAILED(SHCreateItemFromParsingName(w.c_str(), nullptr, IID_PPV_ARGS(&factory))))
		return {};
	HBITMAP bmp = nullptr;
	QImage img;
	// Thumbnails only (no generic file-type icons): those look like missing previews.
	if (SUCCEEDED(factory->GetImage(SIZE{size, size}, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bmp)) && bmp) {
		img = QImage::fromHBITMAP(bmp);
		DeleteObject(bmp);
	}
	factory->Release();
	if (!img.isNull() && img.hasAlphaChannel()) {
		// Shell thumbnails of videos often come with a fully transparent alpha: make it opaque.
		bool allClear = true;
		for (int y = 0; y < img.height() && allClear; y += 7)
			for (int x = 0; x < img.width() && allClear; x += 7)
				allClear = qAlpha(img.pixel(x, y)) == 0;
		if (allClear)
			img = img.convertToFormat(QImage::Format_RGB32);
	}
	return img;
}
