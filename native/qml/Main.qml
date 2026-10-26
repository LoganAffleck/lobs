import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import com.kdab.dockwidgets 2.0 as KDDW
import LOBS
import LOBS.Models
import Qt.labs.platform as Platform

// LOBS editor shell, built from the Fluid Functionalism component set (F*.qml).
// Surfaces: window s1 → side panels s2 → popovers s4 → dialogs s5. Compact (28px) density.
ApplicationWindow {
    id: win
    width: 1520
    height: 900
    visible: true
    title: "LOBS"
    color: Pal.s1
    // The top bar is the title bar (see window_chrome.cpp): Windows still snaps, resizes and
    // maximizes the window, but draws no title bar of its own. macOS keeps a transparent one
    // over the top bar, with its own window buttons (window_chrome_mac.mm).
    flags: chrome.nativeControls ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
                                 : Qt.Window | Qt.FramelessWindowHint
    readonly property bool maximized: visibility === Window.Maximized
    // Filling the screen (maximized or full screen), nothing sits against its edges: the top bar
    // and the status bar keep this much room from them (a window's own edge does that otherwise).
    readonly property int screenEdgeRoom: maximized || visibility === Window.FullScreen ? 6 : 0
    font.family: Pal.font
    font.pixelSize: Pal.body

    property int sceneView: 0 // 0 list, 1 grid
    onSceneViewChanged: app.setGridVisible(sceneView === 1)
    readonly property bool dialogOpen: setupWizard.visible || crashDialog.visible || updateDialog.visible || newProjectDialog.visible || trDialog.visible || addDialog.visible || renameDialog.visible || settingsPage.visible || saveDialog.visible || openDialog.visible || importDialog.visible
    Shortcut { sequence: "Ctrl+,"; enabled: !chrome.nativeMenuBar; onActivated: settingsPage.show() }
    onDialogOpenChanged: app.setPreviewVisible(!dialogOpen)

    // On macOS the menu bar (below) owns these keys; elsewhere they're in-window shortcuts.
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: [StandardKey.Undo]; onActivated: app.undo() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: [StandardKey.Redo, "Ctrl+Y"]; onActivated: app.redo() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+D"; onActivated: app.duplicateSelected() }
    // F2 renames the selected layer in its row, as File Explorer does with files.
    // Not while Assets has focus: F2 renames a file there.
    Shortcut { sequence: "F2"; enabled: !win.focusInside(assetsGuest); onActivated: { const sel = app.layers.filter(l => l.selected); if (sel.length === 1) layerList.rename(sel[0].id) } }
    function focusInside(item) { for (let f = win.activeFocusItem; f; f = f.parent) if (f === item) return true; return false }
    // Layers clipboard. Text fields keep their own copy and paste (they take these keys first).
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Copy; onActivated: app.copyLayers() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Cut; onActivated: app.cutLayers() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Paste; onActivated: app.pasteLayers(false) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+Shift+V"; onActivated: app.pasteLayers(true) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+G"; onActivated: app.groupSelected() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+Shift+G"; onActivated: app.ungroupSelected() }
    // Layer order, like design tools: Ctrl ] forward, Ctrl [ backward, with Shift to the front/back.
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+]"]; onActivated: app.moveSelected(-1) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+["]; onActivated: app.moveSelected(1) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+Shift+]", "Ctrl+}"]; onActivated: app.moveSelected(-1000) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+Shift+[", "Ctrl+{"]; onActivated: app.moveSelected(1000) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+S"; onActivated: win.save() }
    Shortcut { sequence: "Ctrl+O"; enabled: !app.live && !chrome.nativeMenuBar; onActivated: win.guardUnsaved(() => openDialog.open()) }
    // A Mac keyboard's Delete key is Backspace (text fields still take it first).
    Shortcut { sequences: chrome.nativeMenuBar ? ["Delete", "Backspace"] : ["Delete"]; onActivated: app.deleteSelected() }
    function save() { if (!app.saveProject()) saveDialog.open() } // saves in place; Save As only for an unsaved show
    // Edit menu actions go to the text field being typed in, if any, else to the layers.
    function editAction(textFn, layersFn) {
        const f = win.activeFocusItem
        if (f && typeof f[textFn] === "function" && typeof f.selectAll === "function") f[textFn]()
        else layersFn()
    }

    // macOS menu bar: what the top bar's New / Open / Import / Save and Window menus hold on Windows.
    Loader {
        active: chrome.nativeMenuBar
        sourceComponent: Platform.MenuBar {
            window: win
            Platform.Menu {
                title: "File"
                Platform.MenuItem { text: "New Project…"; shortcut: "Ctrl+N"; onTriggered: newProjectDialog.open() }
                Platform.MenuItem { text: "Open…"; shortcut: "Ctrl+O"; enabled: !app.live; onTriggered: win.guardUnsaved(() => openDialog.open()) }
                Platform.Menu {
                    id: recentMac
                    title: "Open Recent"
                    enabled: !app.live
                    Instantiator {
                        model: app.recentShows
                        delegate: Platform.MenuItem {
                            required property string modelData
                            text: modelData.split("/").pop().replace(/\.lobs\.json$/, "")
                            onTriggered: { const f = modelData; win.guardUnsaved(() => app.open("file://" + f)) }
                        }
                        onObjectAdded: (i, o) => recentMac.insertItem(i, o)
                        onObjectRemoved: (i, o) => recentMac.removeItem(o)
                    }
                    Platform.MenuSeparator {}
                    Platform.MenuItem { text: "Clear Menu"; enabled: app.recentShows.length > 0; onTriggered: app.clearRecentShows() }
                }
                Platform.Menu {
                    id: importMac
                    title: "Import from OBS"
                    enabled: !app.live
                    property var collections: []
                    onAboutToShow: collections = app.obsCollections()
                    Instantiator {
                        model: importMac.collections
                        delegate: Platform.MenuItem {
                            required property var modelData
                            text: modelData.name + "  ·  " + modelData.scenes + " scenes"
                            onTriggered: { const p = modelData.path; win.guardUnsaved(() => app.importObs(p)) }
                        }
                        onObjectAdded: (i, o) => importMac.insertItem(i, o)
                        onObjectRemoved: (i, o) => importMac.removeItem(o)
                    }
                    Platform.MenuItem { text: "No OBS scene collections found"; enabled: false; visible: importMac.collections.length === 0 }
                    Platform.MenuSeparator {}
                    Platform.MenuItem { text: "Choose a File…"; onTriggered: win.guardUnsaved(() => importDialog.open()) }
                }
                Platform.MenuSeparator {}
                Platform.MenuItem { text: "Save"; shortcut: "Ctrl+S"; onTriggered: win.save() }
                Platform.MenuItem { text: "Save As…"; shortcut: "Ctrl+Shift+S"; onTriggered: saveDialog.open() }
                // Moved to the LOBS menu by macOS.
                Platform.MenuItem { text: "About LOBS"; role: Platform.MenuItem.AboutRole; onTriggered: settingsPage.show(6) }
                Platform.MenuItem { text: "Settings…"; shortcut: "Ctrl+,"; role: Platform.MenuItem.PreferencesRole; onTriggered: settingsPage.show() }
            }
            Platform.Menu {
                title: "Edit"
                Platform.MenuItem { text: app.canUndo && app.undoLabel ? "Undo " + app.undoLabel : "Undo"; shortcut: StandardKey.Undo; role: Platform.MenuItem.NoRole; onTriggered: win.editAction("undo", app.undo) }
                Platform.MenuItem { text: "Redo"; shortcut: StandardKey.Redo; role: Platform.MenuItem.NoRole; onTriggered: win.editAction("redo", app.redo) }
                Platform.MenuSeparator {}
                Platform.MenuItem { text: "Cut"; shortcut: StandardKey.Cut; role: Platform.MenuItem.NoRole; onTriggered: win.editAction("cut", app.cutLayers) }
                Platform.MenuItem { text: "Copy"; shortcut: StandardKey.Copy; role: Platform.MenuItem.NoRole; onTriggered: win.editAction("copy", app.copyLayers) }
                Platform.MenuItem { text: "Paste"; shortcut: StandardKey.Paste; role: Platform.MenuItem.NoRole; onTriggered: win.editAction("paste", () => app.pasteLayers(false)) }
                Platform.MenuItem { text: "Paste Linked"; shortcut: "Ctrl+Shift+V"; enabled: app.canPaste; onTriggered: app.pasteLayers(true) }
                Platform.MenuItem { text: "Duplicate"; shortcut: "Ctrl+D"; onTriggered: app.duplicateSelected() }
                Platform.MenuItem { text: "Delete"; onTriggered: app.deleteSelected() }
                Platform.MenuSeparator {}
                Platform.MenuItem { text: "Group"; shortcut: "Ctrl+G"; onTriggered: app.groupSelected() }
                Platform.MenuItem { text: "Ungroup"; shortcut: "Ctrl+Shift+G"; onTriggered: app.ungroupSelected() }
                Platform.MenuSeparator {}
                Platform.MenuItem { text: "Bring to Front"; shortcut: "Ctrl+Shift+]"; onTriggered: app.moveSelected(-1000) }
                Platform.MenuItem { text: "Bring Forward"; shortcut: "Ctrl+]"; onTriggered: app.moveSelected(-1) }
                Platform.MenuItem { text: "Send Backward"; shortcut: "Ctrl+["; onTriggered: app.moveSelected(1) }
                Platform.MenuItem { text: "Send to Back"; shortcut: "Ctrl+Shift+["; onTriggered: app.moveSelected(1000) }
            }
            Platform.Menu {
                id: windowMac
                title: "Window"
                Platform.MenuItem { text: "Minimize"; shortcut: "Ctrl+M"; onTriggered: chrome.minimize() }
                Platform.MenuItem { text: "Zoom"; onTriggered: chrome.toggleMaximize() }
                Platform.MenuItem {
                    text: win.visibility === Window.FullScreen ? "Exit Full Screen" : "Enter Full Screen"
                    shortcut: "Meta+Ctrl+F" // ⌃⌘F (Qt's Ctrl is Command on macOS)
                    onTriggered: win.visibility = win.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen
                }
                Platform.MenuSeparator {}
                Instantiator {
                    model: win.panels
                    delegate: Platform.MenuItem {
                        required property var modelData
                        text: modelData.title
                        checkable: true
                        checked: modelData.isOpen
                        onTriggered: win.togglePanel(modelData)
                    }
                    onObjectAdded: (i, o) => windowMac.insertItem(i + 4, o) // after Minimize, Zoom, Full Screen and the separator
                    onObjectRemoved: (i, o) => windowMac.removeItem(o)
                }
                Platform.MenuSeparator {}
                Platform.MenuItem { text: "Show All Panels"; onTriggered: win.panels.forEach(d => { if (!d.isOpen) d.show() }) }
                Platform.MenuItem { text: "Reset Layout"; onTriggered: layoutSaver.restoreFromFile(win.defaultLayoutFile) }
            }
            Platform.Menu {
                title: "Help"
                Platform.MenuItem { text: "LOBS on GitHub"; onTriggered: Qt.openUrlExternally("https://github.com/LoganAffleck/lobs") }
                Platform.MenuItem { text: "Report a Problem…"; onTriggered: Qt.openUrlExternally("https://github.com/LoganAffleck/lobs/issues/new") }
            }
        }
    }

    FileDialog {
        id: saveDialog; fileMode: FileDialog.SaveFile; nameFilters: ["LOBS show (*.lobs.json)"]; defaultSuffix: "lobs.json"
        onAccepted: {
            app.saveAs(selectedFile)
            const next = win.afterSave
            win.afterSave = null
            if (next && app.projectFile !== "") next() // saved: carry on with Open or Import
        }
        onRejected: win.afterSave = null
    }
    // An untitled show with changes asks to be saved before Open or Import replaces it.
    property var afterSave: null
    function guardUnsaved(next) {
        if (app.unsavedShow) { unsavedDialog.next = next; unsavedDialog.open() }
        else next()
    }
    FileDialog { id: importDialog; fileMode: FileDialog.OpenFile; nameFilters: ["OBS scene collection (*.json)"]; onAccepted: app.importObs(selectedFile.toString().replace(/^file:\/{3}/, "")) }
    FileDialog { id: openDialog; fileMode: FileDialog.OpenFile; nameFilters: ["LOBS show (*.lobs.json *.json)"]; onAccepted: app.open(selectedFile) }

    // The Inspector's row labels (Position, Size, Rotate, Crop, Corners), one width so fields line up.
    component RowLabel: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 52 }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    function openTransitions() { trDialog.open() }
    // Brings the Inspector forward (opened if closed), e.g. after picking a source in the mixer.
    function showInspector() { if (!inspectorDock.isOpen) inspectorDock.show(); inspectorDock.dockWidget.setAsCurrentTab() }
    function togglePanel(d) {
        if (d.isOpen) d.close()
        else if (d === statsDock && historyDock.isOpen) historyDock.addDockWidgetAsTab(statsDock) // docked, next to History
        else if (d === captionsDock && audioDock.isOpen) dockArea.addDockWidget(captionsDock, KDDW.KDDockWidgets.Location_OnRight, audioDock, Qt.size(380, 0)) // beside the mixer
        else d.show()
    }
    TransitionsDialog { id: trDialog }
    NewProjectDialog { id: newProjectDialog; onSaveCurrentAs: saveDialog.open() }
    SetupWizard { id: setupWizard; onAddPanels: (chat, info) => win.openTwitchPanels(chat, info) }
    // Stable models for the big lists: rows are updated in place instead of rebuilt (LOBS-022).
    RowsModel { id: sceneRows; rows: app.scenes }
    function openSetup() { setupWizard.open() }
    // Screenshots: LOBS_DEMO=<name> stages a feature on the sample show.
    Timer {
        interval: 2500
        running: app.envValue("LOBS_DEMO") !== ""
        onTriggered: {
            const d = app.envValue("LOBS_DEMO")
            app.runDemo(d)
            if (d === "transitions") trDialog.open()
            else if (d === "setup") setupWizard.open()
            else if (d === "assets") assetsDock.show()
        }
    }
    // For screenshots of a settings page: LOBS_OPEN_SETTINGS=<section index>.
    Timer { interval: 1500; running: app.envValue("LOBS_OPEN_SETTINGS") !== ""; onTriggered: settingsPage.show(Number(app.envValue("LOBS_OPEN_SETTINGS"))) }
    // The Twitch channel both panels show; setup and either panel can set it.
    property string twitchChannel: app.uiValue("twitch/channel", "")
    function setTwitchChannel(c) { app.setUiValue("twitch/channel", c); twitchChannel = c }
    // Docks the Twitch panels at the right edge, Chat and Stream info as tabs.
    function openTwitchPanels(chat, info) {
        twitchChannel = app.uiValue("twitch/channel", "") // setup just saved it
        if (chat && !chatDock.isOpen) dockArea.addDockWidget(chatDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
        if (info && !infoDock.isOpen) {
            if (chatDock.isOpen) chatDock.addDockWidgetAsTab(infoDock)
            else dockArea.addDockWidget(infoDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
        }
    }
    // First run: the setup assistant, once the layout is up.
    Timer { id: setupTimer; interval: 900; running: !app.uiFlag("setup/done", false) && app.envValue("LOBS_NO_SETUP") === ""; onTriggered: setupWizard.open() }
    // A press on empty space anywhere (panel gaps, around the canvas, blank list area) deselects.
    // It sits under all content, so only presses nothing else takes reach it; it never consumes one.
    background: Rectangle {
        color: Pal.s1
        MouseArea { anchors.fill: parent; onPressed: (m) => { app.clearSelection(); m.accepted = false } }
    }
    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    component VDivider: Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Pal.border }

    footer: StatusBar {}

    // The window's popup layer, for the safety net below (Overlay.overlay only resolves on an Item).
    Item { id: overlayRef; readonly property Item overlay: Overlay.overlay }
    // Safety net for the canvas: if it's stepped aside but nothing is open over it, bring it back.
    Timer {
        interval: 1000; repeat: true; running: true
        property int idleChecks: 0
        onTriggered: {
            if (!app.previewHidden || win.dialogOpen) { idleChecks = 0; return }
            const kids = overlayRef.overlay ? overlayRef.overlay.children : []
            let open = app.visibleMenus > 0
            for (let i = 0; i < kids.length; ++i) if (kids[i].visible && kids[i].opacity > 0) open = true
            idleChecks = open ? 0 : idleChecks + 1
            if (idleChecks >= 2) { idleChecks = 0; app.resetCovering() }
        }
    }

    // Every tooltip: app styling, and kept clear of the canvas preview.
    ToolTipStyle { host: win.contentItem; previewItem: win.previewItem }

    // Dev check (LOBS_MIXER_PREVIEW=1): the mixer at a wide/short and a narrow/tall size.
    Loader {
        active: app.envFlag("LOBS_MIXER_PREVIEW")
        sourceComponent: Window {
            id: mixPrev
            visible: true
            width: 1040; height: 560
            color: Pal.s1
            title: "LOBS mixer preview"
            Timer { interval: 5000; running: true; onTriggered: mixPrev.contentItem.grabToImage((r) => r.saveToFile(app.envValue("LOBS_MIXER_PREVIEW"))) }
            Row {
                anchors.fill: parent; anchors.margins: 10; spacing: 10
                Rectangle { width: 700; height: 230; color: Pal.s2; radius: Pal.radius; MixerView { anchors.fill: parent } }
                Rectangle { width: 310; height: 540; color: Pal.s2; radius: Pal.radius; MixerView { anchors.fill: parent } }
            }
        }
    }

    // ---------------- header ----------------
    header: Rectangle {
        id: topBar
        height: 44 + win.screenEdgeRoom
        color: Pal.s1
        Component.onCompleted: { chrome.captionItem = topBar; chrome.captionHeight = height }
        onHeightChanged: chrome.captionHeight = height
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Pal.border }
        RowLayout {
            anchors.fill: parent
            anchors.topMargin: win.screenEdgeRoom
            anchors.leftMargin: 14 + chrome.leadingInset
            anchors.rightMargin: 10
            spacing: 2
            // The app's mark, name and Settings in one button.
            AbstractButton {
                id: brand
                Accessible.role: Accessible.Button
                Accessible.name: "Settings"
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Layout.leftMargin: -6
                Layout.rightMargin: 8
                implicitHeight: Pal.ctl + 4
                implicitWidth: brandRow.implicitWidth + 16
                onClicked: settingsPage.show()
                ToolTip.visible: hovered
                ToolTip.delay: 600
                ToolTip.text: chrome.keys("Settings  Ctrl ,")
                background: Rectangle {
                    radius: Pal.radius
                    color: "#ffffff"
                    opacity: brand.down ? 0.10 : brand.hovered ? 0.06 : 0
                    Behavior on opacity { NumberAnimation { duration: Pal.fast } }
                }
                contentItem: Item {
                    Row {
                        id: brandRow
                        anchors.centerIn: parent
                        spacing: 7
                        Image {
                            source: "qrc:/qt/qml/LOBS/resources/icon/lobs-64.png"
                            width: 20; height: 20
                            sourceSize: Qt.size(40, 40)
                            smooth: true; mipmap: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text { text: "LOBS"; color: Pal.fg; font.family: Pal.font; font.pixelSize: 15; font.weight: 700; anchors.verticalCenter: parent.verticalCenter }
                        FIcon { name: "settings"; size: 14; color: brand.hovered ? Pal.fg : Pal.muted; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
            }
            // New, Open, Import, Save and Window are in the menu bar on macOS.
            FButton { text: "New"; visible: !chrome.nativeMenuBar; variant: "ghost"; onClicked: newProjectDialog.open(); tip: "Start a new project in its own folder" }
            FButton { text: "Open"; visible: !chrome.nativeMenuBar; variant: "ghost"; enabled: !app.live; tip: app.live ? "Stop streaming and recording to open another show" : chrome.keys("Open a show (Ctrl+O)"); onClicked: win.guardUnsaved(() => openDialog.open()) }
            FButton {
                id: importButton
                visible: !chrome.nativeMenuBar
                text: "Import"
                variant: "ghost"
                enabled: !app.live
                tip: app.live ? "Stop streaming and recording to import a collection" : "Import scenes and sources from OBS"
                onClicked: { importMenu.collections = app.obsCollections(); importMenu.popup(importButton, 0, importButton.height + 4) }
                FMenu {
                    id: importMenu
                    width: 280
                    property var collections: []
                    Instantiator {
                        active: importMenu.populated // built while the menu is open
                        model: importMenu.collections
                        delegate: FMenuItem {
                            required property var modelData
                            text: "OBS: " + modelData.name + "  ·  " + modelData.scenes + " scenes"
                            onTriggered: { const p = modelData.path; win.guardUnsaved(() => app.importObs(p)) }
                        }
                        onObjectAdded: (i, o) => importMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => importMenu.removeItem(o)
                    }
                    FMenuItem { text: "No OBS scene collections found"; enabled: false; visible: importMenu.collections.length === 0; height: visible ? implicitHeight : 0 }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem { text: "Choose a file..."; onTriggered: win.guardUnsaved(() => importDialog.open()) }
                }
            }
            FButton { text: "Save"; visible: !chrome.nativeMenuBar; variant: "ghost"; tip: chrome.keys("Save (Ctrl+S)"); onClicked: win.save() }
            FButton {
                visible: ["available", "downloading", "ready"].indexOf(appUpdater.state) >= 0
                iconName: "refresh-cw"
                text: appUpdater.state === "downloading" ? "LOBS " + appUpdater.latest + " " + Math.round(appUpdater.progress * 100) + "%"
                    : appUpdater.state === "ready" ? "Install LOBS " + appUpdater.latest
                    : "LOBS " + appUpdater.latest + " available"
                variant: "ghost"
                tip: "A new version of LOBS"
                onClicked: updateDialog.open()
            }
            Rectangle { visible: !chrome.nativeMenuBar; width: 1; height: 18; color: Pal.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }
            FButton {
                visible: !chrome.nativeMenuBar
                text: "Window"
                variant: "ghost"
                onClicked: windowMenu.popup(this, 0, height + 4)
                FMenu {
                    id: windowMenu
                    width: 240
                    Instantiator {
                        active: windowMenu.populated // built while the menu is open
                        model: win.panels
                        delegate: FMenuItem {
                            required property var modelData
                            text: modelData.title
                            selected: modelData.isOpen
                            onTriggered: win.togglePanel(modelData)
                        }
                        onObjectAdded: (i, o) => windowMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => windowMenu.removeItem(o)
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem { text: "Show all panels"; onTriggered: win.panels.forEach(d => { if (!d.isOpen) d.show() }) }
                    FMenuItem { text: "Reset layout"; onTriggered: layoutSaver.restoreFromFile(win.defaultLayoutFile) }
                }
            }
            Rectangle { width: 1; height: 18; color: Pal.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }
            FButton { iconName: "undo-2"; variant: "ghost"; enabled: app.canUndo; onClicked: app.undo(); tip: "Undo " + app.undoLabel + chrome.keys("  Ctrl Z") }
            FButton { iconName: "redo-2"; variant: "ghost"; enabled: app.canRedo; onClicked: app.redo(); tip: chrome.keys("Redo  Ctrl Shift Z") }
            Item { Layout.fillWidth: true }
            OutputControls {}
            // Window buttons, in Windows' order.
            Row {
                visible: !chrome.nativeControls
                Layout.leftMargin: 8
                Layout.rightMargin: -10
                Layout.fillHeight: true
                WindowButton { iconName: "minus"; label: "Minimize"; onClicked: chrome.minimize() }
                WindowButton { iconName: win.maximized ? "copy" : "square"; label: win.maximized ? "Restore" : "Maximize"; onClicked: chrome.toggleMaximize() }
                WindowButton { iconName: "x"; label: "Close"; danger: true; onClicked: win.close() }
            }
        }
    }
    component WindowButton: AbstractButton {
        id: wb
        property string iconName
        property string label
        property bool danger: false
        Accessible.role: Accessible.Button
        Accessible.name: label
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        width: 46
        height: parent ? parent.height : 44
        background: Rectangle {
            color: wb.danger && wb.hovered ? "#c42b1c" : "#ffffff"
            opacity: wb.danger && wb.hovered ? (wb.down ? 0.85 : 1) : wb.down ? 0.10 : wb.hovered ? 0.07 : 0
        }
        contentItem: Item {
            FIcon { anchors.centerIn: parent; name: wb.iconName; size: 15; color: wb.danger && wb.hovered ? "#ffffff" : Pal.muted }
        }
    }

    // Dockable panels (KDDockWidgets). The video is a panel too, so rows of panels can sit
    // under or over it. Drag a tab to move or float a panel, drop it on another panel to tab them
    // together; the Window menu reopens closed ones.
    PermissionsBar {
        id: permissionsBar
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6; bottomMargin: 0 }
    }

    KDDW.DockingArea {
        id: dockArea
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; top: permissionsBar.visible ? permissionsBar.bottom : parent.top }
        anchors.margins: 6 // same as the gaps between panels, so the edges match
        uniqueName: "LOBSMain"

        KDDW.DockWidget {
            id: videoDock
            uniqueName: "video"
            title: "Video"
            options: KDDW.KDDockWidgets.DockWidgetOption_NotClosable
            CanvasArea { id: videoGuest; anchors.fill: parent }
        }

        KDDW.DockWidget {
            id: scenesDock
            uniqueName: "scenes"
            title: "Scenes"
            Rectangle {
                id: scenesGuest
                property string editingId: "" // the scene whose name is being typed in its row or tile
                property size kddockwidgets_min_size: Qt.size(240, 160) // panel minimum
                anchors.fill: parent
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    FPanelHeader {
                        leading: [
                            FButton { iconName: "list"; variant: "ghost"; selected: win.sceneView === 0; tip: "List"; onClicked: win.sceneView = 0 },
                            FButton { iconName: "layout-grid"; variant: "ghost"; selected: win.sceneView === 1; tip: "Grid with live thumbnails"; onClicked: win.sceneView = 1 }
                        ]
                        FButton { iconName: "plus"; text: "Scene"; variant: "secondary"; onClicked: app.addScene(); tip: "Add a scene" }
                    }
                    // Right-click on a scene, in the list or the grid.
                    FMenu {
                        id: sceneMenu
                        property var row: ({})
                        function openFor(r) { row = r; popup() }
                        FMenuItem { text: "Show this scene"; onTriggered: { app.editScene(sceneMenu.row.id); app.take() } }
                        FMenuItem { text: "Rename…"; onTriggered: scenesGuest.editingId = sceneMenu.row.id }
                        // Shows this scene inside the one being edited, as a layer (nested scenes).
                        FMenuItem { text: "Show in " + app.editSceneName; enabled: sceneMenu.row.id !== app.editSceneId; onTriggered: app.addSceneSource(sceneMenu.row.id) }
                        FMenuItem { text: "Duplicate with linked layers"; onTriggered: app.duplicateScene(sceneMenu.row.id, true) }
                        FMenuItem { text: "Duplicate"; onTriggered: app.duplicateScene(sceneMenu.row.id, false) }
                        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                        FMenuItem { text: "Delete scene"; enabled: app.scenes.length > 1; onTriggered: app.deleteScene(sceneMenu.row.id) }
                    }
                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: win.sceneView
                        FluidList {
                            id: sceneList
                            model: sceneRows
                            topMargin: 2
                            delegate: Item {
                                required property var modelData
                                required property int index
                                width: ListView.view.width
                                height: Pal.ctl + 2
                                Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1; radius: Pal.radius - 2; color: Pal.selected; visible: modelData.editing }
                                MouseArea {
                                    Accessible.role: Accessible.ListItem
                                    Accessible.name: String(modelData.name || "Scene")
                                    anchors.fill: parent
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: (m) => { app.editScene(modelData.id); if (m.button === Qt.RightButton) sceneMenu.openFor(modelData) }
                                    onDoubleClicked: { app.editScene(modelData.id); app.take() }
                                }
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 12
                                    spacing: 6
                                    Body { visible: scenesGuest.editingId !== modelData.id; text: modelData.name; font.weight: modelData.editing ? 550 : 400; Layout.fillWidth: true }
                                    InlineRename {
                                        active: scenesGuest.editingId === modelData.id
                                        original: modelData.name
                                        Layout.fillWidth: true
                                        onRenamed: (n) => app.renameScene(modelData.id, n)
                                        onFinished: scenesGuest.editingId = ""
                                    }
                                    FBadge { visible: modelData.program; text: (app.streaming || app.recording) ? "On air" : "Showing"; tone: (app.streaming || app.recording) ? "live" : "neutral" }
                                    Caption { text: modelData.count }
                                }
                            }
                        }
                        GridView {
                            id: sceneGrid
                            Accessible.role: Accessible.List
                            Accessible.name: "Scenes"
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            boundsMovement: Flickable.StopAtBounds
                            flickDeceleration: 6000
                            maximumFlickVelocity: 3000
                            model: sceneRows
                            leftMargin: 8; rightMargin: 8; topMargin: 4
                            cellWidth: (width - 16) / 2
                            cellHeight: cellWidth * 9 / 16 + 30
                            delegate: Item {
                                required property var modelData
                                width: sceneGrid.cellWidth
                                height: sceneGrid.cellHeight
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    radius: Pal.radius
                                    color: tileMouse.containsMouse ? Pal.s4 : Pal.s3
                                    border.width: modelData.editing || modelData.program ? 2 : 1
                                    border.color: modelData.program ? Pal.live : modelData.editing ? Pal.focus : Pal.border
                                    Behavior on color { ColorAnimation { duration: Pal.fast } }
                                    Rectangle {
                                        id: thumbBox
                                        x: 3; y: 3; width: parent.width - 6; height: width * 9 / 16
                                        radius: Pal.radius - 3; color: "black"; clip: true
                                        Image {
                                            anchors.fill: parent
                                            cache: false
                                            fillMode: Image.PreserveAspectCrop
                                            sourceSize.width: 320
                                            source: win.sceneView === 1 ? "image://thumbs/" + modelData.id + "?" + app.thumbTick : ""
                                        }
                                    }
                                    RowLayout {
                                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: thumbBox.bottom
                                        anchors.leftMargin: 8; anchors.rightMargin: 6; anchors.topMargin: 4
                                        Body { visible: scenesGuest.editingId !== modelData.id; text: modelData.name; font.weight: modelData.editing ? 550 : 400; Layout.fillWidth: true }
                                        InlineRename {
                                            active: scenesGuest.editingId === modelData.id
                                            original: modelData.name
                                            Layout.fillWidth: true
                                            onRenamed: (n) => app.renameScene(modelData.id, n)
                                            onFinished: scenesGuest.editingId = ""
                                        }
                                        FBadge { visible: modelData.program; text: (app.streaming || app.recording) ? "On air" : "Showing"; tone: (app.streaming || app.recording) ? "live" : "neutral" }
                                    }
                                    MouseArea {
                                        Accessible.role: Accessible.ListItem
                                        Accessible.name: String(modelData.name || "Scene")
                                        id: tileMouse
                                        anchors.fill: parent
                                        z: -1 // under the name editor, which takes its own clicks
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        onClicked: (m) => { app.editScene(modelData.id); if (m.button === Qt.RightButton) sceneMenu.openFor(modelData) }
                                        onDoubleClicked: { app.editScene(modelData.id); app.take() }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        KDDW.DockWidget {
            id: poolDock
            uniqueName: "pool"
            title: "Sources"
            Rectangle {
                id: poolGuest
                property size kddockwidgets_min_size: Qt.size(240, 160) // panel minimum
                property int filter: 0 // 0 all, 1 video, 2 audio
                property bool searching: false
                anchors.fill: parent
                clip: true // never draws past the panel when it's narrow
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    FPanelHeader {
                        leading: [
                            FTabs { model: ["All", "Video", "Audio"]; currentIndex: poolGuest.filter; onActivated: (i) => poolGuest.filter = i; Layout.leftMargin: 6 }
                        ]
                        FButton {
                            iconName: "search"; variant: "ghost"; selected: poolGuest.searching
                            tip: "Search sources"
                            onClicked: { poolGuest.searching = !poolGuest.searching; if (poolGuest.searching) poolSearch.forceActiveFocus(); else poolSearch.text = "" }
                        }
                        FButton { iconName: "plus"; text: poolGuest.width < 330 ? "" : "Source"; variant: "secondary"; onClicked: addDialog.open(); tip: "Add a source" }
                    }
                    FSearchField {
                        id: poolSearch
                        visible: poolGuest.searching
                        Layout.fillWidth: true
                        Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
                        implicitHeight: 26
                        placeholderText: "Search by name or device"
                        onDismissed: { poolGuest.searching = false; text = "" }
                    }
                    FluidList {
                        id: poolList
                        readonly property var levels: app.levels // read once per update, not once per meter (LOBS-025)
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        // Video first, then audio, with a divider between; the filter narrows it.
                        model: RowsModel { rows: poolList.poolRowsFn() } // rows stay alive across changes
                        // app.sourceStatus converts the whole map on every read: read it once, not per row.
                        readonly property var status: app.sourceStatus
                        function poolRowsFn() {
                            const st = poolList.status
                            const rows = app.pool.filter(r => !r.header && poolSearch.matches(r.name, (st[r.id] || {}).detail, r.kindLabel))
                            const video = rows.filter(r => r.video), audio = rows.filter(r => !r.video)
                            if (poolGuest.filter === 1) return video
                            if (poolGuest.filter === 2) return audio
                            return video.length && audio.length ? video.concat([{ divider: true }], audio) : video.concat(audio)
                        }
                        topMargin: 4
                        delegate: Item {
                            id: srow
                            required property var modelData
                            required property int index
                            property bool hoverable: !modelData.divider
                            readonly property var st: modelData.divider ? ({}) : (poolList.status[modelData.id] || ({}))
                            readonly property bool problem: !!st.status
                            readonly property bool inScene: !!modelData.inScene
                            width: ListView.view.width
                            height: modelData.divider ? 13 : 46
                            Rectangle { visible: !!modelData.divider; anchors.centerIn: parent; width: parent.width - 24; height: 1; color: Pal.border }
                            Rectangle {
                                visible: !modelData.divider && (modelData.selected || rowHover.hovered)
                                anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1
                                radius: Pal.radius - 2
                                color: modelData.selected ? Pal.selected : Pal.hover
                            }
                            HoverHandler { id: rowHover; enabled: !modelData.divider }
                            RowLayout {
                                visible: !modelData.divider
                                anchors.fill: parent
                                anchors.leftMargin: 12; anchors.rightMargin: 8
                                spacing: 10
                                // Video: a live thumbnail. Audio: its icon over a mini level meter.
                                Rectangle {
                                    implicitWidth: 52; implicitHeight: 30
                                    radius: 4
                                    color: Pal.s1
                                    border.width: 1; border.color: srow.problem ? "#92400e" : Pal.border
                                    clip: true
                                    Image {
                                        id: thumbImg
                                        // Keep the last frame up while the next loads (no placeholder flash each second).
                                        retainWhileLoading: true
                                        property bool hadFrame: false
                                        onStatusChanged: if (status === Image.Ready) hadFrame = true
                                        onSourceChanged: if (!source.toString()) hadFrame = false
                                        visible: modelData.video === true && hadFrame
                                        anchors.fill: parent; anchors.margins: 1
                                        fillMode: Image.PreserveAspectCrop
                                        cache: false
                                        asynchronous: true
                                        sourceSize.width: 104
                                        source: modelData.video && srow.st.hasSize ? "image://thumbs/src:" + modelData.id + "?" + app.sourceTick : ""
                                        // clip is always rectangular: mask to the frame's rounded corners instead.
                                        layer.enabled: true
                                        layer.effect: MultiEffect { maskEnabled: true; maskSource: thumbMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                                    }
                                    Rectangle { id: thumbMask; anchors.fill: thumbImg; radius: 3; visible: false; layer.enabled: true; layer.smooth: true }
                                    FIcon {
                                        visible: !modelData.video || !thumbImg.hadFrame
                                        anchors.centerIn: parent
                                        anchors.verticalCenterOffset: modelData.video ? 0 : -4
                                        name: srow.problem ? "triangle-alert" : (modelData.icon || "")
                                        size: 14
                                        color: srow.problem ? "#fbbf24" : Pal.muted
                                    }
                                    Row { // audio: tiny stereo meter along the bottom
                                        visible: !modelData.video
                                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4 }
                                        spacing: 2
                                        Repeater {
                                            model: 2
                                            delegate: Rectangle {
                                                required property int index
                                                readonly property real db: (poolList.levels[modelData.id] || [-60, -60])[index]
                                                width: (parent.width - 2) / 2; height: 3; radius: 1.5; color: Pal.s4
                                                Rectangle {
                                                    width: parent.width * Math.max(0, Math.min(1, (parent.db + 60) / 60)); height: parent.height; radius: 1.5
                                                    color: modelData.muted ? Pal.muted : parent.db > -6 ? "#ef4444" : parent.db > -18 ? "#f59e0b" : "#22c55e"
                                                }
                                            }
                                        }
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Body { text: modelData.name || ""; font.weight: 550; Layout.fillWidth: true; color: modelData.muted ? Pal.muted : Pal.fg }
                                    Row {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Rectangle { // problems only; healthy sources show their device
                                            visible: srow.problem
                                            width: probText.implicitWidth + 12; height: 16; radius: 8
                                            color: "#3a2a0a"; border.width: 1; border.color: "#92400e"
                                            Text { id: probText; anchors.centerIn: parent; text: srow.st.statusText || ""; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: 10; font.weight: 600 }
                                        }
                                        Caption {
                                            visible: !srow.problem
                                            width: parent.width
                                            elide: Text.ElideRight
                                            text: srow.st.detail || modelData.kindLabel || ""
                                        }
                                    }
                                }
                                // Audio: mute right on the row.
                                FButton {
                                    visible: !modelData.video
                                    iconName: modelData.muted ? "volume-x" : "volume-2"
                                    iconColor: modelData.muted ? Pal.danger : Pal.muted
                                    variant: "ghost"
                                    tip: modelData.muted ? "Muted. Click to unmute." : "Mute"
                                    onClicked: app.setAudio(modelData.id, "muted", !modelData.muted, true)
                                }
                                // Video: is it in the scene you're editing? Hover to add or remove.
                                AbstractButton {
                                    id: sceneToggle
                                    visible: modelData.video === true
                                    hoverEnabled: true
                                    implicitHeight: Pal.ctl
                                    implicitWidth: toggleRow.implicitWidth + 16
                                    readonly property bool showAction: hovered || rowHover.hovered
                                    onClicked: app.toggleInScene(modelData.id)
                                    background: Rectangle {
                                        radius: Pal.radius
                                        color: sceneToggle.hovered ? (srow.inScene ? Pal.dangerBg : Pal.s6) : sceneToggle.showAction && !srow.inScene ? Pal.s5 : "transparent"
                                        Behavior on color { ColorAnimation { duration: Pal.fast } }
                                    }
                                    contentItem: Item {
                                        Row {
                                            id: toggleRow
                                            anchors.centerIn: parent
                                            spacing: 5
                                            Rectangle {
                                                visible: srow.inScene && !sceneToggle.hovered
                                                width: 6; height: 6; radius: 3; color: "#22c55e"
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            FIcon {
                                                visible: !srow.inScene || sceneToggle.hovered
                                                name: srow.inScene ? "x" : "plus"
                                                size: 12
                                                color: srow.inScene && sceneToggle.hovered ? Pal.danger : Pal.fg
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                            Text {
                                                text: srow.inScene ? (sceneToggle.hovered ? "Remove" : "In scene") : (sceneToggle.showAction ? "Add to scene" : "")
                                                color: srow.inScene && !sceneToggle.hovered ? Pal.muted : srow.inScene ? Pal.danger : Pal.fg
                                                font.family: Pal.font; font.pixelSize: Pal.caption
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }
                                    }
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 700
                                    ToolTip.text: srow.inScene ? "Remove from " + app.editSceneName : "Add to " + app.editSceneName
                                }
                                FButton {
                                    id: moreButton
                                    iconName: "ellipsis"
                                    variant: "ghost"
                                    opacity: rowHover.hovered || poolMenu.visible ? 1 : 0.45
                                    tip: "More"
                                    onClicked: poolMenu.popup(moreButton, 0, moreButton.height + 4)
                                }
                            }
                            // Drag onto the canvas to place it where you drop it; click selects (settings in
                            // the Inspector); double-click adds it to the scene; right-click for more.
                            MouseArea {
                                Accessible.role: Accessible.ListItem
                                Accessible.name: String(modelData.name || "Source")
                                enabled: !modelData.divider
                                anchors.fill: parent
                                anchors.rightMargin: 150 // leave the row's buttons clickable
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                property point start
                                property bool dragging: false
                                function inPreview(p) { return p.x >= 0 && p.y >= 0 && p.x <= win.previewItem.width && p.y <= win.previewItem.height }
                                onPressed: (m) => { start = Qt.point(m.x, m.y); dragging = false }
                                onPositionChanged: (m) => {
                                    if (!(m.buttons & Qt.LeftButton) || !modelData.video) return
                                    if (!dragging && Math.hypot(m.x - start.x, m.y - start.y) > 6) dragging = true
                                    if (!dragging) return
                                    const g = mapToItem(win.contentItem, m.x, m.y)
                                    ghost.show(modelData.icon, modelData.name, g.x, g.y)
                                    const p = mapToItem(win.previewItem, m.x, m.y)
                                    if (inPreview(p)) app.poolDragMove(modelData.id, p.x, p.y)
                                    else app.poolDragCancel()
                                }
                                onReleased: (m) => {
                                    if (!dragging) return
                                    dragging = false
                                    ghost.visible = false
                                    const p = mapToItem(win.previewItem, m.x, m.y)
                                    if (inPreview(p)) app.poolDrop(modelData.id, p.x, p.y)
                                    else app.poolDragCancel()
                                }
                                onClicked: (m) => { app.selectSource(modelData.id); if (m.button === Qt.RightButton) poolMenu.popup() }
                                onDoubleClicked: if (modelData.video) app.placeSource(modelData.id)
                            }
                            FMenu {
                                id: poolMenu
                                FMenuItem {
                                    visible: modelData.video === true
                                    height: visible ? implicitHeight : 0
                                    text: modelData.inScene ? "Remove from " + app.editSceneName : "Add to " + app.editSceneName
                                    onTriggered: app.toggleInScene(modelData.id)
                                }
                                FMenuItem { text: "Settings…"; onTriggered: app.selectSource(modelData.id) }
                                FMenuItem { text: "Rename…"; onTriggered: renameDialog.ask("source", modelData.id, modelData.name) }
                                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                                FMenuItem {
                                    enabled: false
                                    text: !modelData.video ? "Plays in every scene"
                                        : (modelData.scenes || []).length ? "Used in " + modelData.scenes.join(", ") : "Not used in any scene yet"
                                }
                                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                                FMenuItem { // LOBS-018: confirm when the source is in more than one scene
                                    text: "Delete source"
                                    onTriggered: (modelData.scenes || []).length > 1 ? deleteSourceDialog.ask(modelData) : app.deleteSource(modelData.id)
                                }
                            }
                        }
                        Caption {
                            visible: poolList.count === 0
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignHCenter
                            text: poolSearch.query ? "No sources match \u201c" + poolSearch.text + "\u201d." : poolGuest.filter === 2 ? "No audio inputs yet." : poolGuest.filter === 1 ? "No video inputs yet." : "Add each camera, capture or mic once,\nthen drag it onto any scene."
                        }
                    }
                }
            }
        }

        KDDW.DockWidget {
            id: assetsDock
            uniqueName: "assets"
            title: "Assets"
            FocusScope {
                id: assetsGuest
                property size kddockwidgets_min_size: Qt.size(240, 140)
                property int view: app.uiValue("assets/view", "grid") === "list" ? 0 : 1 // 0 list, 1 grid
                property int typeFilter: 0 // 0 all, 1 images, 2 video, 3 audio
                property string sortKey: app.uiValue("assets/sort", "name") // name | modified | bytes
                property bool sortDesc: app.uiFlag("assets/sortDesc", false)
                property var selected: [] // paths
                property string anchorPath: ""
                property bool searching: false
                anchors.fill: parent
                // Searching walks every folder. (app.assets is read first only so results refresh when files change.)
                readonly property var raw: assetSearch.query ? (app.assets, app.searchAssets(assetSearch.query)) : app.assets
                // Folders first, then the filter and sort.
                readonly property var items: {
                    const kinds = [null, "image", "video", "audio"]
                    const want = kinds[typeFilter]
                    const rows = raw.filter(a => !want || a.folder || a.kind === want)
                    const key = sortKey, dir = sortDesc ? -1 : 1
                    const cmp = (a, b) => key === "name" ? dir * a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: "base" })
                                                         : dir * ((a[key] || 0) - (b[key] || 0)) || a.name.localeCompare(b.name)
                    return rows.filter(a => a.folder).sort(cmp).concat(rows.filter(a => !a.folder).sort(cmp))
                }
                function isSel(path) { return selected.indexOf(path) >= 0 }
                function selectItem(a, mods) {
                    forceActiveFocus()
                    if (mods & Qt.ControlModifier) {
                        selected = isSel(a.path) ? selected.filter(x => x !== a.path) : selected.concat([a.path])
                    } else if ((mods & Qt.ShiftModifier) && anchorPath) {
                        const paths = items.map(x => x.path), i = paths.indexOf(anchorPath), j = paths.indexOf(a.path)
                        selected = paths.slice(Math.min(i, j), Math.max(i, j) + 1)
                        return
                    } else if (!isSel(a.path)) {
                        selected = [a.path]
                    }
                    anchorPath = a.path
                }
                function targets(a) { return a && isSel(a.path) ? selected : a ? [a.path] : selected }
                function newFolder() { const f = app.createAssetFolder(); if (f) { selected = [f]; renameDialog.ask("asset", f, "New folder") } }
                function currentFolderPath() { return app.assetPath ? app.assetsFolder + "/" + app.assetPath : app.assetsFolder }
                property string dropFolder: "" // the folder a dragged asset would land in (it lights up)
                function folderAt(gx, gy) { // folder item under a global point (drop target)
                    // A parent folder in the breadcrumb takes a drop too (moving an asset back up).
                    const cp = crumbs.mapFromGlobal(gx, gy)
                    const crumb = crumbs.childAt(cp.x, cp.y)
                    if (crumb && crumb.modelData && !crumb.last) return crumb.modelData.dir
                    if (view === 1) { // grid: folders are the compact row above the tiles
                        const flow = assetGrid.headerItem ? assetGrid.headerItem.children[0] : null
                        if (!flow) return ""
                        const p = flow.mapFromGlobal(gx, gy)
                        const it = flow.childAt(p.x, p.y)
                        return it && it.modelData && it.modelData.folder ? it.modelData.path : ""
                    }
                    const p = assetListView.contentItem.mapFromGlobal(gx, gy)
                    const it = assetListView.itemAt(p.x, p.y)
                    return it && it.modelData && it.modelData.folder ? it.modelData.path : ""
                }
                function duration(ms) { const t = Math.round(ms / 1000); return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0") }
                function detailOf(a) { // "1920×1080 · 2.1 MB" / "0:12 · 1280×720 · 8 MB"
                    if (a.folder) return a.size
                    return [a.durationMs ? duration(a.durationMs) : "", a.dims || "", a.size].filter(t => t).join(" · ")
                }
                function setSort(k) {
                    if (sortKey === k) sortDesc = !sortDesc; else { sortKey = k; sortDesc = k !== "name" } // newest / largest first
                    app.setUiValue("assets/sort", sortKey); app.setUiFlag("assets/sortDesc", sortDesc)
                }
                Connections { target: app; function onAssetsChanged() { assetsGuest.selected = assetsGuest.selected.filter(x => app.assets.some(a => a.path === x)) } }
                // Keyboard, while the panel has focus.
                Keys.onPressed: (e) => {
                    const ctrl = e.modifiers & Qt.ControlModifier
                    if (ctrl && e.key === Qt.Key_C) { app.copyAssets(selected); e.accepted = true }
                    else if (ctrl && e.key === Qt.Key_X) { app.cutAssets(selected); e.accepted = true }
                    else if (ctrl && e.key === Qt.Key_V) { app.pasteAssets(); e.accepted = true }
                    else if (ctrl && e.key === Qt.Key_A) { selected = items.map(x => x.path); e.accepted = true }
                    else if (ctrl && e.key === Qt.Key_N) { newFolder(); e.accepted = true }
                    else if (ctrl && e.key === Qt.Key_F) { searching = true; assetSearch.forceActiveFocus(); e.accepted = true }
                    else if (e.key === Qt.Key_Delete && selected.length) { app.trashAssets(selected); e.accepted = true }
                    else if (e.key === Qt.Key_F2 && selected.length === 1) { renameDialog.ask("asset", selected[0], selected[0].split("/").pop()); e.accepted = true }
                    else if (e.key === Qt.Key_Backspace) { app.assetUp(); e.accepted = true }
                    else if ((e.key === Qt.Key_Return || e.key === Qt.Key_Enter) && selected.length === 1) {
                        const a = items.find(x => x.path === selected[0])
                        if (a) a.folder ? app.openAssetFolder(a.path) : a.placeable ? app.placeAsset(a.path) : undefined
                        e.accepted = true
                    }
                }
                FileDialog { id: assetPicker; fileMode: FileDialog.OpenFiles; title: "Add files to assets"; onAccepted: app.importAssets(selectedFiles) }
                FileDialog {
                    id: relinkPicker
                    property string sourceId
                    title: "Find the missing file"
                    onAccepted: app.relinkAsset(sourceId, String(selectedFile).replace(/^file:\/{3}/, ""))
                }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    // ---- one row: where you are, and actions ----
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        Layout.leftMargin: 6; Layout.rightMargin: 6
                        spacing: 0
                        FButton { visible: app.assetPath !== ""; iconName: "corner-left-up"; variant: "ghost"; tip: "Up one folder  Backspace"; onClicked: app.assetUp() }
                        // The path: earlier folders are links, the current one is plain text. At the top, nothing (the tab says Assets).
                        // It gets whatever width is left and clips from the left, so the current folder stays in view.
                        Item {
                          visible: app.assetPath !== ""
                          Layout.fillWidth: true
                          Layout.minimumWidth: 0
                          implicitHeight: 28
                          clip: true
                          Row {
                            id: crumbs
                            anchors.verticalCenter: parent.verticalCenter
                            x: Math.min(0, parent.width - width)
                        Repeater {
                            model: app.assetPath ? app.assetCrumbs : []
                            delegate: Row {
                                required property var modelData
                                required property int index
                                readonly property bool last: index === app.assetCrumbs.length - 1
                                spacing: 0
                                height: 28 // same for links and the current folder, so they line up
                                FIcon { visible: index > 0; name: "chevron-right"; size: 12; color: Pal.muted; anchors.verticalCenter: parent.verticalCenter }
                                FButton { visible: !parent.last; text: modelData.name; variant: "ghost"; selected: assetsGuest.dropFolder === modelData.dir; implicitHeight: 24; anchors.verticalCenter: parent.verticalCenter; onClicked: app.goToAssetPath(modelData.path) }
                                Text { visible: parent.last; text: modelData.name; leftPadding: 8; rightPadding: 4; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; anchors.verticalCenter: parent.verticalCenter }
                            }
                        }
                          }
                        }
                        // Sort and view, next to the path (nothing to sort in an empty folder).
                        Rectangle { visible: assetsGuest.raw.length > 0 && app.assetPath !== ""; implicitWidth: 1; implicitHeight: 16; color: Pal.border; Layout.leftMargin: 4; Layout.rightMargin: 4 }
                        RowLayout {
                            visible: assetsGuest.raw.length > 0
                            spacing: 0
                            FButton {
                                id: sortButton
                                text: ({ name: "Name", modified: "Date", bytes: "Size" })[assetsGuest.sortKey] + (assetsGuest.sortDesc ? " ↓" : " ↑")
                                variant: "ghost"; tip: "Sort"
                                onClicked: sortMenu.popup(sortButton, 0, sortButton.height + 4)
                                FMenu {
                                    id: sortMenu
                                    FMenuItem { text: "Name"; selected: assetsGuest.sortKey === "name"; onTriggered: assetsGuest.setSort("name") }
                                    FMenuItem { text: "Date modified"; selected: assetsGuest.sortKey === "modified"; onTriggered: assetsGuest.setSort("modified") }
                                    FMenuItem { text: "Size"; selected: assetsGuest.sortKey === "bytes"; onTriggered: assetsGuest.setSort("bytes") }
                                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                                    FMenuItem { text: assetsGuest.sortDesc ? "Ascending" : "Descending"; onTriggered: { assetsGuest.sortDesc = !assetsGuest.sortDesc; app.setUiFlag("assets/sortDesc", assetsGuest.sortDesc) } }
                                }
                            }
                            FButton { iconName: "layout-grid"; variant: "ghost"; selected: assetsGuest.view === 1; tip: "Grid"; onClicked: { assetsGuest.view = 1; app.setUiValue("assets/view", "grid") } }
                            FButton { iconName: "list"; variant: "ghost"; selected: assetsGuest.view === 0; tip: "List"; onClicked: { assetsGuest.view = 0; app.setUiValue("assets/view", "list") } }
                        }
                        Item { visible: app.assetPath === ""; Layout.fillWidth: true }
                        FButton {
                            iconName: "search"; variant: "ghost"; selected: assetsGuest.searching
                            tip: chrome.keys("Search all folders  Ctrl F")
                            onClicked: { assetsGuest.searching = !assetsGuest.searching; if (assetsGuest.searching) assetSearch.forceActiveFocus(); else assetSearch.text = "" }
                        }
                        FButton {
                            id: moreAssets
                            iconName: "ellipsis"; variant: "ghost"; tip: "More"
                            onClicked: bgMenu.popup(moreAssets, 0, moreAssets.height + 4)
                        }
                        FButton { iconName: "plus"; text: assetsGuest.width < 360 ? "" : "Add"; variant: "secondary"; tip: "Add files"; onClicked: assetPicker.open(); Layout.leftMargin: 2 }
                    }
                    FSearchField {
                        id: assetSearch
                        visible: assetsGuest.searching
                        Layout.fillWidth: true
                        Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
                        implicitHeight: 26
                        placeholderText: "Search all folders"
                        onDismissed: { assetsGuest.searching = false; text = ""; assetsGuest.forceActiveFocus() }
                    }
                    // ---- missing files (moved or deleted outside LOBS) ----
                    Rectangle {
                        visible: app.missingAssets.length > 0
                        Layout.fillWidth: true
                        Layout.leftMargin: 8; Layout.rightMargin: 8; Layout.bottomMargin: 6
                        implicitHeight: missRow.implicitHeight + 12
                        radius: Pal.radius
                        color: "#2a2008"; border.width: 1; border.color: "#92400e"
                        RowLayout {
                            id: missRow
                            anchors.fill: parent; anchors.margins: 6; anchors.leftMargin: 10
                            spacing: 6
                            FIcon { name: "triangle-alert"; size: 14; color: "#fbbf24" }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                color: "#fde68a"; font.family: Pal.font; font.pixelSize: Pal.caption
                                text: app.missingAssets.length === 1
                                      ? app.missingAssets[0].name + " is missing (moved or deleted outside LOBS)."
                                      : app.missingAssets.length + " files are missing (moved or deleted outside LOBS)."
                            }
                            FButton { text: "Find it"; variant: "secondary"; tip: "Search assets folders by file name"; onClicked: app.findMissingAssets() }
                            FButton {
                                text: "Choose file…"; variant: "ghost"
                                tip: "Point " + (app.missingAssets[0] ? app.missingAssets[0].name : "it") + " at a file"
                                onClicked: { relinkPicker.sourceId = app.missingAssets[0].sourceId; relinkPicker.open() }
                            }
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        // Empty space: clear the selection; right-click for New folder / Paste.
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (m) => { assetsGuest.forceActiveFocus(); assetsGuest.selected = []; if (m.button === Qt.RightButton) bgMenu.popup() }
                        }
                        // Always-visible drop hint at the end of the list or grid.
                        Component {
                            id: dropHint
                            Item {
                                width: ListView.view ? ListView.view.width : (GridView.view ? GridView.view.width : 200)
                                height: assetsGuest.raw.length ? 44 : 0
                                visible: assetsGuest.raw.length > 0 && !assetSearch.query
                                Rectangle {
                                    anchors.fill: parent; anchors.margins: 4
                                    radius: Pal.radius
                                    // Clicking works like Add, for people used to Drive-style drop zones.
                                    color: hintMouse.pressed ? Pal.s5 : hintMouse.containsMouse ? Pal.s4 : "transparent"
                                    border.width: 1; border.color: hintMouse.containsMouse ? Pal.borderStrong : Pal.border
                                    Behavior on color { ColorAnimation { duration: Pal.fast } }
                                    Text { anchors.centerIn: parent; text: "Drop files here, or click to add them"; color: hintMouse.containsMouse ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                                    MouseArea { id: hintMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: assetPicker.open() }
                                }
                            }
                        }
                        FluidList {
                            id: assetListView
                            visible: assetsGuest.view === 0
                            anchors.fill: parent
                            topMargin: 2
                            model: assetsGuest.items
                            footer: dropHint
                            delegate: Item {
                                required property var modelData
                                width: ListView.view.width
                                height: 40
                                Rectangle {
                                    readonly property bool target: modelData.folder && assetsGuest.dropFolder === modelData.path
                                    visible: assetsGuest.isSel(modelData.path) || target
                                    anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1
                                    radius: Pal.radius - 2; color: target ? Pal.s6 : Pal.selected
                                    border.width: target ? 1 : 0; border.color: Pal.focus
                                }
                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 12
                                    spacing: 10
                                    Rectangle {
                                        implicitWidth: 44; implicitHeight: 28; radius: 4
                                        color: modelData.folder ? "transparent" : Pal.s1
                                        Image {
                                            id: rowThumb
                                            anchors.fill: parent
                                            visible: !!modelData.thumb && status === Image.Ready
                                            source: modelData.thumb || ""
                                            sourceSize.width: 88
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            layer.enabled: true
                                            layer.effect: MultiEffect { maskEnabled: true; maskSource: rowMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                                        }
                                        Rectangle { id: rowMask; anchors.fill: parent; radius: 4; visible: false; layer.enabled: true; layer.smooth: true }
                                        FIcon { visible: !rowThumb.visible; anchors.centerIn: parent; name: modelData.icon; size: modelData.folder ? 18 : 14; color: modelData.folder ? (modelData.color || "#e9b949") : Pal.muted }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0
                                        Body { text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                                        Caption { text: (assetSearch.query ? (modelData.where || "Assets") + " · " : "") + assetsGuest.detailOf(modelData); Layout.fillWidth: true; elide: Text.ElideRight }
                                    }
                                    Rectangle { visible: !modelData.folder && modelData.uses > 0; width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                                }
                                AssetMouse {
                                    asset: modelData; previewItem: win.previewItem; ghostItem: ghost; overlayItem: win.contentItem
                                    folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                                    onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                                    onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                                    onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                                }
                            }
                        }
                        GridView {
                            id: assetGrid
                            Accessible.role: Accessible.List
                            Accessible.name: "Assets"
                            visible: assetsGuest.view === 1
                            anchors.fill: parent
                            anchors.leftMargin: 6; anchors.rightMargin: 6; anchors.topMargin: 2; anchors.bottomMargin: 6
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            readonly property int cols: Math.max(2, Math.floor(width / 120))
                            cellWidth: Math.floor(width / cols)
                            cellHeight: Math.round(cellWidth * 0.62) + 52 // thumbnail + two lines of name
                            model: assetsGuest.items.filter(a => !a.folder) // folders sit above, compact
                            footer: dropHint
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; Accessible.name: "Scroll" }
                            // Folders: short rectangles, one cell wide and a third of a tile tall, so three
                            // stack in the space of one asset. They open, select, and take drops like tiles do.
                            header: Item {
                                width: assetGrid.width
                                readonly property var folders: assetsGuest.items.filter(a => a.folder)
                                height: folders.length ? folderFlow.implicitHeight + 8 : 0
                                Flow {
                                    id: folderFlow
                                    objectName: "assetFolderFlow"
                                    width: parent.width
                                    Repeater {
                                        model: parent.parent.folders
                                        delegate: Item {
                                            required property var modelData
                                            width: assetGrid.cellWidth
                                            height: Math.max(40, Math.round(assetGrid.cellHeight / 3))
                                            Rectangle {
                                                readonly property bool sel: assetsGuest.isSel(modelData.path)
                                                anchors.fill: parent; anchors.margins: 4
                                                radius: Pal.radius
                                                readonly property bool target: assetsGuest.dropFolder === modelData.path
                                                color: target ? Pal.s6 : sel ? Pal.s5 : folderMouse.containsMouse ? Pal.s4 : Pal.s3
                                                border.width: sel || target ? 2 : 1
                                                border.color: sel || target ? Pal.focus : Pal.border
                                                Behavior on color { ColorAnimation { duration: Pal.fast } }
                                                RowLayout {
                                                    anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10
                                                    spacing: 8
                                                    FIcon { name: "folder"; size: 15; color: modelData.color || "#e9b949" }
                                                    Text { text: modelData.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true }
                                                    Rectangle { visible: modelData.uses > 0; width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                                                }
                                                AssetMouse {
                                                    id: folderMouse
                                                    asset: modelData; previewItem: win.previewItem; ghostItem: ghost; overlayItem: win.contentItem
                                                    folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                                                    onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                                                    onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                                                    onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            delegate: Item {
                                required property var modelData
                                width: assetGrid.cellWidth
                                height: assetGrid.cellHeight
                                Rectangle {
                                    id: tile
                                    readonly property bool sel: assetsGuest.isSel(modelData.path)
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    radius: Pal.radius
                                    color: sel ? Pal.s5 : tileMouse.containsMouse ? Pal.s4 : "transparent"
                                    border.width: sel ? 2 : 0
                                    border.color: Pal.focus
                                    Behavior on color { ColorAnimation { duration: Pal.fast } }
                                    Rectangle {
                                        id: thumbBox
                                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
                                        height: Math.round(width * 0.62)
                                        radius: Pal.radius - 2
                                        color: modelData.folder ? "transparent" : Pal.s1
                                        border.width: modelData.folder ? 0 : 1; border.color: Pal.border
                                        Image {
                                            id: tileThumb
                                            visible: !!modelData.thumb && status === Image.Ready
                                            anchors.fill: parent; anchors.margins: 1
                                            source: modelData.thumb || ""
                                            sourceSize.width: 260
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                            layer.enabled: true
                                            layer.effect: MultiEffect { maskEnabled: true; maskSource: tileMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                                        }
                                        Rectangle { id: tileMask; anchors.fill: tileThumb; radius: Pal.radius - 3; visible: false; layer.enabled: true; layer.smooth: true }
                                        FIcon {
                                            visible: !tileThumb.visible
                                            anchors.centerIn: parent
                                            name: modelData.icon
                                            size: modelData.folder ? 38 : 22
                                            color: modelData.folder ? (modelData.color || "#e9b949") : Pal.muted
                                        }
                                        // In a scene: the same green dot as Sources.
                                        Rectangle {
                                            visible: !modelData.folder && modelData.uses > 0
                                            anchors { left: parent.left; top: parent.top; margins: 6 }
                                            width: 9; height: 9; radius: 4.5; color: "#22c55e"
                                            border.width: 2; border.color: "#cc000000"
                                        }
                                        // Type corner: duration for video/audio, the extension otherwise.
                                        Rectangle {
                                            visible: !modelData.folder && (modelData.kind === "video" || modelData.kind === "audio" || modelData.kind === "other")
                                            anchors { right: parent.right; bottom: parent.bottom; margins: 5 }
                                            width: typeRow.implicitWidth + 10; height: 17; radius: 4; color: "#cc000000"
                                            Row {
                                                id: typeRow
                                                anchors.centerIn: parent
                                                spacing: 3
                                                FIcon { visible: modelData.kind === "video"; name: "film"; size: 10; color: Pal.fg; anchors.verticalCenter: parent.verticalCenter }
                                                FIcon { visible: modelData.kind === "audio"; name: "audio-lines"; size: 10; color: Pal.fg; anchors.verticalCenter: parent.verticalCenter }
                                                Text {
                                                    text: modelData.durationMs ? assetsGuest.duration(modelData.durationMs) : (modelData.ext || "")
                                                    color: Pal.fg; font.family: Pal.font; font.pixelSize: 10; font.weight: 600
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }
                                    }
                                    // Name: up to two lines, cut at the end (search shows the folder first).
                                    Text {
                                        anchors { left: parent.left; right: parent.right; top: thumbBox.bottom; topMargin: 5; leftMargin: 6; rightMargin: 6 }
                                        text: (assetSearch.query && modelData.where ? modelData.where.split("/").pop() + " / " : "") + modelData.name
                                        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption
                                        wrapMode: Text.WrapAnywhere
                                        maximumLineCount: 2
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignHCenter
                                        lineHeight: 1.05
                                    }
                                    AssetMouse {
                                        id: tileMouse
                                        asset: modelData; previewItem: win.previewItem; ghostItem: ghost; overlayItem: win.contentItem
                                        folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                                        onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                                        onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                                        onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                                    }
                                }
                            }
                        }
                        Caption {
                            visible: assetsGuest.items.length === 0
                            anchors.centerIn: parent
                            width: parent.width - 40
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            text: assetSearch.query ? "Nothing in assets matches \u201c" + assetSearch.text + "\u201d."
                                : assetsGuest.raw.length ? "No " + ["", "images", "videos", "audio files"][assetsGuest.typeFilter] + " here."
                                : app.assetPath ? chrome.keys("This folder is empty. Drag files here, or paste with Ctrl+V.")
                                : "Drop images, video or audio here."
                        }
                        // Drop from File Explorer: files are copied into the current folder.
                        DropArea {
                            id: assetDrop
                            anchors.fill: parent
                            onEntered: (d) => d.accepted = d.hasUrls
                            onDropped: (d) => { if (d.hasUrls) { app.importAssets(d.urls); d.acceptProposedAction() } }
                        }
                        Rectangle {
                            visible: assetDrop.containsDrag
                            anchors.fill: parent; anchors.margins: 6
                            radius: Pal.radius + 2
                            color: "#1a6b97ff"
                            border.width: 2; border.color: Pal.focus
                            Text { anchors.centerIn: parent; text: "Drop to copy into " + (app.assetPath ? app.assetPath.split("/").pop() : "assets"); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
                        }
                        FMenu {
                            id: assetMenu
                            property var asset: ({})
                            readonly property var sel: assetsGuest.targets(asset)
                            readonly property bool many: sel.length > 1
                            FMenuItem { visible: !!assetMenu.asset.folder && !assetMenu.many; height: visible ? implicitHeight : 0; text: "Open"; onTriggered: app.openAssetFolder(assetMenu.asset.path) }
                            FMenuItem { visible: !assetMenu.asset.folder && !assetMenu.many; height: visible ? implicitHeight : 0; text: "Place on " + app.editSceneName; enabled: !!assetMenu.asset.placeable; onTriggered: app.placeAsset(assetMenu.asset.path) }
                            FMenuItem { visible: !assetMenu.many; height: visible ? implicitHeight : 0; text: "Rename…  F2"; onTriggered: renameDialog.ask("asset", assetMenu.asset.path, assetMenu.asset.name) }
                            // Folder color: a row of swatches (the first is the default yellow).
                            Item {
                                visible: !!assetMenu.asset.folder
                                width: parent ? parent.width : 220
                                height: visible ? 34 : 0
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: 12
                                    spacing: 6
                                    Repeater {
                                        model: ["", "#ef4444", "#f97316", "#22c55e", "#14b8a6", "#3b82f6", "#8b5cf6", "#ec4899", "#a3a3a3"]
                                        delegate: Rectangle {
                                            required property string modelData
                                            readonly property bool current: (assetMenu.asset.color || "") === modelData
                                            width: 18; height: 18; radius: 9
                                            color: modelData || "#e9b949"
                                            border.width: current ? 2 : 0
                                            border.color: Pal.fg
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: { for (const p of assetMenu.sel) app.setFolderColor(p, modelData); assetMenu.close() }
                                            }
                                        }
                                    }
                                }
                            }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { text: (assetMenu.many ? "Copy " + assetMenu.sel.length + " items" : "Copy") + chrome.keys("  Ctrl C"); onTriggered: app.copyAssets(assetMenu.sel) }
                            FMenuItem { text: (assetMenu.many ? "Cut " + assetMenu.sel.length + " items" : "Cut") + chrome.keys("  Ctrl X"); onTriggered: app.cutAssets(assetMenu.sel) }
                            FMenuItem { text: chrome.keys("Paste here  Ctrl V"); enabled: app.clipboardHasFiles(); onTriggered: app.pasteAssets() }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { visible: !assetMenu.many; height: visible ? implicitHeight : 0; text: "Show in File Explorer"; onTriggered: app.revealAsset(String(assetMenu.asset.path || "")) }
                            FMenuItem {
                                text: assetMenu.asset.uses && !assetMenu.many ? "In use: remove its layers first" : "Move to Recycle Bin  Del"
                                enabled: !(assetMenu.asset.uses && !assetMenu.many)
                                onTriggered: app.trashAssets(assetMenu.sel)
                            }
                        }
                        FMenu {
                            id: bgMenu
                            FMenuItem { text: chrome.keys("New folder  Ctrl N"); onTriggered: assetsGuest.newFolder() }
                            FMenuItem { text: chrome.keys("Paste  Ctrl V"); enabled: app.clipboardHasFiles(); onTriggered: app.pasteAssets() }
                            FMenuItem { text: "Add files…"; onTriggered: assetPicker.open() }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { text: "Show folder in File Explorer"; onTriggered: app.revealAsset(assetsGuest.currentFolderPath()) }
                        }
                    }
                }
            }
        }

        KDDW.DockWidget {
            id: layersDock
            uniqueName: "layers"
            title: "Layers"
            Rectangle {
                id: layersGuest
                property size kddockwidgets_min_size: Qt.size(280, 140) // panel minimum
                anchors.fill: parent
                color: "transparent"
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    FPanelHeader {
                        title: ""
                        meta: app.editSceneName
                        FButton { iconName: "group"; variant: "ghost"; onClicked: app.groupSelected(); tip: chrome.keys("Group  Ctrl G") }
                        FButton { iconName: "copy"; variant: "ghost"; onClicked: copyMenu.popup(this, 0, height + 4); tip: "Copy to another scene" } // under the button, not wherever the pointer is
                        FButton { iconName: "trash-2"; variant: "ghost"; onClicked: app.deleteSelected(); tip: "Delete  Del" }
                        FMenu {
                            id: copyMenu
                            Instantiator {
                                active: copyMenu.populated // built while the menu is open
                                model: app.scenes
                                delegate: FMenu {
                                    required property var modelData
                                    title: modelData.name
                                    // Close the menu first: the copy rebuilds these scene menus mid-handler.
                                    FMenuItem { text: "Linked copy"; onTriggered: { const id = modelData.id; copyMenu.close(); app.copySelectedTo(id, true) } }
                                    FMenuItem { text: "Independent copy"; onTriggered: { const id = modelData.id; copyMenu.close(); app.copySelectedTo(id, false) } }
                                }
                                onObjectAdded: (i, o) => copyMenu.insertMenu(i, o)
                                onObjectRemoved: (i, o) => copyMenu.removeMenu(o)
                            }
                        }
                    }
                    // One right-click menu for every layer row.
                        FMenu {
                            id: layerMenu
                            property var row: ({}) // the row it was opened on
                            FMenuItem { text: "Rename…"; onTriggered: layerList.rename(layerMenu.row.id) }
                            FMenuItem { text: chrome.keys("Copy    Ctrl C"); onTriggered: app.copyLayers() }
                            FMenuItem { text: chrome.keys("Cut    Ctrl X"); visible: !layerMenu.row.background; height: visible ? implicitHeight : 0; onTriggered: app.cutLayers() }
                            FMenuItem { text: chrome.keys("Paste    Ctrl V"); enabled: app.canPaste; onTriggered: app.pasteLayers(false) }
                            FMenuItem { text: chrome.keys("Paste linked    Ctrl Shift V"); enabled: app.canPaste; onTriggered: app.pasteLayers(true) }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { text: "Duplicate"; onTriggered: app.duplicateSelected() }
                            FCopyToMenu { onCopied: layerMenu.close() }
                            FMenuItem { text: "Group selection"; onTriggered: app.groupSelected() }
                            FMenuItem { text: "Ungroup"; enabled: layerMenu.row.group; onTriggered: app.ungroupSelected() }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { text: chrome.keys("Bring to front    Ctrl Shift ]"); onTriggered: app.moveSelected(-1000) }
                            FMenuItem { text: chrome.keys("Bring forward    Ctrl ]"); onTriggered: app.moveSelected(-1) }
                            FMenuItem { text: chrome.keys("Send backward    Ctrl ["); onTriggered: app.moveSelected(1) }
                            FMenuItem { text: chrome.keys("Send to back    Ctrl Shift ["); onTriggered: app.moveSelected(1000) }
                            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                            FMenuItem { visible: !layerMenu.row.background; height: visible ? implicitHeight : 0; text: "Delete"; onTriggered: app.deleteSelected() }
                        }
                    FluidList {
                        id: layerList
                        // Switching scenes replaces every row; recycling them instead of building new
                        // ones each time was most of a scene switch on a big show (LOBS-025).
                        reuseItems: true
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: RowsModel { rows: app.layers } // rows stay alive across clicks
                        topMargin: 2
                        // Blank space under the layers deselects (pinned to the view, not the content).
                        MouseArea { parent: layerList; anchors.fill: parent; z: -1; onPressed: (m) => { app.clearSelection(); m.accepted = false } }
                        // Drag and drop: a line shows where the layer lands (above/below a row); a
                        // highlight shows dropping into a group, or, after holding over a layer a
                        // moment, making a new group of the two.
                        property string dragId: ""
                        property string editingId: "" // the layer whose name is being typed in its row
                        function rename(id) { editingId = id; const i = app.layers.findIndex(r => r.id === id); if (i >= 0) positionViewAtIndex(i, ListView.Contain) }
                        property int dropIndex: -1
                        property string dropMode: ""   // above | below | into | group | wait
                        property string waitId: ""
                        function rowAt(yInContent) {
                            const i = indexAt(20, yInContent)
                            if (i >= 0) return i
                            return yInContent > 0 ? app.layers.length - 1 : 0
                        }
                        function dragTo(yInContent) {
                            const L = app.layers, i = rowAt(yInContent)
                            const row = L[i], item = itemAtIndex(i)
                            if (!row || !item) { dropIndex = -1; return }
                            // Not onto itself or its own children.
                            const from = L.findIndex(r => r.id === dragId)
                            let end = from + 1
                            while (from >= 0 && end < L.length && L[end].depth > L[from].depth) end++
                            const inside = from >= 0 && i >= from && i < end
                            if (inside) { dropIndex = -1; dropMode = ""; return }
                            const f = (yInContent - item.y) / item.height
                            dropIndex = i
                            if (row.background) { dropMode = "above"; waitId = "" } // the background stays last
                            else if (f < 0.28) { dropMode = "above"; waitId = "" }
                            else if (f > 0.72) { dropMode = "below"; waitId = "" }
                            else if (row.group) { dropMode = "into"; waitId = "" }
                            else if (waitId !== row.id) { waitId = row.id; dropMode = f < 0.5 ? "above" : "below"; groupTimer.restart() }
                        }
                        function endDrag() {
                            const row = app.layers[dropIndex]
                            if (dragId && row && dropMode) app.dropLayer(dragId, row.id, dropMode)
                            dragId = ""; dropIndex = -1; dropMode = ""; waitId = ""; groupTimer.stop()
                        }
                        Timer { id: groupTimer; interval: 300; onTriggered: if (layerList.dragId && layerList.waitId) layerList.dropMode = "group" }
                        // Where it lands.
                        Rectangle {
                            parent: layerList.contentItem
                            readonly property var it: layerList.dropIndex >= 0 ? layerList.itemAtIndex(layerList.dropIndex) : null
                            readonly property var row: layerList.dropIndex >= 0 ? app.layers[layerList.dropIndex] : null
                            visible: !!it && !!row && (layerList.dropMode === "above" || layerList.dropMode === "below")
                            x: row ? 10 + row.depth * 16 : 0
                            width: layerList.width - x - 10
                            height: 2; radius: 1
                            y: it ? (layerList.dropMode === "above" ? it.y - 1 : it.y + it.height - 1) : 0
                            color: Pal.focus
                            z: 10
                        }
                        Rectangle {
                            parent: layerList.contentItem
                            readonly property var it: layerList.dropIndex >= 0 ? layerList.itemAtIndex(layerList.dropIndex) : null
                            visible: !!it && (layerList.dropMode === "into" || layerList.dropMode === "group")
                            x: 4; width: layerList.width - 8
                            y: it ? it.y + 1 : 0; height: it ? it.height - 2 : 0
                            radius: Pal.radius - 2
                            color: "transparent"
                            border.width: 2; border.color: Pal.focus
                            z: 10
                            Text {
                                anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                                text: layerList.dropMode === "group" ? "New group" : "Move into group"
                                color: Pal.focus; font.family: Pal.font; font.pixelSize: Pal.caption
                            }
                        }
                        delegate: Item {
                            required property var modelData
                            required property int index
                            width: ListView.view.width
                            height: Pal.ctl + 2
                            readonly property bool showTools: layerList.hoverIndex === index || modelData.selected
                            // A recycled row waiting in the pool is hidden and gives up focus.
                            ListView.onPooled: { visible = false; focus = false }
                            ListView.onReused: visible = true
                            Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1; radius: Pal.radius - 2; color: Pal.selected; visible: modelData.selected }
                            opacity: layerList.dragId === modelData.id ? 0.45 : 1
                            MouseArea {
                                Accessible.role: Accessible.ListItem
                                Accessible.name: String(modelData.name || "Layer")
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                property real pressY: 0
                                property bool dragged: false
                                preventStealing: dragged
                                onPressed: (m) => { pressY = m.y; dragged = false }
                                onPositionChanged: (m) => {
                                    if (!(m.buttons & Qt.LeftButton)) return
                                    if (!dragged && Math.abs(m.y - pressY) > 5 && !modelData.background) { dragged = true; layerList.dragId = modelData.id }
                                    if (dragged) layerList.dragTo(mapToItem(layerList.contentItem, m.x, m.y).y)
                                }
                                onReleased: if (dragged) layerList.endDrag()
                                onCanceled: { dragged = false; layerList.dragId = ""; layerList.dropIndex = -1 }
                                onClicked: (m) => {
                                    if (dragged) return
                                    if (m.modifiers & Qt.ShiftModifier) app.selectLayerRange(modelData.id) // everything between
                                    else app.selectLayer(modelData.id, m.modifiers & Qt.ControlModifier)
                                    if (m.button === Qt.RightButton) { layerMenu.row = modelData; layerMenu.popup() }
                                }
                                onDoubleClicked: layerList.rename(modelData.id)
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12 + modelData.depth * 16
                                anchors.rightMargin: 8
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: modelData.color; visible: !modelData.group }
                                // A group opens and closes; closed, its layers are out of the list.
                                FIcon {
                                    visible: modelData.group
                                    name: modelData.collapsed ? "chevron-right" : "chevron-down"
                                    color: chevronMouse.containsMouse ? Pal.fg : Pal.muted
                                    Layout.leftMargin: -6
                                    MouseArea {
                                        id: chevronMouse
                                        anchors.fill: parent; anchors.margins: -4
                                        hoverEnabled: true
                                        Accessible.role: Accessible.Button
                                        Accessible.name: modelData.collapsed ? "Show the group's layers" : "Hide the group's layers"
                                        onClicked: app.setGroupCollapsed(modelData.id, !modelData.collapsed)
                                    }
                                }
                                FIcon { name: modelData.icon; color: modelData.visible ? Pal.fg : Pal.muted }
                                Body {
                                    visible: layerList.editingId !== modelData.id
                                    text: modelData.name + (modelData.collapsed ? "  ·  " + modelData.childCount : "")
                                    color: modelData.visible ? Pal.fg : Pal.muted; font.weight: modelData.selected ? 550 : 400; Layout.fillWidth: true
                                }
                                InlineRename {
                                    active: layerList.editingId === modelData.id
                                    original: modelData.name
                                    Layout.fillWidth: true
                                    onRenamed: (n) => app.renameLayer(modelData.id, n)
                                    onFinished: layerList.editingId = ""
                                }
                                FIcon { visible: modelData.linked; name: "link-2"; color: Pal.link; ToolTip.text: "Linked with another scene" }
                                FBadge { visible: modelData.fx > 0; text: modelData.fx + " fx" }
                                Row {
                                    spacing: 0
                                    opacity: showTools ? 1 : 0
                                    Behavior on opacity { NumberAnimation { duration: Pal.fast } }
                                    FButton { visible: !modelData.background; iconName: "chevron-up"; variant: "ghost"; onClicked: app.moveLayer(modelData.id, -1); tip: "Move up" }
                                    FButton { visible: !modelData.background; iconName: "chevron-down"; variant: "ghost"; onClicked: app.moveLayer(modelData.id, 1); tip: "Move down" }
                                    FButton { iconName: modelData.locked ? "lock" : "lock-open"; variant: "ghost"; onClicked: app.toggleLock(modelData.id); tip: modelData.locked ? "Unlock" : "Lock" }
                                }
                                FButton { visible: modelData.locked && !showTools; iconName: "lock"; variant: "ghost"; onClicked: app.toggleLock(modelData.id); tip: "Unlock" }
                                FButton { iconName: modelData.visible ? "eye" : "eye-off"; variant: "ghost"; onClicked: app.toggleVisible(modelData.id); tip: modelData.visible ? "Hide" : "Show" }
                            }
                        }
                        Caption {
                            visible: app.layers.length === 0
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignHCenter
                            text: "Empty scene.\nDrag a source from Sources onto the canvas."
                        }
                    }
                }
            }
        }

        KDDW.DockWidget {
            id: inspectorDock
            uniqueName: "inspector"
            title: "Inspector"
            Loader { id: inspectorGuest; property size kddockwidgets_min_size: Qt.size(280, 200); anchors.fill: parent; sourceComponent: inspectorView }
        }

        KDDW.DockWidget {
            id: historyDock
            uniqueName: "history"
            title: "History"
            Loader { id: historyGuest; property size kddockwidgets_min_size: Qt.size(240, 120); anchors.fill: parent; sourceComponent: historyView }
        }

        // Twitch panels (closed until setup or the Window menu opens them).
        KDDW.DockWidget {
            id: chatDock
            uniqueName: "twitchChat"
            title: "Chat"
            Loader {
                property size kddockwidgets_min_size: Qt.size(260, 200)
                anchors.fill: parent
                active: chatDock.isOpen || item
                sourceComponent: TwitchPanel { kind: "chat"; live: chatDock.isOpen; channel: win.twitchChannel; onChannelPicked: (c) => win.setTwitchChannel(c) }
            }
        }
        KDDW.DockWidget {
            id: infoDock
            uniqueName: "twitchInfo"
            title: "Stream info"
            Loader {
                property size kddockwidgets_min_size: Qt.size(300, 240)
                anchors.fill: parent
                active: infoDock.isOpen || item
                sourceComponent: TwitchPanel { kind: "info"; live: infoDock.isOpen; channel: win.twitchChannel; onChannelPicked: (c) => win.setTwitchChannel(c) }
            }
        }

        KDDW.DockWidget {
            id: audioDock
            uniqueName: "audio"
            title: "Audio Mixer"
            MixerView { id: audioGuest; property size kddockwidgets_min_size: Qt.size(300, 150); anchors.fill: parent; onInspectRequested: win.showInspector() }
        }

        KDDW.DockWidget {
            id: statsDock
            uniqueName: "stats"
            title: "Stats"
            Item {
                id: statsGuest
                property size kddockwidgets_min_size: Qt.size(280, 200)
                anchors.fill: parent
                clip: true
                // Built the first time the panel opens, not at startup.
                Loader {
                    active: statsDock.isOpen || item
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    sourceComponent: StatsView {}
                }
            }
        }

        KDDW.DockWidget {
            id: captionsDock
            uniqueName: "captions"
            title: "Captions"
            Item {
                id: captionsGuest
                property size kddockwidgets_min_size: Qt.size(300, 220)
                anchors.fill: parent
                Loader { active: captionsDock.isOpen || item; anchors.fill: parent; sourceComponent: CaptionsPanel {} }
            }
        }

        Component.onCompleted: Qt.callLater(buildLayout)
        function buildLayout() {
            addDockWidget(videoDock, KDDW.KDDockWidgets.Location_OnLeft, null)
            addDockWidget(scenesDock, KDDW.KDDockWidgets.Location_OnLeft, null, Qt.size(320, 0))
            addDockWidget(poolDock, KDDW.KDDockWidgets.Location_OnBottom, scenesDock, Qt.size(0, 300))
            addDockWidget(assetsDock, KDDW.KDDockWidgets.Location_OnBottom, poolDock, Qt.size(0, 300))
            addDockWidget(layersDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
            addDockWidget(inspectorDock, KDDW.KDDockWidgets.Location_OnBottom, layersDock, Qt.size(0, 520))
            inspectorDock.addDockWidgetAsTab(historyDock)
            inspectorDock.dockWidget.setAsCurrentTab()
            poolDock.addDockWidgetAsTab(audioDock) // Audio Mixer as a tab next to Sources
            poolDock.dockWidget.setAsCurrentTab()
            // Remember this arrangement for "Reset layout", then bring back the user's own.
            layoutSaver.saveToFile(win.defaultLayoutFile)
            layoutSaver.restoreFromFile(win.layoutFile)
            if (!win.panels.some(d => d.isOpen)) // a broken save shouldn't leave you with no panels
                layoutSaver.restoreFromFile(win.defaultLayoutFile)
            // Layouts saved before the Asset Library existed: dock it under Sources, once, after
            // the restore has settled (docking during the restore leaves it floating).
            if (app.firstTime("assetsDocked"))
                Qt.callLater(() => addDockWidget(assetsDock, KDDW.KDDockWidgets.Location_OnBottom, poolDock, Qt.size(0, 300)))
            win.layoutReady = true
        }
    }
    KDDW.LayoutSaver { id: layoutSaver }
    // v2: the video became a panel; layouts saved before that don't fit, so they're left behind.
    readonly property string layoutFile: app.dataFolder + "/layout-v2.json"
    readonly property string defaultLayoutFile: app.dataFolder + "/layout-default-v2.json"
    // Save the arrangement while running (survives crashes). Not on close: by then the panels
    // have been closed by the shutdown itself, and that would be saved as "all panels closed".
    property bool layoutReady: false
    Timer { interval: 3000; repeat: true; running: win.layoutReady; onTriggered: layoutSaver.saveToFile(win.layoutFile) }
    onClosing: { layoutReady = false; app.closeProjectors() }
    readonly property var panels: [scenesDock, poolDock, assetsDock, layersDock, inspectorDock, audioDock, historyDock, statsDock, captionsDock, chatDock, infoDock]
    property Item previewItem: null

    // Docked panels are moved with LOBS's own drag (keeps their slot until the drop).
    readonly property var panelDrag: panelDragController
    PanelDragController {
        id: panelDragController
        parent: Overlay.overlay
        dockArea: dockArea
        panels: [
            { dock: videoDock, guest: videoGuest, icon: "monitor" },
            { help: ["Click: edit a scene without showing it.", "Show or double-click: switch to the scene.", "Grid: shows live thumbnails of every scene.", "Right-click: rename, duplicate with linked or independent layers, or delete."], dock: scenesDock, guest: scenesGuest, icon: "clapperboard" },
            { help: ["Inputs: added once, shared by every scene.", "Place it: drag onto the canvas or double-click.", "Per scene: size, crop and filters.", "Files are in Assets. Add text and color from Layers."], dock: poolDock, guest: poolGuest, icon: "folder" },
            { help: ["Project files: this project's assets folder.", "Add files: drag in from File Explorer or use + Asset.", "Place it: drag onto the canvas or double-click.", "Right-click: show in File Explorer or move to Recycle Bin."], dock: assetsDock, guest: assetsGuest, icon: "image" },
            { help: ["Order: layers higher in the list draw on top.", "Linked: settings shared with another scene.", chrome.keys("Shortcuts: Ctrl+C / Ctrl+V copy and paste (Ctrl+Shift+V pastes linked), Ctrl+D duplicate, Ctrl+G group, Delete remove."), "Copy to: copy layers to another scene."], dock: layersDock, guest: layersGuest, icon: "layers" },
            { help: ["This scene: layer changes apply here only.", "Linked: shared with another scene. Unlink to separate.", "Numbers: math like 1920/2 works. Up/Down nudge, Shift for 10.", "Source: select one in Sources to edit its input."], dock: inspectorDock, guest: inspectorGuest, icon: "sliders-horizontal" },
            { help: ["Global: Mic and Desktop Audio play in every scene.", "Scene audio: plays only in scenes that contain it.", "Tracks: track 1 goes to the stream; recordings can use tracks 1-6.", "Monitor: hear it in headphones. Monitor only keeps it out of output.", "Fader: set the level. Meter shows peak dB."], dock: audioDock, guest: audioGuest, icon: "audio-lines" },
            { help: [chrome.keys("Undo: Ctrl+Z; redo: Ctrl+Shift+Z or Ctrl+Y."), "Jump: click a step to go back to it."], dock: historyDock, guest: historyGuest, icon: "history" },
            { help: ["Frames missed: the PC couldn't render in time.", "Frames skipped: the encoder fell behind.", "Dropped frames: the network couldn't keep up.", "Orange: more than 1%, worth a look."], dock: statsDock, guest: statsGuest, icon: "gauge" },
            { help: ["Speech to text from one audio source, on this PC.", "Send to stream: viewers turn on CC to see them.", "Language: pick it, or let LOBS detect it.", "Text shows up a few seconds after speech."], dock: captionsDock, guest: captionsGuest, icon: "captions" }
        ]
    }

    // Drag ghost that follows the cursor while dragging a pool source.
    Rectangle {
        id: ghost
        visible: false
        z: 1000
        parent: win.contentItem
        radius: Pal.radius
        color: Pal.s5
        border.width: 1
        border.color: Pal.borderStrong
        height: Pal.ctl
        width: ghostRow.implicitWidth + 20
        function show(icon, t, x, y) { ghostIcon.name = icon; ghostText.text = t; ghost.x = x + 12; ghost.y = y + 8; ghost.visible = true }
        Row { id: ghostRow; anchors.centerIn: parent; spacing: 6
            FIcon { id: ghostIcon; anchors.verticalCenter: parent.verticalCenter }
            Text { id: ghostText; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; anchors.verticalCenter: parent.verticalCenter } }
    }

    // ---------------- inspector ----------------

    Component {
        id: inspectorView
        FScrollView {
            id: sv
            clip: true
            contentWidth: availableWidth
            property var d: app.inspector
            ColumnLayout {
                width: sv.availableWidth
                spacing: 0

                // A drawing tool is picked and nothing is selected: its colors and width for the next
                // thing you draw. (A selected shape shows its own in the layer section instead.)
                ColumnLayout {
                    id: toolDefaults
                    readonly property string t: app.tool
                    readonly property bool usesFill: t === "rect" || t === "ellipse" || t === "laser"
                    readonly property bool usesStroke: t !== "laser"
                    readonly property bool lineTool: t === "arrow" || t === "pencil"
                    readonly property var names: ({ text: "Text", rect: "Rectangle", ellipse: "Ellipse", arrow: "Arrow", pencil: "Pencil", laser: "Laser pointer" })
                    visible: sv.d.mode === "none" && t !== "select"
                    Layout.fillWidth: true
                    Layout.margins: 14
                    spacing: 10
                    Text { text: toolDefaults.names[toolDefaults.t] || ""; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
                    Caption {
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: toolDefaults.t === "laser" ? "A fading trail. Nothing is added to the scene."
                            : toolDefaults.t === "text" ? "Drag up or down on the canvas to set the size, then type. Esc or a click elsewhere finishes."
                                                         : "Used for the next one you draw. Select a shape to change its own."
                    }
                    RowLayout {
                        visible: toolDefaults.t === "text"
                        Layout.fillWidth: true; spacing: 4
                        FFontPicker { Layout.fillWidth: true; currentFont: app.toolFont; onPicked: (f) => app.toolFont = f }
                        FButton { text: "B"; font.bold: true; variant: "ghost"; selected: app.toolBold; tip: "Bold"; implicitWidth: Pal.ctl; onClicked: app.toolBold = !app.toolBold }
                        FButton { text: "I"; font.italic: true; variant: "ghost"; selected: app.toolItalic; tip: "Italic"; implicitWidth: Pal.ctl; onClicked: app.toolItalic = !app.toolItalic }
                    }
                    RowLayout {
                        visible: toolDefaults.usesFill
                        Layout.fillWidth: true; spacing: 6
                        Text { text: toolDefaults.t === "laser" ? "Color" : "Fill"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
                        FColorField { Layout.fillWidth: true; color: app.toolFill; onPicked: (c) => app.toolFill = c }
                        FButton { visible: toolDefaults.t !== "laser"; text: "None"; variant: "ghost"; selected: !app.toolFill; tip: "No fill (outline only)"; onClicked: app.toolFill = "" }
                    }
                    RowLayout {
                        visible: toolDefaults.usesStroke
                        Layout.fillWidth: true; spacing: 6
                        Text { text: toolDefaults.lineTool || toolDefaults.t === "text" ? "Color" : "Stroke"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
                        FColorField { Layout.fillWidth: true; color: app.toolStroke; onPicked: (c) => app.toolStroke = c }
                    }
                    FFillSlider {
                        visible: toolDefaults.t === "laser"
                        Layout.fillWidth: true
                        label: "Trail length"
                        from: 150; to: 2000; decimals: 0
                        value: app.laserTrailMs
                        format: (v) => (v / 1000).toFixed(2) + " s"
                        onReleased: (v) => app.laserTrailMs = Math.round(v)
                    }
                    FFillSlider {
                        visible: toolDefaults.usesStroke && toolDefaults.t !== "text"
                        Layout.fillWidth: true
                        label: toolDefaults.lineTool ? "Thickness" : "Stroke width"
                        from: 0; to: 48; decimals: 0
                        value: app.toolStrokeWidth
                        format: (v) => Math.round(v) + " px"
                        onReleased: (v) => app.toolStrokeWidth = Math.round(v)
                    }
                }

                Caption {
                    visible: sv.d.mode === "none" && app.tool === "select"
                    Layout.fillWidth: true
                    Layout.margins: 24
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "Select a layer or source to edit it."
                }

                ColumnLayout {
                    // Groups and multi-selections get a title; a layer is titled under its source instead.
                    visible: sv.d.mode === "group" || sv.d.mode === "multi" || (sv.d.mode === "layer" && !sv.d.source)
                    Layout.fillWidth: true
                    Layout.margins: 12
                    Layout.topMargin: 4
                    spacing: 2
                    Row {
                        spacing: 8
                        FIcon { visible: sv.d.mode !== "multi"; size: 16; name: sv.d.mode === "group" ? "folder" : (sv.d.sourceIcon || ""); anchors.verticalCenter: parent.verticalCenter }
                        Text { text: sv.d.mode === "multi" ? sv.d.count + " layers" : (sv.d.name || ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550; anchors.verticalCenter: parent.verticalCenter }
                    }
                    // (Which source, where it's used and its size are in the Source section below.)
                }

                SourceSection {
                    visible: sv.d.mode === "layer" && !!sv.d.source
                    src: sv.d.source || ({})
                    topDivider: false
                }

                FSection {
                    visible: sv.d.mode === "layer" || sv.d.mode === "group"
                    title: "Transform"; group: "transform"; linkedWith: sv.d.linkTransform || ""; sections: sv.d.sections || ({})
                    // One labeled row per property, each action beside the value it changes: Center
                    // with position; the aspect lock and Fit / Fill / Original with size; the flips
                    // with rotation. Drag any field sideways to scrub it.
                    ColumnLayout {
                        id: tf
                        readonly property bool isLayer: sv.d.mode === "layer"
                        property bool lockAspect: app.uiFlag("inspector/lockAspect", true)
                        spacing: 6; Layout.fillWidth: true
                        RowLayout {
                            spacing: 6; Layout.fillWidth: true
                            RowLabel { text: "Position" }
                            FNumField { label: "X"; live: true; value: sv.d.x || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("x", v); onScrubbed: (v, f) => app.scrubTransform("x", v, f) }
                            FNumField { label: "Y"; live: true; value: sv.d.y || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("y", v); onScrubbed: (v, f) => app.scrubTransform("y", v, f) }
                            FButton { visible: tf.isLayer; iconName: "crosshair"; variant: "ghost"; tip: "Center on the canvas"; onClicked: app.fit("center") }
                        }
                        RowLayout {
                            spacing: 6; Layout.fillWidth: true
                            RowLabel { text: "Size" }
                            FNumField { label: "W"; live: true; from: 1; value: sv.d.w || 0; enabled: tf.isLayer; Layout.fillWidth: true; onCommit: (v) => app.setSize("w", v, tf.lockAspect); onScrubbed: (v, f) => app.scrubTransform("w", v, f, tf.lockAspect) }
                            FNumField { label: "H"; live: true; from: 1; value: sv.d.h || 0; enabled: tf.isLayer; Layout.fillWidth: true; onCommit: (v) => app.setSize("h", v, tf.lockAspect); onScrubbed: (v, f) => app.scrubTransform("h", v, f, tf.lockAspect) }
                            FButton {
                                visible: tf.isLayer
                                iconName: tf.lockAspect ? "link-2" : "unlink-2"
                                variant: "ghost"; selected: tf.lockAspect
                                tip: tf.lockAspect ? "W and H keep their proportions. Click to change them separately." : "W and H change separately. Click to keep proportions."
                                onClicked: { tf.lockAspect = !tf.lockAspect; app.setUiFlag("inspector/lockAspect", tf.lockAspect) }
                            }
                        }
                        RowLayout {
                            visible: tf.isLayer
                            spacing: 4; Layout.fillWidth: true
                            RowLabel {}
                            FButton { text: "Fit"; variant: "ghost"; onClicked: app.fit("fit"); tip: "Fit inside the canvas, keeping proportions" }
                            FButton { text: "Fill"; variant: "ghost"; onClicked: app.fit("fill"); tip: "Cover the whole canvas, keeping proportions" }
                            FButton { text: "Original"; variant: "ghost"; onClicked: app.fit("aspect"); tip: "Restore the source's own proportions" }
                            Item { Layout.fillWidth: true }
                        }
                        RowLayout {
                            visible: tf.isLayer
                            spacing: 6; Layout.fillWidth: true
                            RowLabel { text: "Rotate" }
                            FNumField { suffix: "°"; live: true; from: -360; to: 360; value: sv.d.rot || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("rot", v); onScrubbed: (v, f) => app.scrubTransform("rot", v, f) }
                            FButton { iconName: "flip-horizontal-2"; variant: "ghost"; selected: !!sv.d.flipH; tip: "Mirror left to right"; onClicked: app.setTransform("flipH", sv.d.flipH ? 0 : 1) }
                            FButton { iconName: "flip-vertical-2"; variant: "ghost"; selected: !!sv.d.flipV; tip: "Mirror top to bottom"; onClicked: app.setTransform("flipV", sv.d.flipV ? 0 : 1) }
                        }
                    }
                }

                FSection {
                    // Crop (pictures: captures, images, video, browser) folded into one summary row,
                    // and rounded corners (any layer).
                    id: cropSec
                    readonly property bool croppable: !!sv.d.croppable
                    readonly property real va: sv.d.visibleAspect || 0
                    readonly property bool cropped: !!(sv.d.cl || sv.d.ct || sv.d.cr || sv.d.cb)
                    property bool open: app.uiFlag("inspector/cropOpen", false)
                    function near(r) { return Math.abs(va - r) < 0.01 }
                    readonly property string summary: !cropped ? "None"
                        : near(16 / 9) ? "16:9" : near(4 / 3) ? "4:3" : near(1) ? "1:1" : near(9 / 16) ? "9:16" : "Custom"
                    visible: sv.d.mode === "layer"
                    title: croppable ? "Crop & corners" : "Corners"; group: "crop"; linkedWith: sv.d.linkCrop || ""; sections: sv.d.sections || ({})
                    RowLayout {
                        visible: cropSec.croppable
                        spacing: 6; Layout.fillWidth: true
                        RowLabel { text: "Crop" }
                        // Cropping on the canvas is the main way in; presets and exact values sit
                        // behind the small button that names the current crop.
                        FButton {
                            Layout.fillWidth: true
                            iconName: "crop"; text: app.cropMode ? "Done" : "Crop"
                            variant: app.cropMode ? "primary" : "secondary"
                            tip: app.cropMode ? "Finish cropping (Enter or Esc)" : "Drag the layer's edges on the canvas to crop"
                            onClicked: app.cropMode = !app.cropMode
                        }
                        FButton {
                            variant: "ghost"
                            text: cropSec.summary
                            iconName: cropSec.open ? "chevron-up" : "chevron-down"
                            tip: cropSec.open ? "Hide crop presets and values" : "Crop presets and exact values"
                            onClicked: { cropSec.open = !cropSec.open; app.setUiFlag("inspector/cropOpen", cropSec.open) }
                        }
                    }
                    // Quick crops, centered on the source; the layer keeps its scale.
                    RowLayout {
                        visible: cropSec.croppable && cropSec.open
                        Layout.fillWidth: true; spacing: 4
                        RowLabel {}
                        FButton { text: "None"; variant: "ghost"; selected: !cropSec.cropped; tip: "No crop"; onClicked: app.cropToAspect(0) }
                        FButton { text: "16:9"; variant: "ghost"; selected: cropSec.near(16 / 9) && cropSec.cropped; onClicked: app.cropToAspect(16 / 9) }
                        FButton { text: "4:3"; variant: "ghost"; selected: cropSec.near(4 / 3); onClicked: app.cropToAspect(4 / 3) }
                        FButton { text: "1:1"; variant: "ghost"; selected: cropSec.near(1); tip: "Square"; onClicked: app.cropToAspect(1) }
                        FButton { text: "9:16"; variant: "ghost"; selected: cropSec.near(9 / 16); tip: "Vertical"; onClicked: app.cropToAspect(9 / 16) }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        visible: cropSec.croppable && cropSec.open
                        spacing: 6; Layout.fillWidth: true
                        RowLabel {}
                        FNumField { label: "L"; live: true; from: 0; value: sv.d.cl || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("l", v); onScrubbed: (v, f) => app.scrubCrop("l", Math.round(v), f) }
                        FNumField { label: "T"; live: true; from: 0; value: sv.d.ct || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("t", v); onScrubbed: (v, f) => app.scrubCrop("t", Math.round(v), f) }
                        FNumField { label: "R"; live: true; from: 0; value: sv.d.cr || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("r", v); onScrubbed: (v, f) => app.scrubCrop("r", Math.round(v), f) }
                        FNumField { label: "B"; live: true; from: 0; value: sv.d.cb || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("b", v); onScrubbed: (v, f) => app.scrubCrop("b", Math.round(v), f) }
                    }
                    // Rounded corners: 50% of the short side makes a square a circle; the slider's
                    // ends are square and round.
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        RowLabel { text: "Corners" }
                        FFillSlider {
                            Layout.fillWidth: true
                            label: ""
                            from: 0; to: 50; decimals: 0
                            value: sv.d.radius || 0
                            format: (v) => v <= 0.5 ? "Square" : v >= 49.5 ? "Round" : Math.round(v) + "%"
                            onMoved: (v) => app.setCornerRadius(v, false)
                            onReleased: (v) => app.setCornerRadius(v, true)
                        }
                    }
                }

                FSection {
                    visible: sv.d.mode === "layer"
                    title: "Filters"; group: "filters"; linkedWith: sv.d.linkFilters || ""; sections: sv.d.sections || ({})
                    Repeater {
                        id: filterCards
                        // Keyed by position, not by the filter objects: values change on every slider
                        // move, and a new model array would recreate the slider mid-drag.
                        // Cards are kept once built and hidden when a layer has fewer filters:
                        // building and dropping them on every click kept memory each time (LOBS-039).
                        readonly property int shown: (sv.d.filters || []).length
                        property int built: 0
                        onShownChanged: built = Math.max(built, shown)
                        Component.onCompleted: built = shown
                        model: built
                        delegate: Rectangle {
                            id: card
                            required property int index
                            // A hidden card keeps its last filter, so its controls stay built.
                            property var modelData: ({ enabled: false, label: "", params: [] })
                            readonly property var live: (sv.d.filters || [])[index]
                            onLiveChanged: if (live) modelData = live
                            Component.onCompleted: if (live) modelData = live
                            visible: index < filterCards.shown
                            Layout.fillWidth: true
                            implicitHeight: fcol.implicitHeight + 16
                            radius: Pal.radius + 2
                            color: Pal.s3
                            border.width: 1
                            border.color: Pal.border
                            ColumnLayout {
                                id: fcol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6
                                RowLayout {
                                    Layout.fillWidth: true
                                    FSwitch { checked: card.modelData.enabled; onClicked: app.toggleFilter(card.index) }
                                    Body { text: card.modelData.label; font.weight: 550; Layout.fillWidth: true; Layout.leftMargin: 4 }
                                    FButton { iconName: "trash-2"; variant: "ghost"; onClicked: app.removeFilter(card.index); tip: "Remove filter" }
                                }
                                Repeater {
                                    id: paramRows
                                    // Kept like the cards: a filter with fewer settings hides the rest.
                                    readonly property int shown: card.modelData.params.length
                                    property int built: 0
                                    onShownChanged: built = Math.max(built, shown)
                                    Component.onCompleted: built = shown
                                    model: built
                                    // One control per setting, by kind (see native/src/model/video_fx.h).
                                    delegate: Loader {
                                        required property int index
                                        Layout.fillWidth: true
                                        property var p: ({ key: "", kind: "slider", value: 0, visible: false })
                                        readonly property var live: card.modelData.params[index]
                                        onLiveChanged: if (live) p = live
                                        Component.onCompleted: if (live) p = live
                                        visible: index < paramRows.shown && p.visible
                                        active: p.visible
                                        sourceComponent: p.kind === "color" ? colorParam : p.kind === "choice" ? choiceParam : p.kind === "file" ? fileParam
                                                       : p.kind === "toggle" ? toggleParam : numParam
                                        Component {
                                            id: numParam
                                            FFillSlider {
                                                label: p.label
                                                from: p.min; to: p.max; decimals: p.decimals; displayScale: p.scale; suffix: p.suffix
                                                value: Number(p.value)
                                                onMoved: (v) => app.setFilterValue(card.index, p.key, v, false)
                                                onReleased: (v) => app.setFilterValue(card.index, p.key, v, true)
                                            }
                                        }
                                        Component {
                                            id: colorParam
                                            RowLayout {
                                                spacing: 6
                                                Text { text: p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.fillWidth: true }
                                                FColorField { color: String(p.value); onPicked: (c) => app.setFilterValue(card.index, p.key, c, true) }
                                            }
                                        }
                                        Component {
                                            id: choiceParam
                                            RowLayout {
                                                spacing: 6
                                                Text { text: p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 90 }
                                                FSelect {
                                                    Layout.fillWidth: true
                                                    options: p.options
                                                    currentValue: String(p.value)
                                                    placeholder: String(p.value) // a value from an OBS import that isn't in the list
                                                    onPicked: (v) => app.setFilterValue(card.index, p.key, v, true)
                                                }
                                            }
                                        }
                                        Component {
                                            id: fileParam
                                            RowLayout {
                                                spacing: 6
                                                FInput {
                                                    Layout.fillWidth: true
                                                    placeholderText: p.label
                                                    text: String(p.value)
                                                    onEditingFinished: if (text !== String(p.value)) app.setFilterValue(card.index, p.key, text, true)
                                                }
                                                FButton {
                                                    iconName: "folder-open"; variant: "ghost"; tip: "Choose " + p.label.toLowerCase()
                                                    onClicked: filterFilePicker.pick(card.index, p.key, p.label, p.fileFilter)
                                                }
                                            }
                                        }
                                        Component {
                                            id: toggleParam
                                            FSwitch { text: p.label; checkable: false; checked: !!p.value; onClicked: app.setFilterValue(card.index, p.key, !p.value, true) }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    FSelect {
                        Layout.fillWidth: true
                        placeholder: "Add filter…"
                        options: app.filterTypes()
                        onPicked: (v) => app.addFilter(v)
                    }
                    FileDialog {
                        id: filterFilePicker
                        property int filterIndex: -1
                        property string key
                        function pick(index, k, label, filter) {
                            filterIndex = index; key = k; title = label
                            nameFilters = [filter, "All files (*)"]
                            open()
                        }
                        onAccepted: app.setFilterValue(filterIndex, key, decodeURIComponent(selectedFile.toString().replace(/^file:\/{3}/, "")), true)
                    }
                }

                FSection {
                    visible: sv.d.mode === "layer" || sv.d.mode === "group" || sv.d.mode === "multi"
                    title: "Visibility"; group: "visibility"; linkedWith: sv.d.linkVisibility || ""; sections: sv.d.sections || ({})
                    FSwitch { text: "Visible"; checked: sv.d.visible !== false; onClicked: app.setVisibleFlag(checked) }
                    FFillSlider {
                        Layout.fillWidth: true
                        label: "Opacity"
                        value: sv.d.opacity !== undefined ? sv.d.opacity : 1
                        decimals: 0; displayScale: 100; suffix: "%"
                        onMoved: (v) => app.setOpacity(v, false)
                        onReleased: (v) => app.setOpacity(v, true)
                    }
                }

                ColumnLayout {
                    visible: sv.d.mode === "source"
                    Layout.fillWidth: true
                    Layout.margins: 12
                    Layout.topMargin: 4
                    spacing: 8
                    Row { spacing: 8
                        FIcon { size: 16; name: sv.d.icon || ""; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: sv.d.name || ""; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550; anchors.verticalCenter: parent.verticalCenter } }
                    Caption { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: (sv.d.kindLabel || "") + " · input settings apply to every scene that uses it. Size, crop and filters are set per layer." }
                    SourceFields { d: sv.d; active: sv.d.mode === "source" }
                    // Sources with sound: what the mixer's "..." used to open, here instead.
                    AudioSettings {
                        readonly property var entry: sv.d.mode === "source" ? app.mixer.find(m => m.id === sv.d.id) : undefined
                        visible: !!entry
                        s: entry || ({ id: "", name: "", icon: "", volume: 0, muted: false, monitor: "off", tracks: [], global: true, warnings: [], fx: [], mono: false, balance: 0.5, syncMs: 0 })
                        Layout.fillWidth: true; Layout.topMargin: 8
                    }
                    Text { text: "Used in"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.topMargin: 8 }
                    Repeater {
                        model: sv.d.usedIn || []
                        delegate: FButton { required property var modelData; text: modelData.scene; variant: "ghost"; onClicked: { app.editScene(modelData.sceneId); app.selectLayer(modelData.instId, false) } }
                    }
                    Caption { visible: (sv.d.usedIn || []).length === 0; text: sv.d.video ? "Not on any scene yet — drag it onto the canvas." : "Audio source — always mixed in."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
                Item { Layout.fillHeight: true }
            }
        }
    }

    Component { id: mixerView; MixerView {} }

    Component {
        id: historyView
        FluidList {
            model: RowsModel { rows: app.history } // rows stay alive across edits
            topMargin: 2
            delegate: Item {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: Pal.ctl + 2
                Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1; radius: Pal.radius - 2; color: Pal.selected; visible: index === app.historyIndex }
                MouseArea { Accessible.role: Accessible.ListItem; Accessible.name: String(modelData.label || modelData.name || "History step"); anchors.fill: parent; onClicked: app.jumpHistory(index) }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    Body { text: modelData.label; color: index > app.historyIndex ? Pal.muted : Pal.fg; font.weight: index === app.historyIndex ? 550 : 400; Layout.fillWidth: true }
                    Caption { text: modelData.time }
                }
            }
            Component.onCompleted: positionViewAtEnd()
        }
    }

    // ---------------- dialogs ----------------
    FDialog {
        id: addDialog
        title: "Add a source"
        width: 560
        property string kind: "camera"
        // Camera, display and window are picked here, before the source exists (a window has no
        // sensible default, so Add waits for one).
        readonly property bool picksDevice: ["camera", "display", "window"].indexOf(kind) >= 0
        property var devices: []
        property string device: ""
        property string deviceName: ""
        function refreshDevices() {
            devices = picksDevice ? app.devicesForKind(kind) : []
            const first = devices.find(d => d.name.toLowerCase().indexOf("virtual") < 0)
            device = kind !== "window" && first ? first.value : ""
            deviceName = kind !== "window" && first ? first.name : ""
        }
        onKindChanged: refreshDevices()
        onOpened: { refreshDevices(); nameField.forceActiveFocus() }
        readonly property bool ready: kind !== "window" || device !== ""
        function create() {
            if (!ready)
                return
            // A window source is named after its window, unless you named it.
            const name = nameField.text || (kind === "window" ? deviceName.slice(0, 48) : "")
            app.addSource(kind, name, picksDevice ? device : extraField.text, placeBox.checked)
            nameField.text = ""; extraField.text = ""; close()
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Caption { text: "Each input is added once and shared by every scene."; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            GridLayout {
                columns: 5; columnSpacing: 6; rowSpacing: 6
                Repeater {
                    model: [["camera", "video", "Camera"], ["display", "monitor", "Display"], ["window", "app-window", "Window"], ["game", "gamepad-2", "Game"],
                            ["browser", "globe", "Browser"], ["mic", "mic", "Mic"], ["desktop_audio", "volume-2", "Desktop"]]
                           .filter(k => app.kindAvailable(k[0])) // no game capture on macOS
                    delegate: Rectangle {
                        required property var modelData
                        readonly property bool on: addDialog.kind === modelData[0]
                        width: 96; height: 62; radius: Pal.radius + 2
                        color: on ? Pal.s7 : kindMouse.containsMouse ? Pal.s6 : Pal.s4
                        border.width: 1
                        border.color: on ? Pal.borderStrong : Pal.border
                        Behavior on color { ColorAnimation { duration: Pal.fast } }
                        Column {
                            anchors.centerIn: parent; spacing: 4
                            FIcon { name: modelData[1]; size: 18; color: on ? Pal.fg : Pal.muted; anchors.horizontalCenter: parent.horizontalCenter }
                            Text { text: modelData[2]; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: on ? 550 : 400; anchors.horizontalCenter: parent.horizontalCenter }
                        }
                        MouseArea { Accessible.role: Accessible.Button; Accessible.name: String(modelData[2]); id: kindMouse; anchors.fill: parent; hoverEnabled: true; onClicked: addDialog.kind = modelData[0] }
                    }
                }
            }
            FSelect {
                visible: addDialog.kind === "camera" || addDialog.kind === "display"
                Layout.fillWidth: true
                placeholder: "No " + (addDialog.kind === "camera" ? "camera" : "display") + " found"
                options: addDialog.devices
                currentValue: addDialog.device
                onPicked: (v) => addDialog.device = v
            }
            ColumnLayout {
                visible: addDialog.kind === "window"
                Layout.fillWidth: true
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Caption { text: "Choose the window to capture"; Layout.fillWidth: true }
                    FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "List the open windows again"; onClicked: addDialog.refreshDevices() }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    radius: Pal.radius
                    color: Pal.s3
                    border.color: Pal.border
                    Text {
                        visible: addDialog.devices.length === 0
                        anchors.centerIn: parent
                        width: parent.width - 32
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: permissions.missing.some(m => m.id === "screen") ? "Allow Screen Recording to list your windows." : "No windows to capture."
                        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                    }
                    ListView {
                        id: windowList
                        anchors.fill: parent
                        anchors.margins: 4
                        clip: true
                        model: addDialog.devices
                        delegate: Rectangle {
                            required property var modelData
                            readonly property bool chosen: addDialog.device === modelData.value
                            width: windowList.width
                            height: 30
                            radius: Pal.radius - 2
                            color: chosen ? Pal.selected : winMouse.containsMouse ? Pal.hover : "transparent"
                            Text {
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 10 }
                                text: modelData.name
                                elide: Text.ElideRight
                                color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: chosen ? 550 : 400
                            }
                            MouseArea {
                                id: winMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                Accessible.role: Accessible.Button
                                Accessible.name: String(modelData.name)
                                onClicked: { addDialog.device = modelData.value; addDialog.deviceName = modelData.name }
                                onDoubleClicked: { addDialog.device = modelData.value; addDialog.deviceName = modelData.name; addDialog.create() }
                            }
                        }
                    }
                }
            }
            FInput { id: nameField; Layout.fillWidth: true; placeholderText: "Name (optional)"; onAccepted: addDialog.create() }
            FInput {
                id: extraField
                Layout.fillWidth: true
                visible: ["text", "color", "image", "media", "browser"].indexOf(addDialog.kind) >= 0
                placeholderText: addDialog.kind === "text" ? "Text" : addDialog.kind === "color" ? "#1d1b2e" : addDialog.kind === "browser" ? "URL, e.g. your alert box link" : "File path"
                onAccepted: addDialog.create()
            }
            FCheckbox { id: placeBox; checked: true; text: "Also place on " + app.editSceneName; visible: ["mic", "desktop_audio"].indexOf(addDialog.kind) < 0 }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Item { Layout.fillWidth: true }
                FButton { text: "Cancel"; variant: "ghost"; onClicked: addDialog.close() }
                FButton { text: "Add source"; variant: "primary"; enabled: addDialog.ready; onClicked: addDialog.create() }
            }
        }
    }

    SettingsPage { id: settingsPage; parent: Overlay.overlay; anchors.fill: parent; z: 500; onUpdateRequested: { open = false; updateDialog.open() } }

    FDialog {
        id: renameDialog
        property string target
        property string targetId
        title: "Rename"
        width: 380
        function ask(t, id, value) { target = t; targetId = id; rn.text = value; open() }
        function apply() {
            // Layers and scenes rename in place (InlineRename); sources and assets here.
            target === "source" ? app.renameSource(targetId, rn.text) : app.renameAsset(targetId, rn.text)
            close()
        }
        onOpened: { rn.selectAll(); rn.forceActiveFocus() }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            FInput { id: rn; Layout.fillWidth: true; onAccepted: renameDialog.apply() }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FButton { text: "Cancel"; variant: "ghost"; onClicked: renameDialog.close() }
                FButton { text: "Rename"; variant: "primary"; onClicked: renameDialog.apply() }
            }
        }
    }
    // A new LOBS: what's in it, then download, verify and install (never while on air).
    FDialog {
        id: updateDialog
        title: appUpdater.state === "ready" ? "Install LOBS " + appUpdater.latest + "?" : "LOBS " + appUpdater.latest + " is available"
        width: 420
        readonly property bool onAir: app.live || app.replayActive || app.vcamActive
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: appUpdater.state === "downloading" ? "Downloading... " + Math.round(appUpdater.progress * 100) + "%"
                    : appUpdater.state === "ready" ? "LOBS closes, installs the update and opens again in about a minute. Your shows and settings stay as they are."
                    : appUpdater.message || ("You have " + appUpdater.current + ". The update downloads in the background while you keep working.")
            }
            Rectangle {
                visible: appUpdater.state === "downloading"
                Layout.fillWidth: true
                implicitHeight: 4; radius: 2; color: Pal.s1
                Rectangle { width: parent.width * appUpdater.progress; height: parent.height; radius: 2; color: Pal.focus }
            }
            Text {
                visible: updateDialog.onAir && appUpdater.state === "ready"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.body
                text: "Stop streaming, recording, the replay buffer and the virtual camera first. LOBS won't close while they're running."
            }
            RowLayout {
                Layout.fillWidth: true
                FButton { text: "What's new"; variant: "ghost"; visible: appUpdater.notesUrl.length > 0; onClicked: Qt.openUrlExternally(appUpdater.notesUrl) }
                Item { Layout.fillWidth: true }
                FButton { text: "Later"; variant: "secondary"; onClicked: updateDialog.close() }
                FButton {
                    text: appUpdater.state === "ready" ? "Restart and install" : "Download"
                    variant: "primary"
                    visible: appUpdater.state !== "downloading"
                    enabled: appUpdater.state === "available" || (appUpdater.state === "ready" && !updateDialog.onAir)
                    onClicked: appUpdater.state === "ready" ? appUpdater.installAndRestart() : appUpdater.download()
                }
            }
        }
    }
    // Last run crashed: offer to report it (a minidump and the log are in the crash folder).
    FDialog {
        id: crashDialog
        title: "LOBS closed unexpectedly"
        width: 420
        closePolicy: Popup.NoAutoClose
        // After the setup assistant on a first run, so one doesn't cover the other.
        Timer { interval: 900; running: crashReport.pending && !setupTimer.running && !setupWizard.visible && !crashDialog.visible; onTriggered: crashDialog.open() }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: "Sorry about that. LOBS saved a crash report on this PC. Reporting it on GitHub helps get it fixed. The issue asks for the log, which is safe to post. Keep the .dmp file private: it can hold what LOBS had in memory, like your stream key."
            }
            RowLayout {
                Layout.fillWidth: true
                FButton { text: "Show files"; variant: "ghost"; onClicked: crashReport.openFolder() }
                Item { Layout.fillWidth: true }
                FButton { text: "Not now"; variant: "secondary"; onClicked: { crashReport.dismiss(); crashDialog.close() } }
                FButton { text: "Report on GitHub"; variant: "primary"; onClicked: { crashReport.report(); crashReport.dismiss(); crashDialog.close() } }
            }
        }
    }
    FDialog {
        id: unsavedDialog
        property var next: null
        title: "Save this show first?"
        width: 400
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: "This show has never been saved. If you don't save it, its changes are lost."
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FButton { text: "Cancel"; variant: "ghost"; onClicked: unsavedDialog.close() }
                FButton { text: "Don't save"; variant: "secondary"; onClicked: { const n = unsavedDialog.next; unsavedDialog.close(); n() } }
                FButton { text: "Save…"; variant: "primary"; onClicked: { win.afterSave = unsavedDialog.next; unsavedDialog.close(); saveDialog.open() } }
            }
        }
    }
    FDialog {
        id: deleteSourceDialog
        objectName: "deleteSourceDialog"
        property var row: ({})
        title: "Delete " + (row.name || "source") + "?"
        width: 400
        function ask(r) { row = r; open() }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: "It's used in " + (deleteSourceDialog.row.scenes || []).length + " scenes (" + (deleteSourceDialog.row.scenes || []).join(", ")
                      + "). Deleting it removes it from all of them. You can undo this."
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FButton { text: "Cancel"; variant: "ghost"; onClicked: deleteSourceDialog.close() }
                FButton { text: "Delete from all scenes"; variant: "danger"; onClicked: { app.deleteSource(deleteSourceDialog.row.id); deleteSourceDialog.close() } }
            }
        }
    }
}
