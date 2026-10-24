#include "shellmeta.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QMutex>

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

namespace {

QString key(const QString &path)
{
	const QFileInfo fi(path);
	return QDir::cleanPath(fi.absoluteFilePath()) + "|" + QString::number(fi.lastModified().toMSecsSinceEpoch()) + "|" + QString::number(fi.size());
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
	else {
		@autoreleasepool {
			AVURLAsset *asset = [AVURLAsset URLAssetWithURL:[NSURL fileURLWithPath:path.toNSString()] options:nil];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations" // the async loaders need a run loop; this runs off the GUI thread
			const CMTime d = asset.duration;
			if (CMTIME_IS_NUMERIC(d) && d.value > 0)
				m.durationMs = qint64(CMTimeGetSeconds(d) * 1000.0);
			if (AVAssetTrack *video = [asset tracksWithMediaType:AVMediaTypeVideo].firstObject) {
				const CGSize s = CGSizeApplyAffineTransform(video.naturalSize, video.preferredTransform);
				m.size = QSize(int(std::abs(s.width)), int(std::abs(s.height)));
			}
#pragma clang diagnostic pop
		}
	}
	QMutexLocker l(&g_lock);
	g_meta.insert(k, m);
	return m;
}

QImage shellThumbnail(const QString &path, int size)
{
	QImage img;
	@autoreleasepool {
		QLThumbnailGenerationRequest *req = [[QLThumbnailGenerationRequest alloc]
			initWithFileAtURL:[NSURL fileURLWithPath:path.toNSString()]
				     size:CGSizeMake(size, size)
				    scale:1.0
		      representationTypes:QLThumbnailGenerationRequestRepresentationTypeThumbnail]; // no generic file icons
		dispatch_semaphore_t done = dispatch_semaphore_create(0);
		__block CGImageRef cg = nullptr;
		[[QLThumbnailGenerator sharedGenerator] generateBestRepresentationForRequest:req
			completionHandler:^(QLThumbnailRepresentation *rep, NSError *) {
				if (rep && rep.CGImage)
					cg = CGImageRetain(rep.CGImage);
				dispatch_semaphore_signal(done);
			}];
		dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
		if (cg) {
			NSBitmapImageRep *bmp = [[NSBitmapImageRep alloc] initWithCGImage:cg];
			NSData *png = [bmp representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
			img.loadFromData(static_cast<const uchar *>(png.bytes), int(png.length), "PNG");
			CGImageRelease(cg);
		}
	}
	return img;
}
