#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

class App;
class Engine;
class QWidget;

// LOBS as OBS's frontend, for plugins that use OBS's interface API (obs-frontend-api): docks,
// Tools menu items, events, scene and output controls, save callbacks. OBS's own app registers
// the same way (obs_frontend_set_callbacks_internal). Each plugin dock becomes a LOBS panel
// (PluginPanel embeds its Qt Widgets window); plugins get a hidden QMainWindow as OBS's main
// window. Calls tied to OBS's window (Studio Mode, projectors, source dialogs) answer safely.
namespace lobs::frontend {

void install(Engine *engine); // Engine::start, before modules load (plugins register there)
void setApp(App *app);        // once the App exists: scenes, outputs and events come from it
void loaded();                // the show is up: plugins get FINISHED_LOADING and their saved state
void exiting();               // before the engine shuts down: EXIT, and their state is saved

// Plugin docks, each shown in its own LOBS panel (PluginPanel): [{id, title}], and the widget.
QVariantList docks();
QWidget *dockWidget(const QString &id);
QStringList toolsItems();         // Tools menu items plugins added, by name
void triggerTool(const QString &name);

} // namespace lobs::frontend
