// Document store with undo history. Every edit is a named command; drags are gestures
// that preview live and land in history as one entry.
#pragma once

#include "doc.h"

#include <QDateTime>
#include <QObject>
#include <functional>
#include <optional>

namespace lobs {

struct HistoryEntry {
	QString label;
	Doc doc;
	QDateTime at;
};

class Store : public QObject {
	Q_OBJECT
public:
	explicit Store(Doc doc, QObject *parent = nullptr);

	const Doc &doc() const { return m_doc; }
	void commit(const QString &label, const std::function<void(Doc &)> &fn);
	void gesture(const std::function<void(Doc &)> &fn); // applies to the pre-gesture doc
	void endGesture(const QString &label);
	const Doc *gestureBase() const { return m_gestureBase ? &*m_gestureBase : nullptr; }
	void replace(const Doc &doc, const QString &label);
	// Adjusts the current state in place, folded into the latest history entry (for follow-up
	// fixes that belong to the last command, like sizes that become known after an import).
	void amend(const std::function<void(Doc &)> &fn);
	// Applies fn to the current doc and every undo/redo state, without a history entry. For
	// facts outside the document that changed under it, like a file moving on disk.
	void rewriteAll(const std::function<void(Doc &)> &fn);

	bool canUndo() const { return !m_past.isEmpty(); }
	bool canRedo() const { return !m_future.isEmpty(); }
	void undo();
	void redo();
	QString undoLabel() const { return m_current.label; }
	QString redoLabel() const { return m_future.isEmpty() ? QString() : m_future.first().label; }
	QVector<HistoryEntry> history() const; // oldest first, includes current
	int historyIndex() const { return int(m_past.size()); }
	void jumpTo(int index);

signals:
	void changed();

private:
	Doc m_doc;
	QVector<HistoryEntry> m_past, m_future;
	HistoryEntry m_current;
	std::optional<Doc> m_gestureBase;
};

} // namespace lobs
