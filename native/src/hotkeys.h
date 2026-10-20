// Global hotkeys through libobs's hotkey system (works while a game or another app has focus,
// like OBS). Bindings are app settings: action id -> key combo in QKeySequence portable text
// ("Ctrl+Shift+F1"). Action ids:
//   stream, record, replay, replaySave, vcam      toggles / save
//   scene:<sceneId>                               show that scene
//   mute:<sourceId>, ptt:<sourceId>               mute toggle, push-to-talk (hold)
#pragma once

#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <functional>

class Hotkeys : public QObject {
	Q_OBJECT
public:
	using Trigger = std::function<void(const QString &action, bool pressed)>;
	explicit Hotkeys(Trigger trigger, QObject *parent = nullptr);
	~Hotkeys() override;

	// Re-register exactly the bound actions that still exist.
	void apply(const QVariantMap &bindings, const QStringList &validActions);
	static QString keyName(int qtKey, int qtModifiers); // "" for modifier-only presses

	void fire(const QString &action, bool pressed); // from the libobs hotkey thread

private:
	void clear();
	Trigger m_trigger;
	QHash<size_t, QString> m_ids; // obs_hotkey_id -> action
	QList<void *> m_ctx;          // callback contexts, freed with their registrations
	QString m_sig;
};
