#include "frontend.h"
#include "app.h"
#include "engine.h"

#include <obs-frontend-internal.hpp>
#include <util/config-file.h>

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QMainWindow>
#include <QStandardPaths>
#include <QTimer>

#include <vector>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace lobs::frontend {

namespace {

QString folder()
{
	const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/frontend";
	QDir().mkpath(d);
	return d;
}

char *dup(const QString &s) { return bstrdup(s.toUtf8().constData()); }

// The window plugin docks live in, and what plugins get as OBS's main window.
class PanelsWindow : public QMainWindow {
public:
	PanelsWindow()
	{
		setWindowTitle("Plugin panels");
		setDockNestingEnabled(true);
		setCentralWidget(nullptr);
		resize(420, 560);
	}
};

class Frontend : public obs_frontend_callbacks {
public:
	Engine *engine = nullptr;
	App *app = nullptr;
	PanelsWindow *panels = nullptr;
	struct EventCb { obs_frontend_event_cb cb; void *data; };
	struct SaveCb { obs_frontend_save_cb cb; void *data; };
	struct Tool { QString name; obs_frontend_cb cb; void *data; QAction *action; };
	std::vector<EventCb> events;
	std::vector<SaveCb> saves, preloads;
	std::vector<Tool> tools;
	QHash<QString, QDockWidget *> docks;
	config_t *profileConfig = nullptr, *appConfig = nullptr, *userConfig = nullptr;

	PanelsWindow *window()
	{
		if (!panels && qobject_cast<QApplication *>(QCoreApplication::instance()))
			panels = new PanelsWindow;
		return panels;
	}
	config_t *config(config_t *&c, const char *name)
	{
		if (!c)
			config_open(&c, (folder() + "/" + name).toUtf8().constData(), CONFIG_OPEN_ALWAYS);
		return c;
	}
	void send(obs_frontend_event e)
	{
		const auto list = events; // a callback may remove itself
		for (const EventCb &c : list)
			c.cb(e, c.data);
	}

	// ---- window ----
	void *obs_frontend_get_main_window(void) override { return window(); }
	void *obs_frontend_get_main_window_handle(void) override { return window() ? reinterpret_cast<void *>(window()->winId()) : nullptr; }
	void *obs_frontend_get_system_tray(void) override { return nullptr; }

	// ---- scenes: LOBS's scenes, which are OBS scenes named by their LOBS ids ----
	void obs_frontend_get_scenes(struct obs_frontend_source_list *sources) override
	{
		if (!app)
			return;
		for (const QString &id : app->doc().sceneOrder)
			if (obs_source_t *s = engine->sceneSource(id); s && obs_source_get_ref(s))
				da_push_back(sources->sources, &s);
	}
	obs_source_t *obs_frontend_get_current_scene(void) override
	{
		obs_source_t *s = app ? engine->sceneSource(app->programSceneId()) : nullptr;
		return s ? obs_source_get_ref(s) : nullptr;
	}
	void obs_frontend_set_current_scene(obs_source_t *scene) override
	{
		if (!app || !scene)
			return;
		const QString id = obs_source_get_name(scene);
		QMetaObject::invokeMethod(app, [a = app, id] { a->editScene(id); a->take(); }, Qt::QueuedConnection);
	}
	obs_source_t *obs_frontend_get_current_preview_scene(void) override
	{
		obs_source_t *s = app ? engine->sceneSource(app->editSceneId()) : nullptr;
		return s ? obs_source_get_ref(s) : nullptr;
	}
	void obs_frontend_set_current_preview_scene(obs_source_t *scene) override
	{
		if (app && scene)
			QMetaObject::invokeMethod(app, [a = app, id = QString(obs_source_get_name(scene))] { a->editScene(id); }, Qt::QueuedConnection);
	}

	// ---- transitions: LOBS has one program transition ----
	void obs_frontend_get_transitions(struct obs_frontend_source_list *) override {}
	obs_source_t *obs_frontend_get_current_transition(void) override { return nullptr; }
	void obs_frontend_set_current_transition(obs_source_t *) override {}
	int obs_frontend_get_transition_duration(void) override { return engine->settings().transition.durationMs; }
	void obs_frontend_set_transition_duration(int) override {}
	void obs_frontend_release_tbar(void) override {}
	int obs_frontend_get_tbar_position(void) override { return 0; }
	void obs_frontend_set_tbar_position(int) override {}

	// ---- scene collections and profiles ----
	void obs_frontend_get_scene_collections(std::vector<std::string> &strings) override { strings.push_back("LOBS"); }
	char *obs_frontend_get_current_scene_collection(void) override { return dup("LOBS"); }
	void obs_frontend_set_current_scene_collection(const char *) override {}
	bool obs_frontend_add_scene_collection(const char *) override { return false; }
	void obs_frontend_get_profiles(std::vector<std::string> &strings) override
	{
		for (const QString &p : app ? app->profiles() : QStringList{"Default"})
			strings.push_back(p.toStdString());
	}
	char *obs_frontend_get_current_profile(void) override { return dup(engine->settings().profile); }
	char *obs_frontend_get_current_profile_path(void) override { return dup(folder()); }
	void obs_frontend_set_current_profile(const char *profile) override
	{
		if (app)
			QMetaObject::invokeMethod(app, [a = app, p = QString(profile)] { a->switchProfile(p); }, Qt::QueuedConnection);
	}
	void obs_frontend_create_profile(const char *) override {}
	void obs_frontend_duplicate_profile(const char *) override {}
	void obs_frontend_delete_profile(const char *) override {}

	// ---- outputs ----
	void later(std::function<void(App *)> f) { if (app) QMetaObject::invokeMethod(app, [a = app, f] { f(a); }, Qt::QueuedConnection); }
	void obs_frontend_streaming_start(void) override { later([](App *a) { if (!a->streaming()) a->toggleStreaming(); }); }
	void obs_frontend_streaming_stop(void) override { later([](App *a) { if (a->streaming()) a->toggleStreaming(); }); }
	bool obs_frontend_streaming_active(void) override { return engine->streaming(); }
	void obs_frontend_recording_start(void) override { later([](App *a) { if (!a->recording()) a->toggleRecording(); }); }
	void obs_frontend_recording_stop(void) override { later([](App *a) { if (a->recording()) a->toggleRecording(); }); }
	bool obs_frontend_recording_active(void) override { return engine->recording(); }
	void obs_frontend_recording_pause(bool pause) override { later([pause](App *a) { if (a->recordingPaused() != pause) a->togglePauseRecording(); }); }
	bool obs_frontend_recording_paused(void) override { return engine->recordingPaused(); }
	bool obs_frontend_recording_split_file(void) override { return false; }
	bool obs_frontend_recording_add_chapter(const char *) override { return false; }
	void obs_frontend_replay_buffer_start(void) override { later([](App *a) { if (!a->replayActive()) a->toggleReplay(); }); }
	void obs_frontend_replay_buffer_save(void) override { later([](App *a) { a->saveReplay(); }); }
	void obs_frontend_replay_buffer_stop(void) override { later([](App *a) { if (a->replayActive()) a->toggleReplay(); }); }
	bool obs_frontend_replay_buffer_active(void) override { return engine->replayActive(); }
	obs_output_t *obs_frontend_get_streaming_output(void) override { return engine->streamOutput() ? obs_output_get_ref(engine->streamOutput()) : nullptr; }
	obs_output_t *obs_frontend_get_recording_output(void) override { return engine->recordOutput() ? obs_output_get_ref(engine->recordOutput()) : nullptr; }
	obs_output_t *obs_frontend_get_replay_buffer_output(void) override { return engine->replayOutput() ? obs_output_get_ref(engine->replayOutput()) : nullptr; }
	obs_output_t *obs_frontend_get_virtualcam_output(void) override { return engine->vcamOutput() ? obs_output_get_ref(engine->vcamOutput()) : nullptr; }
	void obs_frontend_start_virtualcam(void) override { later([](App *a) { if (!a->vcamActive()) a->toggleVirtualCam(); }); }
	void obs_frontend_stop_virtualcam(void) override { later([](App *a) { if (a->vcamActive()) a->toggleVirtualCam(); }); }
	bool obs_frontend_virtualcam_active(void) override { return engine->vcamActive(); }
	obs_service_t *obs_frontend_get_streaming_service(void) override { return engine->streamService(); }
	void obs_frontend_set_streaming_service(obs_service_t *) override {}
	void obs_frontend_save_streaming_service() override {}

	// ---- menus and docks ----
	void *obs_frontend_add_tools_menu_qaction(const char *name) override
	{
		auto *a = new QAction(QString::fromUtf8(name));
		tools.push_back({QString::fromUtf8(name), nullptr, nullptr, a});
		return a;
	}
	void obs_frontend_add_tools_menu_item(const char *name, obs_frontend_cb callback, void *private_data) override
	{
		tools.push_back({QString::fromUtf8(name), callback, private_data, nullptr});
	}
	bool obs_frontend_add_dock_by_id(const char *id, const char *title, void *widget) override
	{
		if (!window() || docks.contains(id) || !widget)
			return false;
		auto *d = new QDockWidget(QString::fromUtf8(title), panels);
		d->setObjectName(QString::fromUtf8(id));
		d->setWidget(static_cast<QWidget *>(widget));
		panels->addDockWidget(Qt::LeftDockWidgetArea, d);
		docks.insert(id, d);
		blog(LOG_INFO, "[lobs] plugin panel: %s (%s)", title, id);
		return true;
	}
	void obs_frontend_remove_dock(const char *id) override
	{
		if (QDockWidget *d = docks.take(id))
			d->deleteLater();
	}
	bool obs_frontend_add_custom_qdock(const char *id, void *dock) override
	{
		if (!window() || docks.contains(id) || !dock)
			return false;
		auto *d = static_cast<QDockWidget *>(dock);
		d->setParent(panels);
		panels->addDockWidget(Qt::LeftDockWidgetArea, d);
		docks.insert(id, d);
		return true;
	}

	// ---- events and saving ----
	void obs_frontend_add_event_callback(obs_frontend_event_cb callback, void *private_data) override { events.push_back({callback, private_data}); }
	void obs_frontend_remove_event_callback(obs_frontend_event_cb callback, void *private_data) override
	{
		std::erase_if(events, [&](const EventCb &c) { return c.cb == callback && c.data == private_data; });
	}
	void obs_frontend_add_save_callback(obs_frontend_save_cb callback, void *private_data) override { saves.push_back({callback, private_data}); }
	void obs_frontend_remove_save_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		std::erase_if(saves, [&](const SaveCb &c) { return c.cb == callback && c.data == private_data; });
	}
	void obs_frontend_add_preload_callback(obs_frontend_save_cb callback, void *private_data) override { preloads.push_back({callback, private_data}); }
	void obs_frontend_remove_preload_callback(obs_frontend_save_cb callback, void *private_data) override
	{
		std::erase_if(preloads, [&](const SaveCb &c) { return c.cb == callback && c.data == private_data; });
	}
	void obs_frontend_save(void) override { saveState(); }
	void obs_frontend_defer_save_begin(void) override {}
	void obs_frontend_defer_save_end(void) override {}
	// Plugins' own state (their save callbacks), in frontend/plugins.json.
	void saveState()
	{
		obs_data_t *d = obs_data_create();
		for (const SaveCb &c : saves)
			c.cb(d, true, c.data);
		obs_data_save_json_safe(d, (folder() + "/plugins.json").toUtf8().constData(), "tmp", "bak");
		obs_data_release(d);
	}
	void loadState()
	{
		obs_data_t *d = obs_data_create_from_json_file_safe((folder() + "/plugins.json").toUtf8().constData(), "bak");
		if (!d)
			d = obs_data_create();
		for (const SaveCb &c : preloads)
			c.cb(d, false, c.data);
		for (const SaveCb &c : saves)
			c.cb(d, false, c.data);
		obs_data_release(d);
	}

	config_t *obs_frontend_get_profile_config(void) override { return config(profileConfig, "basic.ini"); }
	config_t *obs_frontend_get_app_config(void) override { return config(appConfig, "global.ini"); }
	config_t *obs_frontend_get_user_config(void) override { return config(userConfig, "user.ini"); }

	// ---- what OBS's window does that LOBS doesn't: safe answers ----
	void obs_frontend_open_projector(const char *, int, const char *, const char *) override {}
	void obs_frontend_push_ui_translation(obs_frontend_translate_ui_cb) override {}
	void obs_frontend_pop_ui_translation(void) override {}
	bool obs_frontend_preview_program_mode_active(void) override { return false; }
	void obs_frontend_set_preview_program_mode(bool) override {}
	void obs_frontend_preview_program_trigger_transition(void) override {}
	bool obs_frontend_preview_enabled(void) override { return true; }
	void obs_frontend_set_preview_enabled(bool) override {}
	void on_load(obs_data_t *) override {}
	void on_preload(obs_data_t *) override {}
	void on_save(obs_data_t *) override {}
	void on_event(enum obs_frontend_event event) override { send(event); }
	void obs_frontend_take_screenshot() override {}
	void obs_frontend_take_source_screenshot(obs_source_t *) override {}
	void obs_frontend_reset_video(void) override {}
	void obs_frontend_open_source_properties(obs_source_t *) override {}
	void obs_frontend_open_source_filters(obs_source_t *) override {}
	void obs_frontend_open_source_interaction(obs_source_t *) override {}
	void obs_frontend_open_sceneitem_edit_transform(obs_sceneitem_t *) override {}
	char *obs_frontend_get_current_record_output_path(void) override { return dup(engine->settings().recordFolder()); }
	const char *obs_frontend_get_locale_string(const char *string) override { return string; }
	bool obs_frontend_is_theme_dark(void) override { return true; }
	char *obs_frontend_get_last_recording(void) override { return dup(engine->lastRecording()); }
	char *obs_frontend_get_last_screenshot(void) override { return dup(QString()); }
	char *obs_frontend_get_last_replay(void) override { return dup(QString()); }
	void obs_frontend_add_undo_redo_action(const char *, const undo_redo_cb, const undo_redo_cb, const char *, const char *, bool) override {}
	obs_canvas_t *obs_frontend_add_canvas(const char *, obs_video_info *, int) override { return nullptr; }
	bool obs_frontend_remove_canvas(obs_canvas_t *) override { return false; }
	void obs_frontend_get_canvases(obs_frontend_canvas_list *) override {}
	void obs_frontend_copy_sceneitem(obs_sceneitem_t *) override {}
	bool obs_frontend_can_paste_sceneitem(bool) override { return false; }
	void obs_frontend_paste_sceneitem(obs_scene_t *, bool) override {}
};

Frontend *g = nullptr;

} // namespace

void install(Engine *engine)
{
	if (g)
		return;
	using SetCallbacks = void (*)(obs_frontend_callbacks *);
	SetCallbacks set = nullptr;
#ifdef Q_OS_WIN
	// obs-frontend-api.dll sits next to obs.dll in the engine's bin folder. By full path: LOBS
	// narrows where Windows looks for DLLs, so a bare name isn't found.
	wchar_t obsPath[MAX_PATH] = {};
	if (HMODULE obs = GetModuleHandleW(L"obs.dll"))
		GetModuleFileNameW(obs, obsPath, MAX_PATH);
	const QString dllPath = QFileInfo(QString::fromWCharArray(obsPath)).absolutePath() + "/obs-frontend-api.dll";
	HMODULE dll = LoadLibraryW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(dllPath).utf16()));
	if (dll)
		// C++ linkage in OBS's header, so the export has MSVC's decorated name.
		set = reinterpret_cast<SetCallbacks>(GetProcAddress(dll, "?obs_frontend_set_callbacks_internal@@YAXPEAUobs_frontend_callbacks@@@Z"));
	const unsigned long err = GetLastError();
#else
	const unsigned long err = 0;
	const QString dllPath;
#endif
	if (!set) {
		blog(LOG_WARNING, "[lobs] obs-frontend-api not usable (%s, error %lu): plugins that use OBS's interface get no answers", qPrintable(dllPath), err);
		return;
	}
	g = new Frontend;
	g->engine = engine;
	set(g);
}

void setApp(App *app)
{
	if (!g)
		return;
	g->app = app;
	// Events plugins follow, from LOBS's own state.
	QObject::connect(app, &App::docChanged, app, [app, last = QString()]() mutable {
		const QString p = app->programSceneId();
		if (p != last && !last.isEmpty())
			g->send(OBS_FRONTEND_EVENT_SCENE_CHANGED);
		last = p;
	});
	QObject::connect(app, &App::engineChanged, app, [app, s = false, r = false, rp = false, rb = false, vc = false]() mutable {
		auto edge = [](bool now, bool &was, obs_frontend_event on, obs_frontend_event off) { if (now != was) g->send(now ? on : off), was = now; };
		edge(app->streaming(), s, OBS_FRONTEND_EVENT_STREAMING_STARTED, OBS_FRONTEND_EVENT_STREAMING_STOPPED);
		edge(app->recording(), r, OBS_FRONTEND_EVENT_RECORDING_STARTED, OBS_FRONTEND_EVENT_RECORDING_STOPPED);
		edge(app->recordingPaused(), rp, OBS_FRONTEND_EVENT_RECORDING_PAUSED, OBS_FRONTEND_EVENT_RECORDING_UNPAUSED);
		edge(app->replayActive(), rb, OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED, OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED);
		edge(app->vcamActive(), vc, OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED, OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED);
	});
}

void loaded()
{
	if (!g)
		return;
	g->loadState();
	g->send(OBS_FRONTEND_EVENT_FINISHED_LOADING);
}

void exiting()
{
	if (!g)
		return;
	g->saveState();
	g->send(OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN);
	g->send(OBS_FRONTEND_EVENT_EXIT);
	// Hidden, not deleted: plugins delete their own widgets when they unload.
	if (g->panels)
		g->panels->hide();
}

bool hasPanels() { return g && !g->docks.isEmpty(); }

void showPanels()
{
	if (g && g->panels) {
		g->panels->show();
		g->panels->raise();
		g->panels->activateWindow();
	}
}

QStringList toolsItems()
{
	QStringList out;
	if (g)
		for (const auto &t : g->tools)
			out << t.name;
	return out;
}

void triggerTool(const QString &name)
{
	if (!g)
		return;
	for (const auto &t : g->tools)
		if (t.name == name) {
			if (t.cb)
				t.cb(t.data);
			else if (t.action)
				t.action->trigger();
			return;
		}
}

} // namespace lobs::frontend
