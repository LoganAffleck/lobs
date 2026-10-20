#include "hotkeys.h"

#include <obs.h>
#include <obs-hotkey.h>

#include <QKeySequence>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

struct Ctx {
	Hotkeys *self;
	QString action;
};

void onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	auto *c = static_cast<Ctx *>(data);
	c->self->fire(c->action, pressed);
}

// Qt key -> Windows virtual key, which libobs maps to its own key ids.
int toVirtualKey(int k)
{
	if ((k >= Qt::Key_A && k <= Qt::Key_Z) || (k >= Qt::Key_0 && k <= Qt::Key_9))
		return k; // same codes
	if (k >= Qt::Key_F1 && k <= Qt::Key_F24)
		return VK_F1 + (k - Qt::Key_F1);
	switch (k) {
	case Qt::Key_Space: return VK_SPACE;
	case Qt::Key_Tab: return VK_TAB;
	case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
	case Qt::Key_Backspace: return VK_BACK;
	case Qt::Key_Insert: return VK_INSERT;
	case Qt::Key_Delete: return VK_DELETE;
	case Qt::Key_Home: return VK_HOME;
	case Qt::Key_End: return VK_END;
	case Qt::Key_PageUp: return VK_PRIOR;
	case Qt::Key_PageDown: return VK_NEXT;
	case Qt::Key_Left: return VK_LEFT;
	case Qt::Key_Right: return VK_RIGHT;
	case Qt::Key_Up: return VK_UP;
	case Qt::Key_Down: return VK_DOWN;
	case Qt::Key_Pause: return VK_PAUSE;
	case Qt::Key_Print: return VK_SNAPSHOT;
	case Qt::Key_ScrollLock: return VK_SCROLL;
	case Qt::Key_NumLock: return VK_NUMLOCK;
	default: break;
	}
	if (k >= 0x20 && k < 0x7f) { // punctuation: ask Windows which key types it
		const SHORT vk = VkKeyScanW(wchar_t(k));
		return vk == -1 ? 0 : (vk & 0xff);
	}
	return 0;
}

} // namespace

Hotkeys::Hotkeys(Trigger trigger, QObject *parent) : QObject(parent), m_trigger(std::move(trigger)) {}

Hotkeys::~Hotkeys() { clear(); }

void Hotkeys::fire(const QString &action, bool pressed)
{
	QMetaObject::invokeMethod(this, [this, action, pressed]() { m_trigger(action, pressed); }, Qt::QueuedConnection);
}

QString Hotkeys::keyName(int qtKey, int qtModifiers)
{
	if (qtKey == Qt::Key_Control || qtKey == Qt::Key_Shift || qtKey == Qt::Key_Alt || qtKey == Qt::Key_Meta || qtKey == 0 || qtKey == Qt::Key_unknown)
		return {};
	const QKeyCombination combo(Qt::KeyboardModifiers(qtModifiers & (Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)), Qt::Key(qtKey));
	return QKeySequence(combo).toString(QKeySequence::PortableText);
}

void Hotkeys::clear()
{
	for (auto it = m_ids.begin(); it != m_ids.end(); ++it)
		obs_hotkey_unregister(obs_hotkey_id(it.key()));
	m_ids.clear();
	for (void *c : m_ctx)
		delete static_cast<Ctx *>(c);
	m_ctx.clear();
}

void Hotkeys::apply(const QVariantMap &bindings, const QStringList &validActions)
{
	QString sig;
	for (auto it = bindings.begin(); it != bindings.end(); ++it)
		if (validActions.contains(it.key()) && !it.value().toString().isEmpty())
			sig += it.key() + "=" + it.value().toString() + ";";
	if (sig == m_sig)
		return;
	m_sig = sig;
	clear();
	for (auto it = bindings.begin(); it != bindings.end(); ++it) {
		const QString action = it.key();
		const QKeySequence seq = QKeySequence::fromString(it.value().toString(), QKeySequence::PortableText);
		if (!validActions.contains(action) || seq.isEmpty())
			continue;
		const QKeyCombination combo = seq[0];
		const int vk = toVirtualKey(combo.key());
		if (!vk)
			continue;
		auto *ctx = new Ctx{this, action};
		const QByteArray name = ("lobs." + action).toUtf8();
		const obs_hotkey_id id = obs_hotkey_register_frontend(name.constData(), name.constData(), onHotkey, ctx);
		const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
		obs_data_t *b = obs_data_create();
		obs_data_set_string(b, "key", obs_key_to_name(obs_key_from_virtual_key(vk)));
		obs_data_set_bool(b, "shift", mods & Qt::ShiftModifier);
		obs_data_set_bool(b, "control", mods & Qt::ControlModifier);
		obs_data_set_bool(b, "alt", mods & Qt::AltModifier);
		obs_data_set_bool(b, "command", mods & Qt::MetaModifier);
		obs_data_array_t *arr = obs_data_array_create();
		obs_data_array_push_back(arr, b);
		obs_hotkey_load(id, arr);
		obs_data_array_release(arr);
		obs_data_release(b);
		m_ids[id] = action;
		m_ctx << ctx;
	}
}
