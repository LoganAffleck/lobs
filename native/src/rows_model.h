#pragma once

#include <QAbstractListModel>
#include <QVariantList>

// A list model for QML that keeps its rows (delegates) alive across updates.
//
// The panels used to bind ListViews straight to a QVariantList, so every change (a click, a
// scene switch) replaced the whole array and QML destroyed and rebuilt every row, which was
// slow and leaked memory with each rebuild (LOBS-022/024). Here, `rows` is compared with the
// previous rows by their "id": rows that stayed are updated in place (dataChanged), and only
// rows that came or went are inserted or removed. Delegates keep using `modelData`.
class RowsModel : public QAbstractListModel {
	Q_OBJECT
	Q_PROPERTY(QVariantList rows READ rows WRITE setRows NOTIFY rowsChanged)
	Q_PROPERTY(int count READ count NOTIFY rowsChanged)
public:
	using QAbstractListModel::QAbstractListModel;

	QVariantList rows() const { return m_rows; }
	int count() const { return int(m_rows.size()); }
	void setRows(const QVariantList &next);
	Q_INVOKABLE QVariant get(int i) const { return i >= 0 && i < m_rows.size() ? m_rows[i] : QVariant(); }

	int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(m_rows.size()); }
	QVariant data(const QModelIndex &index, int role) const override;
	QHash<int, QByteArray> roleNames() const override { return {{Qt::UserRole, "modelData"}}; }

signals:
	void rowsChanged();

private:
	static QString keyOf(const QVariant &row, int index);
	QVariantList m_rows;
};
