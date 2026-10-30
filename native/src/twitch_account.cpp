#include "twitch_account.h"
#include "app.h"
#include "platform.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

namespace {
const QString kClientId = "y0mck0jqerrkhzvqiy1pzhagusz1rn";
// Twitch sends the sign-in to the website, as OBS's goes to obsproject.com; that page passes the
// code on to localhost:5340, where LOBS listens while signing in.
const QString kRedirect = "https://streamlobs.com/twitch/callback"; // registered in the Twitch console
const quint16 kPort = 5340;
const QString kTokenServer = "https://streamlobs.com/api/twitch/token";
const QString kScopes = "channel:read:stream_key"; // what OBS asks for

QNetworkRequest helix(const QString &path, const QString &token)
{
	QNetworkRequest r(QUrl("https://api.twitch.tv/helix/" + path));
	r.setRawHeader("Client-Id", kClientId.toUtf8());
	r.setRawHeader("Authorization", "Bearer " + token.toUtf8());
	return r;
}

QJsonObject body(QNetworkReply *r) { return QJsonDocument::fromJson(r->readAll()).object(); }

// The page Twitch's redirect lands on, in the sign-in window.
QByteArray page(const QString &title, const QString &text)
{
	const QByteArray html = QString("<!doctype html><meta charset=utf-8><title>LOBS</title>"
					"<body style=\"margin:0;height:100vh;display:grid;place-content:center;text-align:center;"
					"background:#0e0c14;color:#ece8f4;font:16px system-ui\"><div><h2 style=\"font-weight:600\">%1</h2>"
					"<p style=\"color:#a39cb3\">%2</p></div>")
					.arg(title.toHtmlEscaped(), text.toHtmlEscaped())
					.toUtf8();
	return "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: " +
	       QByteArray::number(html.size()) + "\r\n\r\n" + html;
}
} // namespace

TwitchAccount::TwitchAccount(App *app, QObject *parent) : QObject(parent), m_app(app), m_net(new QNetworkAccessManager(this))
{
	QSettings s;
	m_access = platform::unprotectSecret(s.value("twitch/access").toString());
	m_refresh = platform::unprotectSecret(s.value("twitch/refresh").toString());
	m_login = s.value("twitch/login").toString();
	m_displayName = s.value("twitch/displayName").toString();
	m_userId = s.value("twitch/userId").toString();
	// Twitch asks apps to validate their tokens at start and every hour.
	m_validateTimer.setInterval(60 * 60 * 1000);
	connect(&m_validateTimer, &QTimer::timeout, this, &TwitchAccount::validate);
	if (connected()) {
		QTimer::singleShot(3000, this, &TwitchAccount::validate);
		m_validateTimer.start();
	}
}

void TwitchAccount::save()
{
	QSettings s;
	if (!connected()) {
		s.remove("twitch");
		return;
	}
	s.setValue("twitch/access", platform::protectSecret(m_access));
	s.setValue("twitch/refresh", platform::protectSecret(m_refresh));
	s.setValue("twitch/login", m_login);
	s.setValue("twitch/displayName", m_displayName);
	s.setValue("twitch/userId", m_userId);
}

QString TwitchAccount::beginSignIn()
{
	cancelSignIn();
	m_error.clear();
	m_server = new QTcpServer(this);
	if (!m_server->listen(QHostAddress::LocalHost, kPort)) {
		fail(QString("Twitch sign-in needs port %1, which another app is using.").arg(kPort));
		delete m_server, m_server = nullptr;
		return {};
	}
	connect(m_server, &QTcpServer::newConnection, this, [this] {
		while (QTcpSocket *sock = m_server ? m_server->nextPendingConnection() : nullptr) {
			connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
			connect(sock, &QTcpSocket::readyRead, this, [this, sock] {
				if (!sock->canReadLine())
					return;
				const QByteArray line = sock->readLine();
				const bool ours = line.contains("/twitch/callback");
				sock->write(ours ? page("Signed in to Twitch", "Finishing up in LOBS…") : page("LOBS", ""));
				sock->disconnectFromHost();
				if (ours)
					onCallback(line);
			});
		}
	});
	m_state = QString::number(QRandomGenerator::system()->generate64(), 36);
	QUrl u("https://id.twitch.tv/oauth2/authorize");
	QUrlQuery q;
	q.addQueryItem("client_id", kClientId);
	q.addQueryItem("redirect_uri", kRedirect);
	q.addQueryItem("response_type", "code");
	q.addQueryItem("scope", kScopes);
	q.addQueryItem("state", m_state);
	q.addQueryItem("force_verify", "true"); // shows which account, so you can switch
	u.setQuery(q);
	emit changed();
	return u.toString(QUrl::FullyEncoded);
}

void TwitchAccount::cancelSignIn()
{
	if (m_server) {
		m_server->close();
		m_server->deleteLater();
		m_server = nullptr;
	}
}

void TwitchAccount::onCallback(const QByteArray &requestLine)
{
	// "GET /twitch/callback?code=...&scope=...&state=... HTTP/1.1"
	const QList<QByteArray> parts = requestLine.split(' ');
	if (parts.size() < 2)
		return;
	const QUrlQuery q(QUrl("http://localhost" + QString::fromUtf8(parts[1])));
	cancelSignIn();
	if (q.queryItemValue("state") != m_state)
		return fail("Twitch's answer didn't match this sign-in. Try again.");
	if (q.hasQueryItem("error"))
		return fail(q.hasQueryItem("error_description") ? q.queryItemValue("error_description", QUrl::FullyDecoded) : "Sign-in was cancelled.");
	exchange(q.queryItemValue("code"));
}

void TwitchAccount::exchange(const QString &code)
{
	setBusy(true);
	QNetworkRequest r{QUrl(kTokenServer)};
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	QNetworkReply *rep = m_net->post(r, QJsonDocument(QJsonObject{{"code", code}, {"redirect_uri", kRedirect}}).toJson(QJsonDocument::Compact));
	connect(rep, &QNetworkReply::finished, this, [this, rep] {
		rep->deleteLater();
		const QJsonObject o = body(rep);
		if (o.value("access_token").toString().isEmpty())
			return fail("LOBS couldn't finish signing in (" + (o.value("message").toString().isEmpty() ? rep->errorString() : o.value("message").toString()) + ").");
		m_access = o.value("access_token").toString();
		m_refresh = o.value("refresh_token").toString();
		fetchAccount();
	});
}

void TwitchAccount::fetchAccount()
{
	QNetworkReply *rep = m_net->get(helix("users", m_access));
	connect(rep, &QNetworkReply::finished, this, [this, rep] {
		rep->deleteLater();
		const QJsonArray data = body(rep).value("data").toArray();
		if (data.isEmpty()) {
			m_access.clear(), m_refresh.clear();
			return fail("Twitch didn't return your account.");
		}
		const QJsonObject u = data.first().toObject();
		m_userId = u.value("id").toString();
		m_login = u.value("login").toString();
		m_displayName = u.value("display_name").toString();
		save();
		m_validateTimer.start();
		// The Twitch panels use the same channel.
		m_app->setUiValue("twitch/channel", m_login);
		emit changed();
		refreshStreamKey();
	});
}

void TwitchAccount::refreshStreamKey()
{
	if (!connected())
		return;
	setBusy(true);
	QNetworkReply *rep = m_net->get(helix("streams/key?broadcaster_id=" + m_userId, m_access));
	connect(rep, &QNetworkReply::finished, this, [this, rep] {
		rep->deleteLater();
		if (rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
			return refreshTokens([this](bool ok) { if (ok) refreshStreamKey(); else fail("Sign in to Twitch again."); });
		const QJsonArray data = body(rep).value("data").toArray();
		const QString key = data.isEmpty() ? QString() : data.first().toObject().value("stream_key").toString();
		if (key.isEmpty())
			return fail("Twitch didn't return your stream key.");
		const QString service = m_app->settings().value("service").toString();
		QVariantMap ch{{"key", key}};
		if (!service.startsWith("Twitch"))
			ch["service"] = "Twitch";
		m_app->saveSettings(ch);
		setBusy(false);
		emit signedIn();
	});
}

void TwitchAccount::refreshTokens(std::function<void(bool)> done)
{
	if (m_refresh.isEmpty())
		return done(false);
	QNetworkRequest r{QUrl(kTokenServer)};
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	QNetworkReply *rep = m_net->post(r, QJsonDocument(QJsonObject{{"refresh_token", m_refresh}}).toJson(QJsonDocument::Compact));
	connect(rep, &QNetworkReply::finished, this, [this, rep, done] {
		rep->deleteLater();
		const QJsonObject o = body(rep);
		if (o.value("access_token").toString().isEmpty()) {
			// A network error keeps the account; Twitch refusing the refresh token ends it.
			if (rep->error() == QNetworkReply::NoError || rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400) {
				m_access.clear(), m_refresh.clear();
				save();
				emit changed();
			}
			return done(false);
		}
		m_access = o.value("access_token").toString();
		if (!o.value("refresh_token").toString().isEmpty())
			m_refresh = o.value("refresh_token").toString();
		save();
		done(true);
	});
}

void TwitchAccount::validate()
{
	if (!connected())
		return;
	QNetworkRequest r(QUrl("https://id.twitch.tv/oauth2/validate"));
	r.setRawHeader("Authorization", "OAuth " + m_access.toUtf8());
	QNetworkReply *rep = m_net->get(r);
	connect(rep, &QNetworkReply::finished, this, [this, rep] {
		rep->deleteLater();
		if (rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401)
			refreshTokens([this](bool) { emit changed(); });
	});
}

void TwitchAccount::disconnectAccount()
{
	cancelSignIn();
	if (!m_access.isEmpty()) { // revoke, so the token stops working on Twitch's side too
		QNetworkRequest r(QUrl("https://id.twitch.tv/oauth2/revoke"));
		r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
		QNetworkReply *rep = m_net->post(r, QByteArray("client_id=" + kClientId.toUtf8() + "&token=" + m_access.toUtf8()));
		connect(rep, &QNetworkReply::finished, rep, &QObject::deleteLater);
	}
	m_access.clear(), m_refresh.clear(), m_login.clear(), m_displayName.clear(), m_userId.clear(), m_error.clear();
	m_validateTimer.stop();
	save();
	emit changed();
}

void TwitchAccount::fail(const QString &why)
{
	m_error = why;
	m_busy = false;
	emit changed();
	emit signInFailed(why);
}
