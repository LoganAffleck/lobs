#include "plugin_panel.h"
#include "frontend.h"

#include <QQuickWindow>
#include <QWidget>
#include <QWindow>

PluginPanel::PluginPanel(QQuickItem *parent) : QQuickItem(parent) {}

PluginPanel::~PluginPanel()
{
	// The widget is the plugin's: hand it back hidden, out of LOBS's window.
	if (m_widget && m_widget->windowHandle()) {
		m_widget->hide();
		m_widget->windowHandle()->setParent(nullptr);
	}
}

void PluginPanel::setDockId(const QString &id)
{
	if (id == m_id)
		return;
	m_id = id;
	emit dockIdChanged();
	attach();
}

void PluginPanel::itemChange(ItemChange change, const ItemChangeData &value)
{
	QQuickItem::itemChange(change, value);
	if (change == ItemSceneChange) // docked into another window, or floated
		attach();
	else if (change == ItemVisibleHasChanged)
		sync();
}

void PluginPanel::attach()
{
	QObject::disconnect(m_frame);
	m_window = window();
	m_widget = lobs::frontend::dockWidget(m_id);
	if (!m_window || !m_widget)
		return;
	// A native window of its own, then a child of LOBS's window (the panel's, when floating).
	m_widget->setParent(nullptr);
	m_widget->setWindowFlags(Qt::FramelessWindowHint);
	m_widget->setAttribute(Qt::WA_NativeWindow);
	m_widget->winId();
	if (QWindow *w = m_widget->windowHandle())
		w->setParent(m_window);
	m_rect = {};
	m_frame = connect(m_window, &QQuickWindow::afterAnimating, this, &PluginPanel::sync);
	sync();
}

// LOBS's popups (menus, dialogs, Settings) live in the window's overlay layer.
bool PluginPanel::coveredByPopup(const QRectF &r) const
{
	if (!m_window)
		return false;
	for (QQuickItem *layer : m_window->contentItem()->childItems()) {
		if (!layer->inherits("QQuickOverlay"))
			continue;
		for (QQuickItem *p : layer->childItems())
			if (p->isVisible() && p->opacity() > 0 && p->mapRectToScene(QRectF(0, 0, p->width(), p->height())).intersects(r))
				return true;
	}
	return false;
}

void PluginPanel::sync()
{
	if (!m_widget || !m_window)
		return;
	const QRectF scene = mapRectToScene(QRectF(0, 0, width(), height()));
	const bool show = isVisible() && width() > 8 && height() > 8 && !coveredByPopup(scene);
	if (show != m_widget->isVisible())
		m_widget->setVisible(show);
	if (!show)
		return;
	const QRect r = scene.toAlignedRect();
	if (r != m_rect) {
		m_rect = r;
		m_widget->windowHandle()->setGeometry(r); // in the parent window's coordinates
	}
}
