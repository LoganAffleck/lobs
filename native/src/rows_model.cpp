#include "rows_model.h"

QString RowsModel::keyOf(const QVariant &row, int index)
{
	const QVariantMap m = row.toMap();
	const QString id = m.value("id").toString();
	// Rows without an id (dividers, headers) are matched by position.
	return id.isEmpty() ? QString("#%1").arg(index) : id;
}

QVariant RowsModel::data(const QModelIndex &index, int role) const
{
	if (role != Qt::UserRole || index.row() < 0 || index.row() >= m_rows.size())
		return {};
	return m_rows[index.row()];
}

void RowsModel::setRows(const QVariantList &next)
{
	// Walk both lists in order: keep matching rows (updating changed ones), remove rows that
	// are gone, insert rows that are new. Reorders become a remove plus an insert.
	int i = 0;
	for (int k = 0; k < next.size(); ++k) {
		const QString key = keyOf(next[k], k);
		// Drop old rows that don't appear later in `next`.
		while (i < m_rows.size() && keyOf(m_rows[i], i) != key) {
			bool later = false;
			for (int j = k; j < next.size() && !later; ++j)
				later = keyOf(next[j], j) == keyOf(m_rows[i], i);
			if (later)
				break;
			beginRemoveRows({}, i, i);
			m_rows.removeAt(i);
			endRemoveRows();
		}
		if (i < m_rows.size() && keyOf(m_rows[i], i) == key) {
			if (m_rows[i] != next[k]) {
				m_rows[i] = next[k];
				emit dataChanged(index(i), index(i), {Qt::UserRole});
			}
		} else {
			beginInsertRows({}, i, i);
			m_rows.insert(i, next[k]);
			endInsertRows();
		}
		++i;
	}
	if (i < m_rows.size()) {
		beginRemoveRows({}, i, int(m_rows.size()) - 1);
		m_rows.erase(m_rows.begin() + i, m_rows.end());
		endRemoveRows();
	}
	emit rowsChanged();
}
