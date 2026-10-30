#pragma once

#include <QPointer>
#include <QQuickItem>
#include <QRect>

class QWidget;

// A plugin's dock (a Qt Widgets panel, from OBS's frontend API) inside a LOBS panel. LOBS's
// panels are Qt Quick, so the widget stays a native window of its own, placed exactly over this
// item as a child of LOBS's window (as the native preview is), and follows it every frame: when
// the panel moves, resizes, floats, closes, or a LOBS popup (menu, dialog, Settings) covers it,
// since a native window draws above LOBS's own popups.
class PluginPanel : public QQuickItem {
	Q_OBJECT
	Q_PROPERTY(QString dockId READ dockId WRITE setDockId NOTIFY dockIdChanged)
public:
	explicit PluginPanel(QQuickItem *parent = nullptr);
	~PluginPanel() override;

	QString dockId() const { return m_id; }
	void setDockId(const QString &id);

signals:
	void dockIdChanged();

protected:
	void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
	void sync();
	void attach();
	bool coveredByPopup(const QRectF &r) const;

	QString m_id;
	QPointer<QWidget> m_widget;
	QPointer<QQuickWindow> m_window;
	QMetaObject::Connection m_frame;
	QRect m_rect;
};
