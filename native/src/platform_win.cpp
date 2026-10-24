#include "platform.h"

#include <QCoreApplication>
#include <QDir>

#include <util/base.h>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dpapi.h>
#include <shellapi.h>
#include <shlobj.h>

namespace platform {

// Opens File Explorer through the shell API (as Explorer's own "Show in folder" does),
// rather than starting explorer.exe with arguments, which breaks on quoting and some setups.
void reveal(const QString &path, bool select)
{
	const std::wstring w = QDir::toNativeSeparators(QDir::cleanPath(path)).toStdWString();
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	// Explorer may reuse a window it already has open on this folder; let it come to the front
	// (Windows otherwise keeps another process from taking focus).
	AllowSetForegroundWindow(ASFW_ANY);
	HRESULT hr = E_FAIL;
	if (select) {
		if (PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(w.c_str())) {
			hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
			ILFree(pidl);
		}
		if (FAILED(hr)) { // fall back to Explorer's own /select switch, through the shell
			const std::wstring args = L"/select,\"" + w + L"\"";
			const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL));
			blog(LOG_INFO, "[lobs] reveal fallback for %s: %lld", path.toUtf8().constData(), (long long)r);
		}
	} else {
		const auto r = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"explore", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
		hr = r > 32 ? S_OK : E_FAIL;
	}
	blog(LOG_INFO, "[lobs] reveal %s (select=%d): hr=0x%08lx", path.toUtf8().constData(), int(select), (unsigned long)hr);
}

void setHidden(const QString &path, bool hidden)
{
	const std::wstring w = QDir::toNativeSeparators(path).toStdWString();
	SetFileAttributesW(w.c_str(), hidden ? FILE_ATTRIBUTE_HIDDEN : FILE_ATTRIBUTE_NORMAL);
}

QString protectSecret(const QString &plain)
{
	if (plain.isEmpty())
		return {};
	const QByteArray in = plain.toUtf8();
	DATA_BLOB src{DWORD(in.size()), reinterpret_cast<BYTE *>(const_cast<char *>(in.data()))}, out{};
	if (!CryptProtectData(&src, L"LOBS stream key", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	const QByteArray enc(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	LocalFree(out.pbData);
	return QString::fromLatin1(enc.toBase64());
}

QString unprotectSecret(const QString &b64)
{
	if (b64.isEmpty())
		return {};
	const QByteArray in = QByteArray::fromBase64(b64.toLatin1());
	DATA_BLOB src{DWORD(in.size()), reinterpret_cast<BYTE *>(const_cast<char *>(in.data()))}, out{};
	if (!CryptUnprotectData(&src, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
		return {};
	const QString plain = QString::fromUtf8(reinterpret_cast<const char *>(out.pbData), int(out.cbData));
	SecureZeroMemory(out.pbData, out.cbData);
	LocalFree(out.pbData);
	return plain;
}

Access access(const QString &) { return Access::Granted; }
void requestAccess(const QString &, std::function<void()> answered) { answered(); }
void openPrivacySettings(const QString &) {}
void relaunchAfterExit() {}
void debugKeyEvents() {}
bool secureKeyboardEntryOn() { return false; }
QVariantList captureDevices(const QString &) { return {}; }

void fatalError(const QString &message)
{
	MessageBoxW(nullptr, reinterpret_cast<const wchar_t *>(message.utf16()), L"LOBS", MB_ICONERROR);
}

QString resourceDir() { return QCoreApplication::applicationDirPath(); }
QString outputDir() { return QCoreApplication::applicationDirPath(); }

} // namespace platform
