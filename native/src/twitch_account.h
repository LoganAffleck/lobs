#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

class App;
class QNetworkAccessManager;
class QTcpServer;

// "Connect with Twitch", as OBS's Connect Account. Twitch's sign-in page opens in LOBS's own
// browser (the profile the Twitch panels use, so they're signed in too); Twitch sends the
// one-time code to streamlobs.com/twitch/callback, which passes it on to localhost:5340, where
// this listens while signing in.
// LOBS's server (streamlobs.com/api/twitch/token) holds the client secret and trades the code
// for tokens. Then the stream key and channel come from Twitch's API and fill in the stream
// settings. Tokens are stored encrypted (platform::protectSecret), validated at start and hourly,
// and refreshed through the server.
class TwitchAccount : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool connected READ connected NOTIFY changed)
	Q_PROPERTY(QString login READ login NOTIFY changed)       // the channel, as in twitch.tv/<login>
	Q_PROPERTY(QString displayName READ displayName NOTIFY changed)
	Q_PROPERTY(bool busy READ busy NOTIFY changed)
	Q_PROPERTY(QString error READ error NOTIFY changed)
public:
	explicit TwitchAccount(App *app, QObject *parent = nullptr);

	bool connected() const { return !m_access.isEmpty(); }
	QString login() const { return m_login; }
	QString displayName() const { return m_displayName; }
	bool busy() const { return m_busy; }
	QString error() const { return m_error; }

	// Starts listening for Twitch's answer and returns the sign-in page to open ("" on failure:
	// see error).
	Q_INVOKABLE QString beginSignIn();
	Q_INVOKABLE void cancelSignIn();
	Q_INVOKABLE void disconnectAccount();
	// Fetches the stream key again and saves it (it changes when reset on Twitch).
	Q_INVOKABLE void refreshStreamKey();

signals:
	void changed();
	void signedIn();          // the sign-in window can close
	void signInFailed(const QString &why);

private:
	void onCallback(const QByteArray &requestLine);
	void exchange(const QString &code);
	void refreshTokens(std::function<void(bool)> done);
	void validate();
	void fetchAccount();
	void fail(const QString &why);
	void setBusy(bool b) { if (m_busy != b) { m_busy = b; emit changed(); } }
	void save();

	App *m_app;
	QNetworkAccessManager *m_net;
	QTcpServer *m_server = nullptr;
	QTimer m_validateTimer;
	QString m_state;
	QString m_access, m_refresh, m_login, m_displayName, m_userId, m_error;
	bool m_busy = false;
};
