#include "crash_report.h"

#include <QDir>

#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>

namespace {
// Fixed buffers: the handler runs in a crashed process, so it allocates nothing.
char g_dir[1024];
char g_log[1024];
volatile sig_atomic_t g_writing = 0;

void writeAll(int fd, const char *s) { (void)!write(fd, s, strlen(s)); }

void onSignal(int sig, siginfo_t *, void *)
{
	if (!g_writing) {
		g_writing = 1;
		const time_t now = time(nullptr);
		struct tm t;
		localtime_r(&now, &t);
		char base[1100], path[1200];
		snprintf(base, sizeof(base), "%s/lobs-%04d%02d%02d-%02d%02d%02d", g_dir, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
			 t.tm_min, t.tm_sec);
		snprintf(path, sizeof(path), "%s.crash", base);
		if (const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644); fd >= 0) {
			char head[128];
			snprintf(head, sizeof(head), "LOBS " LOBS_VERSION " crashed: signal %d (%s)\n\n", sig, strsignal(sig));
			writeAll(fd, head);
			void *frames[128];
			backtrace_symbols_fd(frames, backtrace(frames, 128), fd);
			close(fd);
		}
		// lobs.log is flushed per line, and the next launch truncates it.
		snprintf(path, sizeof(path), "%s.log", base);
		const int in = open(g_log, O_RDONLY), out = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		char buf[16384];
		for (ssize_t n; in >= 0 && out >= 0 && (n = read(in, buf, sizeof(buf))) > 0;)
			(void)!write(out, buf, size_t(n));
		if (in >= 0)
			close(in);
		if (out >= 0)
			close(out);
	}
	// Let macOS end the process as usual (and write its own report to DiagnosticReports).
	signal(sig, SIG_DFL);
	raise(sig);
}
}

namespace crash_report {
const char *const kDumpExt = ".crash";

void install(const QString &crashDir, const QString &logPath)
{
	QDir().mkpath(crashDir);
	strlcpy(g_dir, QDir::toNativeSeparators(crashDir).toUtf8().constData(), sizeof(g_dir));
	strlcpy(g_log, QDir::toNativeSeparators(logPath).toUtf8().constData(), sizeof(g_log));
	struct sigaction sa = {};
	sa.sa_sigaction = onSignal;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP})
		sigaction(sig, &sa, nullptr);
}
}
