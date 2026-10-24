// File thumbnails and media facts from the Windows shell (what File Explorer shows) or, on macOS,
// QuickLook and AVFoundation (what Finder shows): a
// thumbnail for images and videos, and duration / frame size for video and audio.
// Results are cached per path + modified time.
#pragma once

#include <QImage>
#include <QSize>
#include <QString>

struct ShellMeta {
	qint64 durationMs = 0; // 0 = not a timed file (or unknown)
	QSize size;            // pixel size for images and videos, if known
};

ShellMeta shellMeta(const QString &path);
QImage shellThumbnail(const QString &path, int size); // empty if the shell has none
