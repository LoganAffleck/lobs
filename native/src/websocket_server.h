#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QVariantMap>

class App;
class QTcpServer;
class QTcpSocket;

// Remote control over the obs-websocket protocol (v5, rpcVersion 1), so Stream Deck, Touch Portal
// and VTuber apps such as VNyan control LOBS the way they control OBS. LOBS speaks it itself
// (OBS's obs-websocket plugin needs OBS's interface), answering in LOBS's own terms: scenes and
// sources by their names, layers as scene items. Off by default, like OBS; port 4455.
//
// Covered: GetVersion, GetStats, scenes (list, current program/preview, set), inputs (list,
// mute, volume), scene items (list, id, enabled), stream, record, replay buffer and virtual
// camera (status, start, stop, toggle; save replay), plus their events. Not yet: filters, input
// settings, transitions, studio mode, hotkeys.
class ObsWebSocket : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
	Q_PROPERTY(int port READ port NOTIFY changed)
	Q_PROPERTY(QString password READ password NOTIFY changed)
	Q_PROPERTY(bool authRequired READ authRequired NOTIFY changed)
	Q_PROPERTY(int clients READ clients NOTIFY changed)
	Q_PROPERTY(QString error READ error NOTIFY changed)
public:
	explicit ObsWebSocket(App *app, QObject *parent = nullptr);

	bool enabled() const { return m_enabled; }
	int port() const { return m_port; }
	QString password() const { return m_password; }
	bool authRequired() const { return m_authRequired; }
	int clients() const { return int(m_identified.size()); }
	QString error() const { return m_error; }

	Q_INVOKABLE void setEnabled(bool on);
	Q_INVOKABLE void setPort(int port);
	Q_INVOKABLE void setAuthRequired(bool on);
	Q_INVOKABLE void newPassword(); // a random one, as OBS generates

signals:
	void changed();

private:
	struct Client {
		QByteArray buf;     // bytes not yet parsed
		QByteArray message; // a fragmented message so far
		bool upgraded = false;
		bool identified = false;
		quint32 events = 0; // event subscriptions (bit flags, as obs-websocket's)
		QString challenge, salt;
	};
	void restart();
	void save();
	void onReadyRead(QTcpSocket *s);
	void handshake(QTcpSocket *s, Client &c);
	void onMessage(QTcpSocket *s, Client &c, const QByteArray &text);
	void send(QTcpSocket *s, const QJsonObject &msg);
	void sendFrame(QTcpSocket *s, int opcode, const QByteArray &payload);
	QJsonObject request(const QString &type, const QJsonObject &data, int &code, QString &comment);
	void emitEvent(const QString &type, quint32 intent, const QJsonObject &data = {});
	void watch(); // compares LOBS's state with the last one sent, and sends events

	// Lookups by the names (or uuids) clients send.
	QString sceneId(const QJsonObject &d) const;
	QString sourceId(const QJsonObject &d) const;
	int itemId(const QString &instanceId);
	QString instanceForItem(const QString &sceneId, int itemId) const;

	App *m_app;
	QTcpServer *m_server = nullptr;
	QHash<QTcpSocket *, Client> m_clients;
	QSet<QTcpSocket *> m_identified;
	bool m_enabled = false, m_authRequired = true;
	int m_port = 4455;
	QString m_password, m_error;
	QHash<QString, int> m_itemIds; // layer id -> scene item id (stable while LOBS runs)
	int m_nextItemId = 1;
	QVariantMap m_last; // what the last events described
};
