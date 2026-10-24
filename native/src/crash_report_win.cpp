#include "crash_report.h"

#include <QDir>

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
const char *const kDumpExt = ".dmp";

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

