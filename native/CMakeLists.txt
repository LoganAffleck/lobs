cmake_minimum_required(VERSION 3.21)
project(LOBS VERSION 0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Svg Network)
# KDDockWidgets (QtQuick frontend), built into deps/kddw by deps/build-kddw.bat
find_package(KDDockWidgets-qt6 REQUIRED PATHS ${CMAKE_SOURCE_DIR}/deps/kddw NO_DEFAULT_PATH)
qt_standard_project_setup(REQUIRES 6.5)

# libobs headers (obs-studio 32.2.2) and an import library generated from the bundled obs.dll.
set(LIBOBS_INCLUDE ${CMAKE_SOURCE_DIR}/../obs-plugin/deps/obs-studio/libobs)
set(LIBOBS_IMPLIB ${CMAKE_SOURCE_DIR}/deps/obs.lib)

qt_add_executable(LOBS WIN32
    src/main.cpp
    resources/lobs.rc
    src/app.cpp
    src/app.h
    src/engine.cpp
    src/engine_output.cpp
    src/engine_extras.cpp
    src/hotkeys.cpp
    src/shellmeta.cpp
    src/shellmeta.h
    src/hotkeys.h
    src/settings.cpp
    src/settings.h
    src/icons.h
    src/engine.h
    src/engine_manager.cpp
    src/engine_manager.h
    src/lobs_instance.c
    src/lobs_frame.c
    src/shapes.cpp
    src/eyedropper.cpp
    src/browser_view.cpp
    src/projector.cpp
    src/rows_model.cpp
    src/rows_model.h
    src/projector.h
    src/browser_view.h
    src/eyedropper.h
    src/shapes.h
    src/model/doc.cpp
    src/model/doc.h
    src/model/store.cpp
    src/model/obs_import.cpp
    src/model/store.h
)
qt_add_qml_module(LOBS URI LOBS VERSION 1.0 QML_FILES
    qml/Main.qml
    # Fluid Functionalism components (QML port)
    qml/FButton.qml qml/FInput.qml qml/FNumField.qml qml/FFillSlider.qml qml/FSwitch.qml qml/FCheckbox.qml
    qml/FTabs.qml qml/FMenu.qml qml/FMenuItem.qml qml/FSelect.qml qml/FDialog.qml qml/FluidList.qml
    qml/FSection.qml qml/FBadge.qml qml/FPanelHeader.qml
    qml/SettingsPage.qml qml/MixerView.qml qml/FIcon.qml qml/FHelp.qml qml/FScrollView.qml qml/CanvasArea.qml qml/StatusBar.qml qml/OutputControls.qml qml/AssetMouse.qml qml/MeterFader.qml qml/ToolTipStyle.qml qml/SourceFields.qml qml/SourceSection.qml qml/FFontPicker.qml qml/FColorField.qml qml/FSearchField.qml qml/GraphicTools.qml
        qml/TransitionsDialog.qml
        qml/NewProjectDialog.qml
        qml/SetupWizard.qml
        qml/TwitchPanel.qml
    qml/dock/DockTabBar.qml qml/dock/DockTitleBar.qml qml/dock/DockGroup.qml qml/dock/DockFloatingWindow.qml qml/dock/DockSeparator.qml qml/dock/DockWidgetBg.qml qml/dock/PanelDragController.qml)

target_include_directories(LOBS PRIVATE ${LIBOBS_INCLUDE})
target_compile_definitions(LOBS PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(LOBS PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Svg Qt6::Network KDAB::kddockwidgets ${LIBOBS_IMPLIB} crypt32 version delayimp shell32 ole32 propsys)
# libobs is loaded from the engine folder chosen at startup (<app>/engine/<version>), so it
# must not be resolved before main() runs.
target_link_options(LOBS PRIVATE "/DELAYLOAD:obs.dll")

# App folder (what the installer ships): dist/LOBS/LOBS.exe, engines in dist/LOBS/engine/<version>
set_target_properties(LOBS PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/LOBS)

# QA build (qa/build-qa.bat): adds the --scenariotest runner and goes to dist/LOBS-qa instead,
# so the shipped app never contains it.
option(LOBS_TESTS "Build the QA scenario runner" OFF)
if(LOBS_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    target_sources(LOBS PRIVATE src/test_runner.cpp src/test_runner.h)
    target_compile_definitions(LOBS PRIVATE LOBS_TESTS QT_QML_DEBUG) # QML profiler for perf work (QA build only)
    target_link_libraries(LOBS PRIVATE Qt6::Test psapi)
    set_target_properties(LOBS PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/dist/LOBS-qa)
endif()
