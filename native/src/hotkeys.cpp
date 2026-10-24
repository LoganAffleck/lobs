#include "hotkeys.h"

#include <obs.h>
#include <obs-hotkey.h>
#include <util/base.h>

#include <QHash>
#include <QKeySequence>

#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <Carbon/Carbon.h> // kVK_* key codes
#endif

namespace {

struct Ctx {
	Hotkeys *self;
	QString action;
};

void onHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	auto *c = static_cast<Ctx *>(data);
	blog(LOG_INFO, "[lobs] hotkey %s %s", qPrintable(c->action), pressed ? "pressed" : "released");
	c->self->fire(c->action, pressed);
}

#ifdef Q_OS_WIN
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

#else
// Qt key -> macOS virtual key code (kVK_*), which libobs maps to its own key ids.
int toVirtualKey(int k)
{
	static const QHash<int, int> keys = {
		{Qt::Key_A, kVK_ANSI_A}, {Qt::Key_B, kVK_ANSI_B}, {Qt::Key_C, kVK_ANSI_C}, {Qt::Key_D, kVK_ANSI_D},
		{Qt::Key_E, kVK_ANSI_E}, {Qt::Key_F, kVK_ANSI_F}, {Qt::Key_G, kVK_ANSI_G}, {Qt::Key_H, kVK_ANSI_H},
		{Qt::Key_I, kVK_ANSI_I}, {Qt::Key_J, kVK_ANSI_J}, {Qt::Key_K, kVK_ANSI_K}, {Qt::Key_L, kVK_ANSI_L},
		{Qt::Key_M, kVK_ANSI_M}, {Qt::Key_N, kVK_ANSI_N}, {Qt::Key_O, kVK_ANSI_O}, {Qt::Key_P, kVK_ANSI_P},
		{Qt::Key_Q, kVK_ANSI_Q}, {Qt::Key_R, kVK_ANSI_R}, {Qt::Key_S, kVK_ANSI_S}, {Qt::Key_T, kVK_ANSI_T},
		{Qt::Key_U, kVK_ANSI_U}, {Qt::Key_V, kVK_ANSI_V}, {Qt::Key_W, kVK_ANSI_W}, {Qt::Key_X, kVK_ANSI_X},
		{Qt::Key_Y, kVK_ANSI_Y}, {Qt::Key_Z, kVK_ANSI_Z},
		{Qt::Key_0, kVK_ANSI_0}, {Qt::Key_1, kVK_ANSI_1}, {Qt::Key_2, kVK_ANSI_2}, {Qt::Key_3, kVK_ANSI_3},
		{Qt::Key_4, kVK_ANSI_4}, {Qt::Key_5, kVK_ANSI_5}, {Qt::Key_6, kVK_ANSI_6}, {Qt::Key_7, kVK_ANSI_7},
		{Qt::Key_8, kVK_ANSI_8}, {Qt::Key_9, kVK_ANSI_9},
		{Qt::Key_F1, kVK_F1}, {Qt::Key_F2, kVK_F2}, {Qt::Key_F3, kVK_F3}, {Qt::Key_F4, kVK_F4},
		{Qt::Key_F5, kVK_F5}, {Qt::Key_F6, kVK_F6}, {Qt::Key_F7, kVK_F7}, {Qt::Key_F8, kVK_F8},
		{Qt::Key_F9, kVK_F9}, {Qt::Key_F10, kVK_F10}, {Qt::Key_F11, kVK_F11}, {Qt::Key_F12, kVK_F12},
		{Qt::Key_F13, kVK_F13}, {Qt::Key_F14, kVK_F14}, {Qt::Key_F15, kVK_F15}, {Qt::Key_F16, kVK_F16},
		{Qt::Key_F17, kVK_F17}, {Qt::Key_F18, kVK_F18}, {Qt::Key_F19, kVK_F19}, {Qt::Key_F20, kVK_F20},
		{Qt::Key_Space, kVK_Space}, {Qt::Key_Tab, kVK_Tab}, {Qt::Key_Return, kVK_Return}, {Qt::Key_Enter, kVK_ANSI_KeypadEnter},
		{Qt::Key_Backspace, kVK_Delete}, {Qt::Key_Delete, kVK_ForwardDelete}, {Qt::Key_Home, kVK_Home}, {Qt::Key_End, kVK_End},
		{Qt::Key_PageUp, kVK_PageUp}, {Qt::Key_PageDown, kVK_PageDown}, {Qt::Key_Left, kVK_LeftArrow},
		{Qt::Key_Right, kVK_RightArrow}, {Qt::Key_Up, kVK_UpArrow}, {Qt::Key_Down, kVK_DownArrow},
		{Qt::Key_Minus, kVK_ANSI_Minus}, {Qt::Key_Equal, kVK_ANSI_Equal}, {Qt::Key_BracketLeft, kVK_ANSI_LeftBracket},
		{Qt::Key_BracketRight, kVK_ANSI_RightBracket}, {Qt::Key_Backslash, kVK_ANSI_Backslash}, {Qt::Key_Semicolon, kVK_ANSI_Semicolon},
		{Qt::Key_Apostrophe, kVK_ANSI_Quote}, {Qt::Key_Comma, kVK_ANSI_Comma}, {Qt::Key_Period, kVK_ANSI_Period},
		{Qt::Key_Slash, kVK_ANSI_Slash}, {Qt::Key_QuoteLeft, kVK_ANSI_Grave},
	};
	return keys.value(k, -1);
}

#endif

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
#ifdef Q_OS_WIN
		if (!vk)
			continue;
#else
		if (vk < 0) // kVK_ANSI_A is 0
			continue;
#endif
		auto *ctx = new Ctx{this, action};
		const QByteArray name = ("lobs." + action).toUtf8();
		const obs_hotkey_id id = obs_hotkey_register_frontend(name.constData(), name.constData(), onHotkey, ctx);
		const Qt::KeyboardModifiers mods = combo.keyboardModifiers();
		obs_data_t *b = obs_data_create();
		obs_data_set_string(b, "key", obs_key_to_name(obs_key_from_virtual_key(vk)));
		blog(LOG_INFO, "[lobs] hotkey %s bound to %s (virtual key %d, Qt modifiers 0x%x)", qPrintable(action),
		     obs_key_to_name(obs_key_from_virtual_key(vk)), vk, unsigned(mods));
		obs_data_set_bool(b, "shift", mods & Qt::ShiftModifier);
#ifdef Q_OS_MACOS
		// Qt names Command "Ctrl" on macOS (so "Ctrl+S" is ⌘S) and the Control key "Meta".
		obs_data_set_bool(b, "control", mods & Qt::MetaModifier);
		obs_data_set_bool(b, "command", mods & Qt::ControlModifier);
#else
		obs_data_set_bool(b, "control", mods & Qt::ControlModifier);
		obs_data_set_bool(b, "command", mods & Qt::MetaModifier);
#endif
		obs_data_set_bool(b, "alt", mods & Qt::AltModifier);
		obs_data_array_t *arr = obs_data_array_create();
		obs_data_array_push_back(arr, b);
		obs_hotkey_load(id, arr);
		obs_data_array_release(arr);
		obs_data_release(b);
		m_ids[id] = action;
		m_ctx << ctx;
	}
}
