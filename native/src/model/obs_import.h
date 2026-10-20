// Converts an OBS Studio scene collection (%APPDATA%/obs-studio/basic/scenes/*.json) into a
// LOBS document. Plain data; no libobs. Layer sizes that depend on a source's natural size
// (OBS stores scale factors, not pixel sizes) come back as `pending` placements, which the
// caller resolves once the engine knows each source's size.
#pragma once

#include "doc.h"

namespace lobs {

struct ObsPlacement {
	QString instId;
	double posX = 0, posY = 0, scaleX = 1, scaleY = 1;
	int align = 5;              // OBS alignment bits: 1 left, 2 right, 4 top, 8 bottom (0 = center)
	int boundsType = 0;         // 0 none, 1 stretch, 2 scale inner, 3 scale outer, 4 to width, 5 to height, 6 max only
	int boundsAlign = 0;
	double boundsW = 0, boundsH = 0;
	int fallbackW = 320, fallbackH = 180; // stand-in natural size while the real one is unknown
};

struct ObsImport {
	Doc doc;
	QVector<ObsPlacement> pending; // every placed layer; refine with resolveObsPlacement()
	QStringList skipped;           // human-readable notes on what didn't come over
	QString programScene;          // LOBS scene id of OBS's current program scene
};

bool isObsCollection(const QJsonObject &o);
ObsImport importObsCollection(const QJsonObject &o);

// Size and position for a placement, given the source's natural size (0x0 if unknown) and crop.
Transform resolveObsPlacement(const ObsPlacement &p, int naturalW, int naturalH, const Crop &crop);

} // namespace lobs
