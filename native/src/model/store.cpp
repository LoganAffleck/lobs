#include "store.h"

#include <algorithm>

namespace lobs {

static const int kMaxHistory = 200;

// Each undo step holds a whole Doc. Unchanged parts are shared, but editing one layer copies
// the layer table, about 300 bytes per layer. A big show keeps fewer steps, so history stays
// within roughly 64 MB; small shows keep all 200 (LOBS-036).
static int maxHistory(const Doc &d)
{
	const qint64 perStep = std::max<qint64>(1, qint64(d.instances.size()) * 320);
	return int(std::clamp<qint64>(64ll * 1024 * 1024 / perStep, 30, kMaxHistory));
}

Store::Store(Doc doc, QObject *parent) : QObject(parent), m_doc(std::move(doc))
{
	m_current = {"Open document", m_doc, QDateTime::currentDateTime()};
}

void Store::commit(const QString &label, const std::function<void(Doc &)> &fn)
{
	if (m_gestureBase)
		endGesture(QStringLiteral("Edit"));
	Doc draft = m_doc;
	fn(draft);
	draft.syncVertical(); // vertical layouts follow their scenes
	if (draft == m_doc) // LOBS-017: an edit that changes nothing adds no undo step
		return;
	m_past.push_back(m_current);
	while (m_past.size() > maxHistory(m_doc))
		m_past.removeFirst();
	m_future.clear();
	m_doc = draft;
	m_current = {label, m_doc, QDateTime::currentDateTime()};
	emit changed();
}

void Store::gesture(const std::function<void(Doc &)> &fn)
{
	if (!m_gestureBase)
		m_gestureBase = m_doc;
	Doc draft = *m_gestureBase;
	fn(draft);
	draft.syncVertical();
	m_doc = draft;
	emit changed();
}

void Store::endGesture(const QString &label)
{
	if (!m_gestureBase)
		return;
	m_gestureBase.reset();
	m_past.push_back(m_current);
	while (m_past.size() > maxHistory(m_doc))
		m_past.removeFirst();
	m_future.clear();
	m_current = {label, m_doc, QDateTime::currentDateTime()};
	emit changed();
}

void Store::replace(const Doc &doc, const QString &label)
{
	commit(label, [&](Doc &d) { d = doc; });
}

void Store::amend(const std::function<void(Doc &)> &fn)
{
	if (m_gestureBase)
		return; // never fold into an open drag
	fn(m_doc);
	m_current.doc = m_doc;
	emit changed();
}

void Store::rewriteAll(const std::function<void(Doc &)> &fn)
{
	fn(m_doc);
	if (m_gestureBase)
		fn(*m_gestureBase);
	for (HistoryEntry &e : m_past)
		fn(e.doc);
	for (HistoryEntry &e : m_future)
		fn(e.doc);
	m_current.doc = m_doc;
	emit changed();
}

void Store::undo()
{
	if (m_past.isEmpty())
		return;
	m_future.prepend(m_current);
	m_current = m_past.takeLast();
	m_doc = m_current.doc;
	emit changed();
}

void Store::redo()
{
	if (m_future.isEmpty())
		return;
	m_past.push_back(m_current);
	m_current = m_future.takeFirst();
	m_doc = m_current.doc;
	emit changed();
}

QVector<HistoryEntry> Store::history() const
{
	QVector<HistoryEntry> all = m_past;
	all << m_current << m_future;
	return all;
}

void Store::jumpTo(int index)
{
	QVector<HistoryEntry> all = history();
	if (index < 0 || index >= all.size())
		return;
	m_past = all.mid(0, index);
	m_current = all[index];
	m_future = all.mid(index + 1);
	m_doc = m_current.doc;
	emit changed();
}

} // namespace lobs
