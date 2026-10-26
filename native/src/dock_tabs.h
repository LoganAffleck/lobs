#pragma once
// Moves a docked panel to a position among the tabs of its group, through the core tab bar's
// moveTabTo (QtQuick's half of it is LOBS's deps/kddw-patches/tabbar-move.patch).
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/TabBar.h>

#include <QObject>

class DockTabs : public QObject
{
	Q_OBJECT
public:
	using QObject::QObject;

	// Index of the panel among its group's tabs, or -1.
	Q_INVOKABLE int indexOf(const QString &uniqueName) const
	{
		KDDockWidgets::Core::DockWidget *dw = nullptr;
		KDDockWidgets::Core::Group *g = groupOf(uniqueName, &dw);
		return g ? g->indexOfDockWidget(dw) : -1;
	}

	// Puts the panel at `index` in its group (clamped), keeping it the tab in front.
	Q_INVOKABLE void move(const QString &uniqueName, int index)
	{
		KDDockWidgets::Core::DockWidget *dw = nullptr;
		KDDockWidgets::Core::Group *g = groupOf(uniqueName, &dw);
		if (!g || g->dockWidgetCount() < 2)
			return;
		const int from = g->indexOfDockWidget(dw);
		const int to = std::clamp(index, 0, g->dockWidgetCount() - 1);
		if (from < 0 || from == to)
			return;
		g->tabBar()->moveTabTo(from, to);
		g->setCurrentDockWidget(dw);
	}

private:
	static KDDockWidgets::Core::Group *groupOf(const QString &uniqueName, KDDockWidgets::Core::DockWidget **dw)
	{
		*dw = KDDockWidgets::DockRegistry::self()->dockByName(uniqueName);
		if (!*dw)
			return nullptr;
		for (KDDockWidgets::Core::Group *g : KDDockWidgets::DockRegistry::self()->groups())
			if (g->containsDockWidget(*dw))
				return g;
		return nullptr;
	}
};
