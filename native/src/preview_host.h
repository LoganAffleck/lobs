#pragma once

#include <QObject>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QRect>
#include <QWindow>
#include <kddockwidgets/qtquick/ViewFactory.h>

class App;
class Engine;
class QKeyEvent;
class PreviewWindow;
#ifdef Q_OS_WIN
class PreviewItem;
#endif

bool canvasKey(App *app, QKeyEvent *e);

class ThumbProvider : public QQuickImageProvider {
public:
	explicit ThumbProvider(Engine *e);
	QImage requestImage(const QString &id, QSize *size, const QSize &req) override;

private:
	Engine *m_engine;
};

class AssetThumbProvider : public QQuickImageProvider {
public:
	AssetThumbProvider();
	QImage requestImage(const QString &id, QSize *size, const QSize &req) override;
};

class LobsViewFactory : public KDDockWidgets::QtQuick::ViewFactory {
public:
	QUrl tabbarFilename() const override;
	QUrl titleBarFilename() const override;
	QUrl groupFilename() const override;
	QUrl floatingWindowFilename() const override;
	QUrl separatorFilename() const override;
	QUrl dockwidgetFilename() const override;
};

class PreviewHost : public QObject {
	Q_OBJECT
public:
	PreviewHost(Engine *engine, App *app);
	~PreviewHost() override;

	Q_INVOKABLE void attach(QQuickItem *item);

private:
	void attachNative();
	void sync();

	Engine *m_engine;
	App *m_app;
	QQuickItem *m_item = nullptr;
	PreviewWindow *m_native = nullptr;
#ifdef Q_OS_WIN
	PreviewItem *m_inScene = nullptr;
#else
	QQuickItem *m_inScene = nullptr;
#endif
	QRect m_rect;
	bool m_hidden = false;
};
