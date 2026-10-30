#pragma once

#include <QString>
#include <QStringList>

class App;
class Engine;

// LOBS as OBS's frontend, for plugins that use OBS's interface API (obs-frontend-api): docks,
// Tools menu items, events, scene and output controls, save callbacks. OBS's own app registers
// the same way (obs_frontend_set_callbacks_internal). Plugin docks go into one "Plugin panels"
// window, a QMainWindow (also what plugins get as OBS's main window), since LOBS's own panels are
// Qt Quick. Calls tied to OBS's window (Studio Mode, projectors, source dialogs) answer safely.
namespace lobs::frontend {

void install(Engine *engine); // Engine::start, before modules load (plugins register there)
void setApp(App *app);        // once the App exists: scenes, outputs and events come from it
void loaded();                // the show is up: plugins get FINISHED_LOADING and their saved state
void exiting();               // before the engine shuts down: EXIT, and their state is saved

bool hasPanels();
void showPanels();
QStringList toolsItems();         // Tools menu items plugins added, by name
void triggerTool(const QString &name);

} // namespace lobs::frontend
