// QA scenario runner (built only with -DLOBS_TESTS=ON; never in the shipped app).
// --scenariotest <script.js> <out dir>: runs a JavaScript scenario against the live controller,
// engine and UI, writes <out dir>/results.json after every story, then quits (exit 1 on failures).
#pragma once

#include <QString>

class App;
class Engine;
class QGuiApplication;
class QQmlApplicationEngine;

namespace lobs_test {
// Before Engine/App exist: settings go to an INI file in <out dir>/profile and app data to
// Qt's test-mode folders, so a run never reads or writes the real profile.
void isolateProfile(const QString &outDir);
void startScenario(QGuiApplication &app, App &controller, Engine &engine, QQmlApplicationEngine &qml,
		   const QString &script, const QString &outDir);
} // namespace lobs_test
