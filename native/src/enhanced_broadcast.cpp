#include "enhanced_broadcast.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSettings>
#include <QRegularExpression>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace enhanced_broadcast {

namespace {

#ifdef Q_OS_WIN
// Each GPU as OBS lists it: model, PCI ids, video memory, driver version.
QJsonArray gpus()
{
	QJsonArray out;
	IDXGIFactory1 *factory = nullptr;
	if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))))
		return out;
	IDXGIAdapter1 *a = nullptr;
	for (UINT i = 0; factory->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 d{};
		a->GetDesc1(&d);
		if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			QString driver;
			LARGE_INTEGER umd{};
			if (SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd)))
				driver = QString("%1.%2.%3.%4").arg(HIWORD(umd.HighPart)).arg(LOWORD(umd.HighPart)).arg(HIWORD(umd.LowPart)).arg(LOWORD(umd.LowPart));
			out.append(QJsonObject{{"model", QString::fromWCharArray(d.Description)},
					       {"vendor_id", int(d.VendorId)},
					       {"device_id", int(d.DeviceId)},
					       {"dedicated_video_memory", double(d.DedicatedVideoMemory)},
					       {"shared_system_memory", double(d.SharedSystemMemory)},
					       {"driver_version", driver}});
		}
		a->Release();
	}
	factory->Release();
	return out;
}

int physicalCores()
{
	DWORD len = 0;
	GetLogicalProcessorInformation(nullptr, &len);
	std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> info(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
	if (info.empty() || !GetLogicalProcessorInformation(info.data(), &len))
		return int(std::thread::hardware_concurrency());
	int n = 0;
	for (const auto &i : info)
		n += i.Relationship == RelationProcessorCore;
	return n;
}

QJsonObject cpu()
{
	const QSettings r(R"(HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0)", QSettings::NativeFormat);
	return {{"name", r.value("ProcessorNameString").toString().trimmed()},
		{"speed", r.value("~MHz").toInt()},
		{"logical_cores", int(std::thread::hardware_concurrency())},
		{"physical_cores", physicalCores()}};
}

QJsonObject memory()
{
	MEMORYSTATUSEX m{sizeof(m)};
	GlobalMemoryStatusEx(&m);
	return {{"total", double(m.ullTotalPhys)}, {"free", double(m.ullAvailPhys)}};
}

QJsonObject system()
{
	const QSettings r(R"(HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion)", QSettings::NativeFormat);
	SYSTEM_INFO si{};
	GetNativeSystemInfo(&si);
	const bool arm = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64;
	return {{"name", "Windows"},
		{"version", "10.0"},
		{"build", r.value("CurrentBuildNumber").toString().toInt()},
		{"release", r.value("DisplayVersion").toString()},
		{"revision", r.value("UBR").toString()},
		{"bits", 64},
		{"arm", arm},
		{"armEmulation", false}};
}
#else
QJsonArray gpus() { return {}; }
QJsonObject cpu() { return {{"logical_cores", int(std::thread::hardware_concurrency())}}; }
QJsonObject memory() { return {}; }
QJsonObject system() { return {{"name", "macOS"}}; }
#endif

} // namespace

QJsonObject request(const QString &streamKey, int width, int height, int fps, const QString &engineVersion)
{
	const QJsonObject canvas{{"width", width}, {"height", height}, {"canvas_width", width}, {"canvas_height", height},
				 {"framerate", QJsonObject{{"numerator", fps}, {"denominator", 1}}}};
	// The client is LOBS's encoding stack: OBS's own libobs, encoders and outputs (Inspector reports
	// it as "obs-output module"), so Twitch's plan for OBS of this version is what LOBS can run.
	return {
		{"schema_version", "2025-01-25"},
		{"service", "IVS"},
		{"authentication", streamKey},
		{"client", QJsonObject{{"name", "obs-studio"}, {"version", engineVersion}, {"supported_codecs", QJsonArray{"h264"}}}},
		{"capabilities", QJsonObject{{"cpu", cpu()}, {"memory", memory()}, {"gpu", gpus()}, {"system", system()},
					     {"gaming_features", QJsonObject{}}}},
		{"preferences", QJsonObject{{"canvases", QJsonArray{canvas}},
					    {"composition_gpu_index", 0},
					    {"audio_samples_per_sec", 48000},
					    {"audio_channels", 2},
					    {"audio_max_buffering_ms", 960},
					    {"audio_fixed_buffering", false},
					    {"maximum_aggregate_bitrate", QJsonValue::Null},
					    {"maximum_video_tracks", QJsonValue::Null},
					    {"vod_track_audio", false}}},
	};
}

QString check(const QJsonObject &plan)
{
	// Twitch can say no (an unsupported GPU or setup) with a reason for people to read.
	const QJsonObject status = plan.value("status").toObject();
	if (status.value("result").toString() == "error") {
		QString why = status.value("html_en_us").toString();
		why.remove(QRegularExpression("<[^>]*>"));
		return why.isEmpty() ? "Twitch turned down Enhanced Broadcasting for this setup" : why.trimmed();
	}
	if (plan.value("encoder_configurations").toArray().isEmpty())
		return "Twitch sent no qualities to encode";
	for (const QJsonValue &e : plan.value("ingest_endpoints").toArray())
		if (e.toObject().value("protocol").toString() == "RTMP" || e.toObject().value("protocol").toString() == "RTMPS")
			return {};
	return "Twitch sent no address to stream to";
}

} // namespace enhanced_broadcast
