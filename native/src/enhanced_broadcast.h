#pragma once

#include <QJsonObject>
#include <QSize>
#include <QString>

// Twitch's Enhanced Broadcasting, as OBS does it (OBS's goliveapi and multitrack-video-output).
// Before going live LOBS sends Twitch this PC's hardware and canvas; Twitch answers with a plan:
// the qualities to encode (resolution, frame rate, encoder settings), the audio, and where to
// send it. Engine::startStreaming then makes one encoder per quality on one output.
namespace enhanced_broadcast {

// Twitch's endpoint, from rtmp-services' Twitch entry ("multitrack_video_configuration_url").
inline const char *kConfigUrl = "https://ingest.twitch.tv/api/v3/GetClientConfiguration";

// The request body (schema 2025-01-25): key, hardware (GPU, CPU, memory, Windows), canvas, audio.
// vertical: the vertical canvas's size, when it's on (Twitch's vertical stream); empty for none.
QJsonObject request(const QString &streamKey, int width, int height, int fps, const QString &engineVersion, QSize vertical = {});

// Twitch's answer, checked: "" when it's a plan LOBS can stream, else why not, in plain words.
QString check(const QJsonObject &plan);

} // namespace enhanced_broadcast
