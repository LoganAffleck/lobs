#pragma once

#include <QObject>
#include <QString>

// Crash reports: if LOBS crashes, a dump (a minidump on Windows, a backtrace on macOS) and a
// copy of lobs.log land in <local data>/crashes. On the next launch the UI offers to report it.
namespace crash_report {
void install(const QString &crashDir, const QString &logPath); // crash_report_win.cpp / _mac.cpp
extern const char *const kDumpExt; // ".dmp" or ".crash"
}

class CrashReport : public QObject {
	Q_OBJECT
	Q_PROPERTY(bool pending READ pending NOTIFY changed)
	Q_PROPERTY(QString folder READ folder CONSTANT)
public:
	explicit CrashReport(const QString &crashDir, QObject *parent = nullptr);
	bool pending() const { return !m_latest.isEmpty(); }
	QString folder() const { return m_dir; }
	Q_INVOKABLE void openFolder() const;
	Q_INVOKABLE void report() const; // a prefilled GitHub issue; the files are attached by hand
	Q_INVOKABLE void dismiss();

signals:
	void changed();

private:
	QString m_dir, m_latest; // m_latest: newest dump not yet seen, without extension
};
