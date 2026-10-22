#include "crash_report.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>

#include <util/base.h>

#include <windows.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdlib>
#include <cwchar>

namespace {
// Fixed buffers: the handler runs in a crashed process, so it allocates nothing.
wchar_t g_dir[MAX_PATH];
wchar_t g_log[MAX_PATH];
volatile LONG g_writing = 0;

void writeReport(EXCEPTION_POINTERS *ep)
{
	if (InterlockedExchange(&g_writing, 1)) // a second crash while writing: just stop
		return;
	SYSTEMTIME t;
	GetLocalTime(&t);
	wchar_t base[MAX_PATH], path[MAX_PATH];
	swprintf(base, MAX_PATH, L"%ls\\lobs-%04u%02u%02u-%02u%02u%02u", g_dir, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
	swprintf(path, MAX_PATH, L"%ls.dmp", base);
	const HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (f != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
				  MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithUnloadedModules),
				  ep ? &mei : nullptr, nullptr, nullptr);
		CloseHandle(f);
	}
	swprintf(path, MAX_PATH, L"%ls.log", base);
	CopyFileW(g_log, path, FALSE); // lobs.log is flushed per line, and the next launch truncates it
}

LONG WINAPI onException(EXCEPTION_POINTERS *ep)
{
	writeReport(ep);
	return EXCEPTION_CONTINUE_SEARCH; // let Windows end the process as usual
}

// abort(), pure virtual calls and bad CRT arguments never reach the exception filter.
[[noreturn]] void dumpAndExit()
{
	__try {
		RaiseException(0xE0000001, EXCEPTION_NONCONTINUABLE, 0, nullptr); // gives the dump a stack
	} __except (writeReport(GetExceptionInformation()), EXCEPTION_EXECUTE_HANDLER) {
	}
	TerminateProcess(GetCurrentProcess(), 3);
	for (;;) {}
}
}

namespace crash_report {
void install(const QString &crashDir, const QString &logPath)
{
	QDir().mkpath(crashDir);
	wcsncpy(g_dir, reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(crashDir).utf16()), MAX_PATH - 1);
	wcsncpy(g_log, reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(logPath).utf16()), MAX_PATH - 1);
	SetUnhandledExceptionFilter(onException);
	std::signal(SIGABRT, [](int) { dumpAndExit(); });
	_set_purecall_handler([] { dumpAndExit(); });
	_set_invalid_parameter_handler([](const wchar_t *, const wchar_t *, const wchar_t *, unsigned, uintptr_t) { dumpAndExit(); });
}
}

CrashReport::CrashReport(const QString &crashDir, QObject *parent) : QObject(parent), m_dir(crashDir)
{
	QFile seen(m_dir + "/seen.txt");
	const QString last = seen.open(QIODevice::ReadOnly) ? QString::fromUtf8(seen.readAll()).trimmed() : QString();
	const QStringList dumps = QDir(m_dir).entryList({"lobs-*.dmp"}, QDir::Files, QDir::Name);
	if (!dumps.isEmpty() && dumps.last().chopped(4) > last) // names sort by time
		m_latest = dumps.last().chopped(4);
	if (!m_latest.isEmpty())
		blog(LOG_INFO, "[lobs] last run crashed: %s", qPrintable(m_latest));
	// Keep the newest 10 reports.
	for (qsizetype i = 0; i + 10 < dumps.size(); ++i) {
		QFile::remove(m_dir + "/" + dumps[i]);
		QFile::remove(m_dir + "/" + dumps[i].chopped(4) + ".log");
	}
}

void CrashReport::openFolder() const
{
	QDesktopServices::openUrl(QUrl::fromLocalFile(m_dir));
}

void CrashReport::report() const
{
	QUrl url("https://github.com/LoganAffleck/lobs/issues/new");
	QUrlQuery q;
	q.addQueryItem("title", "Crash in LOBS " LOBS_VERSION);
	q.addQueryItem("body", "**What were you doing when it crashed?**\n\n\n"
			       "**Log:** drag " + m_latest + ".log from the crash folder that just opened into this box. "
			       "(Don't post the .dmp here: it can hold your stream key. Share it privately if asked.)\n\n"
			       "LOBS " LOBS_VERSION);
	url.setQuery(q);
	openFolder();
	QDesktopServices::openUrl(url);
}

void CrashReport::dismiss()
{
	if (m_latest.isEmpty())
		return;
	QFile seen(m_dir + "/seen.txt");
	if (seen.open(QIODevice::WriteOnly | QIODevice::Truncate))
		seen.write(m_latest.toUtf8());
	m_latest.clear();
	emit changed();
}
