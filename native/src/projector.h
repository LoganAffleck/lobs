#pragma once

#include <QWindow>

class Engine;
struct obs_display;
struct obs_source;

// A projector: the program output (what viewers see) or one scene, on its own window or full
// screen on a monitor. Useful as a big clean view of a capture card or camera. Esc closes a
// full-screen projector; double-click toggles full screen.
class Projector : public QWindow {
public:
	// sceneId empty = program output. screen null = a normal window.
	Projector(Engine *engine, const QString &sceneId, const QString &title, QScreen *fullscreenOn);
	~Projector() override;

protected:
	void exposeEvent(QExposeEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	bool event(QEvent *e) override;

private:
	static void draw(void *data, uint32_t cx, uint32_t cy);
	void ensureDisplay();
	Engine *m_engine;
	obs_source *m_scene = nullptr; // held while open, so its inputs stay active
	QString m_sceneId;
	obs_display *m_display = nullptr;
	int m_canvasW = 1920, m_canvasH = 1080;
};
