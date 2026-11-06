#pragma once

#include <QString>

class QGuiApplication;
class App;
class Engine;
class EngineUpdater;
class Captioner;

int runEngineSmokeTest(QGuiApplication &app, Engine &engine, const QString &docPath, const QString &reportPath);
void attachCliTests(QGuiApplication &app, App &controller, Engine &engine, EngineUpdater &engineUpdater, Captioner &captions, const QString &appOut);
