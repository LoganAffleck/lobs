#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Switching from OBS in one step (first-time setup's "Coming from OBS?"): finds OBS's settings,
// and reads a profile (canvas, encoder, bitrate, stream service and key, recording) into LOBS
// settings. The scene collection goes through the existing importer; plugins through
// plugins::install.
namespace obs_switch {

// OBS's settings folder (%APPDATA%\obs-studio), or "" when OBS isn't set up on this PC.
QString obsFolder();

// What's there: {found, folder, profiles: [{name, dir}], collections: [{name, path}],
// lastProfile, lastCollection (the ones OBS used last), plugins: [{name, path}]}.
QVariantMap scan(const QString &folder = obsFolder());

// A profile as LOBS settings changes (AppSettings::applyMap keys), plus "_notes": what didn't
// come over and why.
QVariantMap readProfile(const QString &profileDir);

} // namespace obs_switch
