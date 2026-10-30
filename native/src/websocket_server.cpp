#include "websocket_server.h"
#include "app.h"
#include "platform.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>
#include <cmath>
#include <obs.h>

using namespace lobs;

namespace {

// obs-websocket's opcodes, event subscription bits and request status codes.
enum Op { Hello = 0, Identify = 1, Identified = 2, Reidentify = 3, Event = 5, Request = 6, RequestResponse = 7, RequestBatch = 8, RequestBatchResponse = 9 };
enum Sub : quint32 { General = 1 << 0, Config = 1 << 1, Scenes = 1 << 2, Inputs = 1 << 3, Transitions = 1 << 4, Filters = 1 << 5,
		     Outputs = 1 << 6, SceneItems = 1 << 7, MediaInputs = 1 << 8, Vendors = 1 << 9, Ui = 1 << 10, All = 0x7ff,
		     InputVolumeMeters = 1 << 16 }; // high volume: only when asked for
enum Status { Success = 100, MissingRequestType = 203, UnknownRequestType = 204, MissingRequestField = 300, InvalidRequestField = 400,
	      OutputRunning = 500, OutputNotRunning = 501, ResourceNotFound = 600, RequestProcessingFailed = 702 };

QString b64sha256(const QByteArray &in) { return QCryptographicHash::hash(in, QCryptographicHash::Sha256).toBase64(); }

QString randomText(int n)
{
	static const char abc[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
	QString s;
	for (int i = 0; i < n; i++)
		s += QChar(abc[QRandomGenerator::system()->bounded(int(sizeof(abc) - 1))]);
	return s;
}

// OBS's output state names, for StreamStateChanged / RecordStateChanged and friends.
QJsonObject outputState(bool active) { return {{"outputActive", active}, {"outputState", active ? "OBS_WEBSOCKET_OUTPUT_STARTED" : "OBS_WEBSOCKET_OUTPUT_STOPPED"}}; }

double mulToDb(double m) { return m <= 0 ? -100.0 : 20.0 * std::log10(m); }

} // namespace

ObsWebSocket::ObsWebSocket(App *app, QObject *parent) : QObject(parent), m_app(app)
{
	QSettings s;
	m_enabled = s.value("websocket/enabled", false).toBool();
	m_port = s.value("websocket/port", 4455).toInt();
	m_authRequired = s.value("websocket/auth", true).toBool();
	m_password = platform::unprotectSecret(s.value("websocket/password").toString());
	if (m_password.isEmpty()) {
		m_password = randomText(16);
		save();
	}
	connect(app, &App::docChanged, this, &ObsWebSocket::watch);
	connect(app, &App::engineChanged, this, &ObsWebSocket::watch);
	// Audio meters, 20 times a second as OBS's: remote mixers list the inputs that have levels.
	connect(app, &App::levelsChanged, this, [this] {
		bool wanted = false;
		for (QTcpSocket *s : m_identified)
			wanted |= bool(m_clients.value(s).events & InputVolumeMeters);
		if (!wanted)
			return;
		const Doc &doc = m_app->doc();
		const QVariantMap levels = m_app->levels();
		QJsonArray inputs;
		for (auto it = levels.cbegin(); it != levels.cend(); ++it) {
			if (!doc.sources.contains(it.key()))
				continue;
			QJsonArray channels;
			for (const QVariant &db : it.value().toList()) {
				const double mul = db.toDouble() <= -60 ? 0 : std::pow(10.0, db.toDouble() / 20.0);
				channels.append(QJsonArray{mul, mul, mul}); // magnitude, peak, input peak (LOBS meters peak)
			}
			inputs.append(QJsonObject{{"inputName", doc.sources[it.key()].name}, {"inputUuid", it.key()}, {"inputLevelsMul", channels}});
		}
		emitEvent("InputVolumeMeters", InputVolumeMeters, {{"inputs", inputs}});
	});
	restart();
}

// This PC on the local network (a private IPv4 address, as a phone or another PC reaches it).
QString ObsWebSocket::address() const
{
	QString ip = "localhost";
	for (const QNetworkInterface &nic : QNetworkInterface::allInterfaces()) {
		if (!(nic.flags() & QNetworkInterface::IsUp) || (nic.flags() & QNetworkInterface::IsLoopBack) || nic.type() == QNetworkInterface::Virtual)
			continue;
		for (const QNetworkAddressEntry &e : nic.addressEntries())
			if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && e.ip().isPrivateUse())
				return QString("ws://%1:%2").arg(e.ip().toString()).arg(m_port);
	}
	return QString("ws://%1:%2").arg(ip).arg(m_port);
}

void ObsWebSocket::save()
{
	QSettings s;
	s.setValue("websocket/enabled", m_enabled);
	s.setValue("websocket/port", m_port);
	s.setValue("websocket/auth", m_authRequired);
	s.setValue("websocket/password", platform::protectSecret(m_password));
}

void ObsWebSocket::setEnabled(bool on) { m_enabled = on; save(); restart(); }
void ObsWebSocket::setPort(int port) { m_port = std::clamp(port, 1024, 65535); save(); restart(); }
void ObsWebSocket::setAuthRequired(bool on) { m_authRequired = on; save(); emit changed(); }
void ObsWebSocket::newPassword() { m_password = randomText(16); save(); emit changed(); }

void ObsWebSocket::restart()
{
	for (QTcpSocket *s : m_clients.keys())
		s->disconnectFromHost();
	m_clients.clear();
	m_identified.clear();
	if (m_server)
		m_server->close(), m_server->deleteLater(), m_server = nullptr;
	m_error.clear();
	if (m_enabled) {
		m_server = new QTcpServer(this);
		// Every interface, as OBS: Stream Deck on a phone or another PC connects over the network.
		if (!m_server->listen(QHostAddress::Any, quint16(m_port))) {
			m_error = QString("Port %1 is in use. Is OBS running?").arg(m_port);
			m_server->deleteLater(), m_server = nullptr;
		} else {
			connect(m_server, &QTcpServer::newConnection, this, [this] {
				while (QTcpSocket *s = m_server ? m_server->nextPendingConnection() : nullptr) {
					m_clients.insert(s, Client{});
					connect(s, &QTcpSocket::readyRead, this, [this, s] { onReadyRead(s); });
					connect(s, &QTcpSocket::disconnected, this, [this, s] {
						m_clients.remove(s);
						m_identified.remove(s);
						s->deleteLater();
						emit changed();
					});
				}
			});
			blog(LOG_INFO, "[lobs] obs-websocket server on port %d", m_port);
		}
	}
	emit changed();
}

// ---------- WebSocket framing (RFC 6455) ----------

void ObsWebSocket::onReadyRead(QTcpSocket *s)
{
	Client &c = m_clients[s];
	c.buf += s->readAll();
	if (!c.upgraded)
		return handshake(s, c);
	while (c.buf.size() >= 2) {
		const uchar *b = reinterpret_cast<const uchar *>(c.buf.constData());
		const bool fin = b[0] & 0x80;
		const int op = b[0] & 0x0f;
		const bool masked = b[1] & 0x80;
		quint64 len = b[1] & 0x7f;
		int at = 2;
		if (len == 126) {
			if (c.buf.size() < 4) return;
			len = qFromBigEndian<quint16>(b + 2), at = 4;
		} else if (len == 127) {
			if (c.buf.size() < 10) return;
			len = qFromBigEndian<quint64>(b + 2), at = 10;
		}
		if (len > 16 * 1024 * 1024)
			return s->abort(); // nothing a remote control sends is this big
		if (quint64(c.buf.size()) < at + (masked ? 4 : 0) + len)
			return;
		QByteArray payload = c.buf.mid(at + (masked ? 4 : 0), qsizetype(len));
		if (masked)
			for (qsizetype i = 0; i < payload.size(); i++)
				payload[i] = char(payload[i] ^ b[at + (i % 4)]);
		c.buf.remove(0, at + (masked ? 4 : 0) + qsizetype(len));
		if (op == 0x8) // close
			return sendFrame(s, 0x8, payload.left(2)), s->disconnectFromHost();
		if (op == 0x9) { // ping
			sendFrame(s, 0xA, payload);
			continue;
		}
		if (op == 0x1 || op == 0x2 || op == 0x0) {
			c.message += payload;
			if (fin) {
				const QByteArray msg = c.message;
				c.message.clear();
				onMessage(s, c, msg);
				if (!m_clients.contains(s))
					return;
			}
		}
	}
}

void ObsWebSocket::handshake(QTcpSocket *s, Client &c)
{
	const int end = c.buf.indexOf("\r\n\r\n");
	if (end < 0)
		return;
	QByteArray key, protocols;
	for (const QByteArray &line : c.buf.left(end).split('\n')) {
		if (line.toLower().startsWith("sec-websocket-key:"))
			key = line.mid(18).trimmed();
		else if (line.toLower().startsWith("sec-websocket-protocol:"))
			protocols = line.mid(23).trimmed();
	}
	c.buf.remove(0, end + 4);
	if (key.isEmpty()) {
		s->write("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
		return s->disconnectFromHost();
	}
	const QByteArray accept = QCryptographicHash::hash(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1).toBase64();
	// The subprotocol only if the client offered it: browsers drop a connection whose answer names
	// one they didn't ask for. (MessagePack, obs-websocket's other encoding, isn't spoken here.)
	const bool json = protocols.contains("obswebsocket.json");
	blog(LOG_INFO, "[lobs] obs-websocket client from %s, protocols '%s'", qPrintable(s->peerAddress().toString()), protocols.constData());
	if (!protocols.isEmpty() && !json) {
		s->write("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
		return s->disconnectFromHost();
	}
	s->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept +
		 (json ? "\r\nSec-WebSocket-Protocol: obswebsocket.json" : "") + "\r\n\r\n");
	c.upgraded = true;
	// Hello, with a challenge when a password is set.
	QJsonObject d{{"obsWebSocketVersion", "5.6.2"}, {"rpcVersion", 1}};
	if (m_authRequired) {
		c.challenge = randomText(32), c.salt = randomText(32);
		d["authentication"] = QJsonObject{{"challenge", c.challenge}, {"salt", c.salt}};
	}
	send(s, {{"op", Hello}, {"d", d}});
}

void ObsWebSocket::sendFrame(QTcpSocket *s, int opcode, const QByteArray &payload)
{
	QByteArray h;
	h += char(0x80 | opcode);
	if (payload.size() < 126)
		h += char(payload.size());
	else if (payload.size() < 65536) {
		h += char(126);
		uchar n[2];
		qToBigEndian<quint16>(quint16(payload.size()), n);
		h.append(reinterpret_cast<char *>(n), 2);
	} else {
		h += char(127);
		uchar n[8];
		qToBigEndian<quint64>(quint64(payload.size()), n);
		h.append(reinterpret_cast<char *>(n), 8);
	}
	s->write(h + payload);
}

void ObsWebSocket::send(QTcpSocket *s, const QJsonObject &msg) { sendFrame(s, 0x1, QJsonDocument(msg).toJson(QJsonDocument::Compact)); }

// ---------- protocol ----------

void ObsWebSocket::onMessage(QTcpSocket *s, Client &c, const QByteArray &text)
{
	const QJsonObject msg = QJsonDocument::fromJson(text).object();
	const int op = msg.value("op").toInt(-1);
	const QJsonObject d = msg.value("d").toObject();
	if (op == Identify || op == Reidentify) {
		if (op == Identify && m_authRequired) {
			const QString secret = b64sha256((m_password + c.salt).toUtf8());
			if (d.value("authentication").toString() != b64sha256((secret + c.challenge).toUtf8())) {
				sendFrame(s, 0x8, QByteArray("\x0f\xa8", 2) + "Authentication failed."); // 4008
				return s->disconnectFromHost();
			}
		}
		c.identified = true;
		c.events = quint32(d.value("eventSubscriptions").toInt(All & ~quint32(Vendors)));
		m_identified.insert(s);
		send(s, {{"op", Identified}, {"d", QJsonObject{{"negotiatedRpcVersion", 1}}}});
		emit changed();
		return;
	}
	if (!c.identified) {
		sendFrame(s, 0x8, QByteArray("\x0f\xa3", 2) + "Not identified."); // 4003
		return s->disconnectFromHost();
	}
	auto one = [this](const QJsonObject &r) {
		int code = Success;
		QString comment;
		const QString type = r.value("requestType").toString();
		const QJsonObject out = type.isEmpty() ? (code = MissingRequestType, QJsonObject()) : request(type, r.value("requestData").toObject(), code, comment);
		QJsonObject status{{"result", code == Success}, {"code", code}};
		if (!comment.isEmpty())
			status["comment"] = comment;
		QJsonObject res{{"requestType", type}, {"requestId", r.value("requestId")}, {"requestStatus", status}};
		if (!out.isEmpty())
			res["responseData"] = out;
		return res;
	};
	if (op == Request)
		send(s, {{"op", RequestResponse}, {"d", one(d)}});
	else if (op == RequestBatch) {
		QJsonArray results;
		const bool haltOnFailure = d.value("haltOnFailure").toBool();
		for (const QJsonValue &r : d.value("requests").toArray()) {
			const QJsonObject res = one(r.toObject());
			results.append(res);
			if (haltOnFailure && !res["requestStatus"].toObject()["result"].toBool())
				break;
		}
		send(s, {{"op", RequestBatchResponse}, {"d", QJsonObject{{"requestId", d.value("requestId")}, {"results", results}}}});
	}
}

void ObsWebSocket::emitEvent(const QString &type, quint32 intent, const QJsonObject &data)
{
	const QJsonObject msg{{"op", Event}, {"d", QJsonObject{{"eventType", type}, {"eventIntent", int(intent)}, {"eventData", data}}}};
	for (QTcpSocket *s : m_identified)
		if (m_clients.value(s).events & intent)
			send(s, msg);
}

// ---------- lookups ----------

QString ObsWebSocket::sceneId(const QJsonObject &d) const
{
	const Doc &doc = m_app->doc();
	if (const QString u = d.value("sceneUuid").toString(); doc.scenes.contains(u))
		return u;
	const QString n = d.value("sceneName").toString();
	for (const QString &id : doc.sceneOrder)
		if (doc.scenes[id].name == n)
			return id;
	return {};
}

QString ObsWebSocket::sourceId(const QJsonObject &d) const
{
	const Doc &doc = m_app->doc();
	if (const QString u = d.value("inputUuid").toString(); doc.sources.contains(u))
		return u;
	const QString n = d.value("inputName").toString(d.value("sourceName").toString());
	for (const Source &s : doc.sources)
		if (s.name == n)
			return s.id;
	return {};
}

int ObsWebSocket::itemId(const QString &instanceId)
{
	auto it = m_itemIds.find(instanceId);
	if (it == m_itemIds.end())
		it = m_itemIds.insert(instanceId, m_nextItemId++);
	return it.value();
}

QString ObsWebSocket::instanceForItem(const QString &scene, int item) const
{
	for (auto it = m_itemIds.cbegin(); it != m_itemIds.cend(); ++it)
		if (it.value() == item && m_app->doc().instances.value(it.key()).sceneId == scene)
			return it.key();
	return {};
}

// ---------- requests ----------

QJsonObject ObsWebSocket::request(const QString &type, const QJsonObject &d, int &code, QString &comment)
{
	const Doc &doc = m_app->doc();
	auto fail = [&](int c, const QString &why) { code = c, comment = why; return QJsonObject(); };
	auto sceneJson = [&](const QString &id, int index) {
		return QJsonObject{{"sceneName", doc.scenes[id].name}, {"sceneUuid", id}, {"sceneIndex", index}};
	};
	const QString program = m_app->programSceneId();

	if (type == "GetVersion") {
		QJsonArray reqs;
		for (const char *r : {"GetVersion", "GetStats", "GetSceneList", "GetCurrentProgramScene", "SetCurrentProgramScene", "GetCurrentPreviewScene",
				      "SetCurrentPreviewScene", "GetInputList", "GetInputMute", "SetInputMute", "ToggleInputMute", "GetInputVolume",
				      "SetInputVolume", "GetSceneItemList", "GetSceneItemId", "GetSceneItemEnabled", "SetSceneItemEnabled", "GetStreamStatus",
				      "StartStream", "StopStream", "ToggleStream", "GetRecordStatus", "StartRecord", "StopRecord", "ToggleRecord",
				      "GetReplayBufferStatus", "StartReplayBuffer", "StopReplayBuffer", "ToggleReplayBuffer", "SaveReplayBuffer",
				      "GetVirtualCamStatus", "StartVirtualCam", "StopVirtualCam", "ToggleVirtualCam", "GetStudioModeEnabled",
				      "GetSourceScreenshot", "GetSpecialInputs", "GetInputAudioMonitorType", "SetInputAudioMonitorType"})
			reqs.append(r);
		return {{"obsVersion", obs_get_version_string()}, {"obsWebSocketVersion", "5.6.2"}, {"rpcVersion", 1}, {"availableRequests", reqs},
			{"supportedImageFormats", QJsonArray()}, {"platform", "windows"}, {"platformDescription", "LOBS"}};
	}
	if (type == "GetStats") {
		const QVariantMap st = m_app->liveStats();
		return {{"cpuUsage", st.value("cpu").toDouble()}, {"memoryUsage", st.value("memoryMb").toDouble()},
			{"activeFps", st.value("fps").toDouble()}, {"averageFrameRenderTime", st.value("renderMs").toDouble()},
			{"renderSkippedFrames", st.value("lagged").toInt()}, {"renderTotalFrames", st.value("rendered").toInt()},
			{"outputSkippedFrames", st.value("skipped").toInt()}, {"outputTotalFrames", st.value("encoded").toInt()},
			{"availableDiskSpace", st.value("diskFreeGb").toDouble() * 1024}, {"webSocketSessionIncomingMessages", 0}, {"webSocketSessionOutgoingMessages", 0}};
	}

	// Scenes. OBS lists them bottom-up (index 0 last), so do clients' lists.
	if (type == "GetSceneList") {
		QJsonArray scenes;
		const int n = int(doc.sceneOrder.size());
		for (int i = n - 1; i >= 0; i--)
			scenes.append(sceneJson(doc.sceneOrder[i], n - 1 - i));
		const QString edit = m_app->editSceneId();
		return {{"currentProgramSceneName", doc.scenes.value(program).name}, {"currentProgramSceneUuid", program},
			{"currentPreviewSceneName", doc.scenes.value(edit).name}, {"currentPreviewSceneUuid", edit}, {"scenes", scenes}};
	}
	if (type == "GetCurrentProgramScene")
		return {{"sceneName", doc.scenes.value(program).name}, {"sceneUuid", program}, {"currentProgramSceneName", doc.scenes.value(program).name}, {"currentProgramSceneUuid", program}};
	if (type == "GetCurrentPreviewScene") {
		const QString edit = m_app->editSceneId();
		return {{"sceneName", doc.scenes.value(edit).name}, {"sceneUuid", edit}, {"currentPreviewSceneName", doc.scenes.value(edit).name}, {"currentPreviewSceneUuid", edit}};
	}
	if (type == "SetCurrentProgramScene" || type == "SetCurrentPreviewScene") {
		const QString id = sceneId(d);
		if (id.isEmpty())
			return fail(ResourceNotFound, "No scene by that name");
		m_app->editScene(id);
		if (type == "SetCurrentProgramScene")
			m_app->take(); // with the current transition, as OBS
		return {};
	}
	if (type == "GetStudioModeEnabled")
		return {{"studioModeEnabled", false}};

	// A picture of a scene or source, as remote controls show the program (PNG or JPEG, base64).
	if (type == "GetSourceScreenshot") {
		QString scene = sceneId(QJsonObject{{"sceneName", d.value("sourceName")}, {"sceneUuid", d.value("sourceUuid")}});
		const QString src = scene.isEmpty() ? sourceId(QJsonObject{{"inputName", d.value("sourceName")}, {"inputUuid", d.value("sourceUuid")}}) : QString();
		if (scene.isEmpty() && src.isEmpty())
			return fail(ResourceNotFound, "No scene or source by that name");
		const int w = std::clamp(d.value("imageWidth").toInt(640), 8, 4096);
		QImage img = !scene.isEmpty() ? m_app->engine()->renderThumbnail(scene, w) : m_app->engine()->renderSourceThumbnail(src, w);
		if (img.isNull())
			return fail(RequestProcessingFailed, "Couldn't draw it");
		if (const int h = d.value("imageHeight").toInt(); h > 0 && h != img.height())
			img = img.scaled(img.width(), h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		const QString fmt = d.value("imageFormat").toString("png").toLower();
		const int quality = d.value("imageCompressionQuality").toInt(-1);
		QByteArray bytes;
		QBuffer buf(&bytes);
		buf.open(QIODevice::WriteOnly);
		const bool jpg = fmt == "jpg" || fmt == "jpeg";
		img.save(&buf, jpg ? "JPG" : fmt == "webp" ? "WEBP" : "PNG", quality);
		return {{"imageData", QString("data:image/%1;base64,").arg(jpg ? "jpeg" : fmt == "webp" ? "webp" : "png") + QString::fromLatin1(bytes.toBase64())}};
	}

	// The global audio devices, as OBS's Desktop Audio and Mic/Aux slots.
	if (type == "GetSpecialInputs") {
		QStringList desk, mics;
		for (const Source &s : doc.sources) {
			if (s.kind == "desktop_audio")
				desk << s.name;
			else if (s.kind == "mic")
				mics << s.name;
		}
		auto slot = [](const QStringList &l, int i) { return i < l.size() ? QJsonValue(l[i]) : QJsonValue(QJsonValue::Null); };
		return {{"desktop1", slot(desk, 0)}, {"desktop2", slot(desk, 1)}, {"mic1", slot(mics, 0)}, {"mic2", slot(mics, 1)}, {"mic3", slot(mics, 2)}, {"mic4", slot(mics, 3)}};
	}

	// Inputs: LOBS's sources.
	if (type == "GetInputList") {
		QJsonArray inputs;
		const QString kind = d.value("inputKind").toString();
		for (const Source &s : doc.sources) {
			if (s.kind == "scene")
				continue;
			const QString k = Engine::obsIdFor(s.kind);
			if (!kind.isEmpty() && kind != k)
				continue;
			inputs.append(QJsonObject{{"inputName", s.name}, {"inputUuid", s.id}, {"inputKind", k}, {"unversionedInputKind", k}});
		}
		return {{"inputs", inputs}};
	}
	if (type.startsWith("GetInput") || type.startsWith("SetInput") || type == "ToggleInputMute") {
		const QString id = sourceId(d);
		if (id.isEmpty())
			return fail(ResourceNotFound, "No input by that name");
		const Source &s = doc.sources[id];
		if (type == "GetInputMute")
			return {{"inputMuted", s.audio.muted}};
		if (type == "SetInputMute") {
			if (!d.contains("inputMuted"))
				return fail(MissingRequestField, "inputMuted");
			m_app->setAudio(id, "muted", d.value("inputMuted").toBool(), true);
			return {};
		}
		if (type == "ToggleInputMute") {
			m_app->setAudio(id, "muted", !s.audio.muted, true);
			return {{"inputMuted", !s.audio.muted}};
		}
		// Monitoring, in OBS's names for LOBS's off / monitor only / monitor and output.
		if (type == "GetInputAudioMonitorType")
			return {{"monitorType", s.audio.monitor == "monitor_only" ? "OBS_MONITORING_TYPE_MONITOR_ONLY"
					      : s.audio.monitor == "monitor_and_output" ? "OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT" : "OBS_MONITORING_TYPE_NONE"}};
		if (type == "SetInputAudioMonitorType") {
			const QString t = d.value("monitorType").toString();
			if (t.isEmpty())
				return fail(MissingRequestField, "monitorType");
			m_app->setAudio(id, "monitor", t == "OBS_MONITORING_TYPE_MONITOR_ONLY" ? "monitor_only" : t == "OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT" ? "monitor_and_output" : "off", true);
			return {};
		}
		if (type == "GetInputVolume")
			return {{"inputVolumeMul", s.audio.volume}, {"inputVolumeDb", mulToDb(s.audio.volume)}};
		if (type == "SetInputVolume") {
			double mul = s.audio.volume;
			if (d.contains("inputVolumeMul"))
				mul = d.value("inputVolumeMul").toDouble();
			else if (d.contains("inputVolumeDb"))
				mul = d.value("inputVolumeDb").toDouble() <= -100 ? 0 : std::pow(10.0, d.value("inputVolumeDb").toDouble() / 20.0);
			else
				return fail(MissingRequestField, "inputVolumeMul or inputVolumeDb");
			m_app->setAudio(id, "volume", std::clamp(mul, 0.0, 20.0), true);
			return {};
		}
	}

	// Scene items: a scene's top-level layers, top first as OBS's list is bottom-first reversed.
	if (type.startsWith("GetSceneItem") || type == "SetSceneItemEnabled") {
		const QString scene = sceneId(d);
		if (scene.isEmpty())
			return fail(ResourceNotFound, "No scene by that name");
		const QStringList layers = doc.scenes[scene].layers;
		if (type == "GetSceneItemList") {
			QJsonArray items;
			const int n = int(layers.size());
			for (int i = n - 1; i >= 0; i--) { // OBS's index 0 is the bottom
				const Instance &in = doc.instances[layers[i]];
				const Source &src = doc.sources.value(in.sourceId);
				items.append(QJsonObject{{"sceneItemId", itemId(in.id)}, {"sceneItemIndex", n - 1 - i}, {"sceneItemEnabled", doc.visibility(in).visible},
							 {"sceneItemLocked", in.locked}, {"sourceName", in.name.isEmpty() ? src.name : in.name}, {"sourceUuid", in.sourceId},
							 {"sourceType", "OBS_SOURCE_TYPE_INPUT"}, {"inputKind", Engine::obsIdFor(src.kind)}, {"isGroup", in.kind == "group"}});
			}
			return {{"sceneItems", items}};
		}
		if (type == "GetSceneItemId") {
			const QString name = d.value("sourceName").toString();
			for (const QString &l : layers) {
				const Instance &in = doc.instances[l];
				if (in.name == name || doc.sources.value(in.sourceId).name == name)
					return {{"sceneItemId", itemId(l)}};
			}
			return fail(ResourceNotFound, "No layer by that name in the scene");
		}
		const QString inst = instanceForItem(scene, d.value("sceneItemId").toInt(-1));
		if (inst.isEmpty())
			return fail(ResourceNotFound, "No scene item with that id (ask GetSceneItemList or GetSceneItemId first)");
		const bool visible = doc.visibility(doc.instances[inst]).visible;
		if (type == "GetSceneItemEnabled")
			return {{"sceneItemEnabled", visible}};
		if (!d.contains("sceneItemEnabled"))
			return fail(MissingRequestField, "sceneItemEnabled");
		if (d.value("sceneItemEnabled").toBool() != visible)
			m_app->toggleVisible(inst); // a live control: allowed on air
		return {};
	}

	// Outputs.
	auto output = [&](const QString &name, bool active, auto toggle) -> QJsonObject {
		if (type == "Get" + name + "Status")
			return {};
		if (type == "Start" + name) {
			if (active) return fail(OutputRunning, "Already running");
			toggle();
		} else if (type == "Stop" + name) {
			if (!active) return fail(OutputNotRunning, "Not running");
			toggle();
		} else if (type == "Toggle" + name) {
			toggle();
			return {{"outputActive", !active}};
		}
		return {};
	};
	if (type.endsWith("Stream") || type == "GetStreamStatus") {
		const QVariantMap ss = m_app->streamStats();
		if (type == "GetStreamStatus")
			return {{"outputActive", m_app->streaming()}, {"outputReconnecting", ss.value("reconnecting").toBool()}, {"outputTimecode", ss.value("time").toString() + ".000"},
				{"outputDuration", 0}, {"outputCongestion", ss.value("congestion").toDouble()}, {"outputBytes", ss.value("bytes").toDouble()},
				{"outputSkippedFrames", ss.value("dropped").toInt()}, {"outputTotalFrames", ss.value("total").toInt()}};
		return output("Stream", m_app->streaming(), [this] { m_app->toggleStreaming(); });
	}
	if (type.endsWith("Record") || type == "GetRecordStatus") {
		if (type == "GetRecordStatus")
			return {{"outputActive", m_app->recording()}, {"outputPaused", false}, {"outputTimecode", m_app->liveStats().value("recTime").toString() + ".000"}};
		return output("Record", m_app->recording(), [this] { m_app->toggleRecording(); });
	}
	if (type.endsWith("ReplayBuffer") || type == "GetReplayBufferStatus") {
		if (type == "GetReplayBufferStatus")
			return {{"outputActive", m_app->replayActive()}};
		if (type == "SaveReplayBuffer") {
			if (!m_app->replayActive()) return fail(OutputNotRunning, "The replay buffer is off");
			m_app->saveReplay();
			return {};
		}
		return output("ReplayBuffer", m_app->replayActive(), [this] { m_app->toggleReplay(); });
	}
	if (type.endsWith("VirtualCam") || type == "GetVirtualCamStatus") {
		if (type == "GetVirtualCamStatus")
			return {{"outputActive", m_app->vcamActive()}};
		return output("VirtualCam", m_app->vcamActive(), [this] { m_app->toggleVirtualCam(); });
	}
	blog(LOG_INFO, "[lobs] obs-websocket: %s isn't supported yet", qPrintable(type)); // what clients ask for that's missing
	return fail(UnknownRequestType, "LOBS doesn't support " + type + " yet");
}

// ---------- events ----------

// LOBS has no per-change notifications for these, so each change is found by comparing with what
// clients were last told (cheap: names, ids and a few flags).
void ObsWebSocket::watch()
{
	if (m_identified.isEmpty()) {
		m_last.clear();
		return;
	}
	const Doc &doc = m_app->doc();
	QVariantMap now;
	now["program"] = m_app->programSceneId();
	now["preview"] = m_app->editSceneId();
	QStringList scenes;
	for (const QString &id : doc.sceneOrder)
		scenes << id + "\x1f" + doc.scenes[id].name;
	now["scenes"] = scenes;
	QVariantMap mutes, volumes, items;
	for (const Source &s : doc.sources)
		mutes[s.id] = s.audio.muted, volumes[s.id] = s.audio.volume;
	for (const Instance &in : doc.instances)
		items[in.id] = doc.visibility(in).visible;
	now["mutes"] = mutes, now["volumes"] = volumes, now["items"] = items;
	now["stream"] = m_app->streaming(), now["record"] = m_app->recording(), now["replay"] = m_app->replayActive(), now["vcam"] = m_app->vcamActive();
	if (m_last.isEmpty()) {
		m_last = now;
		return;
	}
	auto changed = [&](const char *k) { return now.value(k) != m_last.value(k); };
	if (changed("scenes")) {
		QJsonArray list;
		const int n = int(doc.sceneOrder.size());
		for (int i = n - 1; i >= 0; i--)
			list.append(QJsonObject{{"sceneName", doc.scenes[doc.sceneOrder[i]].name}, {"sceneUuid", doc.sceneOrder[i]}, {"sceneIndex", n - 1 - i}});
		emitEvent("SceneListChanged", Scenes, {{"scenes", list}});
	}
	if (changed("program")) {
		const QString p = now["program"].toString();
		emitEvent("CurrentProgramSceneChanged", Scenes, {{"sceneName", doc.scenes.value(p).name}, {"sceneUuid", p}});
	}
	if (changed("preview")) {
		const QString p = now["preview"].toString();
		emitEvent("CurrentPreviewSceneChanged", Scenes, {{"sceneName", doc.scenes.value(p).name}, {"sceneUuid", p}});
	}
	const QVariantMap lastMutes = m_last["mutes"].toMap(), lastVolumes = m_last["volumes"].toMap(), lastItems = m_last["items"].toMap();
	for (auto it = mutes.cbegin(); it != mutes.cend(); ++it) {
		if (lastMutes.contains(it.key()) && lastMutes[it.key()] != it.value())
			emitEvent("InputMuteStateChanged", Inputs, {{"inputName", doc.sources[it.key()].name}, {"inputUuid", it.key()}, {"inputMuted", it.value().toBool()}});
		if (lastVolumes.contains(it.key()) && std::abs(lastVolumes[it.key()].toDouble() - volumes[it.key()].toDouble()) > 1e-6) {
			const double v = volumes[it.key()].toDouble();
			emitEvent("InputVolumeChanged", Inputs, {{"inputName", doc.sources[it.key()].name}, {"inputUuid", it.key()}, {"inputVolumeMul", v}, {"inputVolumeDb", mulToDb(v)}});
		}
	}
	for (auto it = items.cbegin(); it != items.cend(); ++it)
		if (lastItems.contains(it.key()) && lastItems[it.key()] != it.value()) {
			const Instance &in = doc.instances[it.key()];
			emitEvent("SceneItemEnableStateChanged", SceneItems, {{"sceneName", doc.scenes.value(in.sceneId).name}, {"sceneUuid", in.sceneId},
									     {"sceneItemId", itemId(in.id)}, {"sceneItemEnabled", it.value().toBool()}});
		}
	if (changed("stream"))
		emitEvent("StreamStateChanged", Outputs, outputState(now["stream"].toBool()));
	if (changed("record"))
		emitEvent("RecordStateChanged", Outputs, outputState(now["record"].toBool()));
	if (changed("replay"))
		emitEvent("ReplayBufferStateChanged", Outputs, outputState(now["replay"].toBool()));
	if (changed("vcam"))
		emitEvent("VirtualcamStateChanged", Outputs, outputState(now["vcam"].toBool()));
	m_last = now;
}
