#include "crash_report.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>

#include <util/base.h>

CrashReport::CrashReport(const QString &crashDir, QObject *parent) : QObject(parent), m_dir(crashDir)
{
	QFile seen(m_dir + "/seen.txt");
	const QString last = seen.open(QIODevice::ReadOnly) ? QString::fromUtf8(seen.readAll()).trimmed() : QString();
	const QString ext = crash_report::kDumpExt;
	const QStringList dumps = QDir(m_dir).entryList({"lobs-*" + ext}, QDir::Files, QDir::Name);
	if (!dumps.isEmpty() && dumps.last().chopped(ext.size()) > last) // names sort by time
		m_latest = dumps.last().chopped(ext.size());
	if (!m_latest.isEmpty())
		blog(LOG_INFO, "[lobs] last run crashed: %s", qPrintable(m_latest));
	// Keep the newest 10 reports.
	for (qsizetype i = 0; i + 10 < dumps.size(); ++i) {
		QFile::remove(m_dir + "/" + dumps[i]);
		QFile::remove(m_dir + "/" + dumps[i].chopped(ext.size()) + ".log");
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
			       "(Don't post the " + QString(crash_report::kDumpExt) + " here: it can hold your stream key. Share it privately if asked.)\n\n"
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
