#pragma once
#include "app.h"

namespace lobs {
struct Filter;
}

QString pluginTypeLabel(const QString &type);
QString filterLabel(const QString &type);
void registerPluginKinds();
QString firstRealDevice(Engine *engine, const QString &kind);
lobs::Doc withDefaultDevices(lobs::Doc d, Engine *engine);
bool writeShowFile(const QString &path, const lobs::Doc &d);
QString assetKind(const QString &file);
bool isAssetKind(const QString &kind);
void pruneUnusedAssetSources(lobs::Doc &d);
bool isBackgroundLayer(const lobs::Doc &d, const QString &id);
void placeBackground(lobs::Doc &d, const QString &sceneId);
lobs::Filter duckFilter(const QString &voiceId);
void cropKeepingScale(lobs::Doc &d, const QString &id, lobs::Crop nc, QSize ns);
