import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import com.kdab.dockwidgets 2.0 as KDDW
import LOBS
import LOBS.Models
import LOBS.Web
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
    readonly property int screenEdgeRoom: maximized || visibility === Window.FullScreen ? 6 : 0
    font.family: Pal.font
    font.pixelSize: Pal.body

    property int sceneView: 0 // 0 list, 1 grid
    onSceneViewChanged: app.setGridVisible(sceneView === 1)
    readonly property bool dialogOpen: setupWizard.visible || crashDialog.visible || updateDialog.visible || newProjectDialog.visible || trDialog.visible || addDialog.visible || twitchSignIn.visible || settingsPage.visible || saveDialog.visible || openDialog.visible || importDialog.visible
    Shortcut { sequence: "Ctrl+,"; enabled: !chrome.nativeMenuBar; onActivated: settingsPage.show() }
    onDialogOpenChanged: app.setPreviewVisible(!dialogOpen)

    Shortcut { enabled: !chrome.nativeMenuBar; sequences: [StandardKey.Undo]; onActivated: app.undo() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: [StandardKey.Redo, "Ctrl+Y"]; onActivated: app.redo() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+D"; onActivated: app.duplicateSelected() }
    Shortcut { enabled: app.cropMode && !addDialog.visible; sequence: "Escape"; onActivated: app.cancelCrop() }
    Shortcut { enabled: app.cropMode && !addDialog.visible; sequences: ["Return", "Enter"]; onActivated: app.cropMode = false }
    Shortcut { sequence: "F2"; enabled: !win.focusInside(assetsGuest); onActivated: { const sel = app.layers.filter(l => l.selected); if (sel.length === 1) layersGuest.rename(sel[0].id) } }
    function focusInside(item) { for (let f = win.activeFocusItem; f; f = f.parent) if (f === item) return true; return false }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Copy; onActivated: app.copyLayers() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Cut; onActivated: app.cutLayers() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: StandardKey.Paste; onActivated: app.pasteLayers(false) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+Shift+V"; onActivated: app.pasteLayers(true) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+G"; onActivated: app.groupSelected() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+Shift+G"; onActivated: app.ungroupSelected() }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+]"]; onActivated: app.moveSelected(-1) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+["]; onActivated: app.moveSelected(1) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+Shift+]", "Ctrl+}"]; onActivated: app.moveSelected(-1000) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequences: ["Ctrl+Shift+[", "Ctrl+{"]; onActivated: app.moveSelected(1000) }
    Shortcut { enabled: !chrome.nativeMenuBar; sequence: "Ctrl+S"; onActivated: win.save() }
    Shortcut { sequence: "Ctrl+O"; enabled: !app.live && !chrome.nativeMenuBar; onActivated: win.guardUnsaved(() => openDialog.open()) }
    Shortcut { sequences: chrome.nativeMenuBar ? ["Delete", "Backspace"] : ["Delete"]; onActivated: app.deleteSelected() }
    function save() { if (!app.saveProject()) saveDialog.open() }
    function editAction(textFn, layersFn) {
        const f = win.activeFocusItem
        if (f && typeof f[textFn] === "function" && typeof f.selectAll === "function") f[textFn]()
        else layersFn()
    }

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
                Platform.MenuItem { text: "About LOBS"; role: Platform.MenuItem.AboutRole; onTriggered: settingsPage.show(8) }
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
                    shortcut: "Meta+Ctrl+F"
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
                    onObjectAdded: (i, o) => windowMac.insertItem(i + 4, o)
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
            if (next && app.projectFile !== "") next()
        }
        onRejected: win.afterSave = null
    }
    property var afterSave: null
    function guardUnsaved(next) {
        if (app.unsavedShow) { unsavedDialog.next = next; unsavedDialog.open() }
        else next()
    }
    FileDialog { id: importDialog; fileMode: FileDialog.OpenFile; nameFilters: ["OBS scene collection (*.json)"]; onAccepted: app.importObs(selectedFile.toString().replace(/^file:\/{3}/, "")) }
    FileDialog { id: openDialog; fileMode: FileDialog.OpenFile; nameFilters: ["LOBS show (*.lobs.json *.json)"]; onAccepted: app.open(selectedFile) }

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Divider: Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    component VDivider: Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Pal.border }

    function openTransitions() { trDialog.open() }
    function showInspector() { if (!inspectorDock.isOpen) inspectorDock.show(); inspectorDock.dockWidget.setAsCurrentTab() }
    function togglePanel(d) {
        if (d.isOpen) d.close()
        else if (d === statsDock && historyDock.isOpen) historyDock.addDockWidgetAsTab(statsDock)
        else if (d === captionsDock && audioDock.isOpen) dockArea.addDockWidget(captionsDock, KDDW.KDDockWidgets.Location_OnRight, audioDock, Qt.size(380, 0))
        else if (layersDock.isOpen && d !== layersDock) dockArea.addDockWidget(d, KDDW.KDDockWidgets.Location_OnBottom, layersDock, Qt.size(0, 360))
        else d.show()
    }
    TransitionsDialog { id: trDialog }
    NewProjectDialog { id: newProjectDialog; onSaveCurrentAs: saveDialog.open() }
    SetupWizard {
        id: setupWizard
        onAddPanels: (chat, info) => win.openTwitchPanels(chat, info)
        onTwitchConnectRequested: twitchSignIn.begin()
        onTuneMicRequested: {
            if (!app.mixer.some(m => m.kind === "mic"))
                app.addSource("mic", "", "", false)
            const mic = app.mixer.find(m => m.kind === "mic")
            if (mic) Qt.callLater(() => addMicWizard.openFor(mic.id, true))
        }
    }
    function openSetup() { setupWizard.open() }
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
    Timer { interval: 1500; running: app.envValue("LOBS_OPEN_SETTINGS") !== ""; onTriggered: settingsPage.show(Number(app.envValue("LOBS_OPEN_SETTINGS"))) }
    property string twitchChannel: app.uiValue("twitch/channel", "")
    function setTwitchChannel(c) { app.setUiValue("twitch/channel", c); twitchChannel = c }
    Connections { target: twitch; function onChanged() { if (twitch.connected && twitch.login !== win.twitchChannel) win.setTwitchChannel(twitch.login) } }
    function openTwitchPanels(chat, info) {
        twitchChannel = app.uiValue("twitch/channel", "")
        if (chat && !chatDock.isOpen) dockArea.addDockWidget(chatDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
        if (info && !infoDock.isOpen) {
            if (chatDock.isOpen) chatDock.addDockWidgetAsTab(infoDock)
            else dockArea.addDockWidget(infoDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
        }
        if (info && !feedDock.isOpen) {
            if (infoDock.isOpen) infoDock.addDockWidgetAsTab(feedDock)
            else dockArea.addDockWidget(feedDock, KDDW.KDDockWidgets.Location_OnRight, null, Qt.size(340, 0))
        }
    }
    WhatsNewDialog { id: whatsNew }
    Timer { interval: 1500; running: true; onTriggered: if (!setupWizard.visible) whatsNew.check() }
    Timer { id: setupTimer; interval: 900; running: !app.uiFlag("setup/done", false) && app.envValue("LOBS_NO_SETUP") === ""; onTriggered: setupWizard.open() }
    background: Rectangle {
        color: Pal.s1
        MouseArea { anchors.fill: parent; onPressed: (m) => { app.clearSelection(); m.accepted = false } }
    }

    footer: StatusBar {}

    Item { id: overlayRef; readonly property Item overlay: Overlay.overlay }
    Timer {
        interval: 1000; repeat: true; running: !app.previewInScene
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

    ToolTipStyle { host: win.contentItem; previewItem: win.previewItem }

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
                onClicked: settingsPage.open ? settingsPage.open = false : settingsPage.show()
                ToolTip.visible: hovered && !settingsPage.open
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
                        Gem {
                            id: brandGem
                            width: 20; height: 20
                            anchors.verticalCenter: parent.verticalCenter
                            NumberAnimation on frame { id: brandRoll; running: false; duration: 450; easing.type: Easing.OutCubic }
                            Connections {
                                target: settingsPage
                                function onOpenChanged() {
                                    brandRoll.stop()
                                    brandRoll.from = brandGem.frame
                                    brandRoll.to = settingsPage.open ? brandGem.count - 1 : 0
                                    brandRoll.start()
                                }
                            }
                        }
                        Text { text: "LOBS"; color: Pal.fg; font.family: Pal.font; font.pixelSize: 15; font.weight: 700; anchors.verticalCenter: parent.verticalCenter }
                        FIcon { name: "settings"; size: 14; color: brand.hovered ? Pal.fg : Pal.muted; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
            }
            FButton { text: "New"; visible: !settingsPage.open && (!chrome.nativeMenuBar); variant: "ghost"; onClicked: newProjectDialog.open(); tip: "New project in its own folder" }
            FButton { text: "Open"; visible: !settingsPage.open && (!chrome.nativeMenuBar); variant: "ghost"; enabled: !app.live; tip: app.live ? "Stop streaming and recording to open another show" : chrome.keys("Open a show (Ctrl+O)"); onClicked: win.guardUnsaved(() => openDialog.open()) }
            FButton {
                id: importButton
                visible: !settingsPage.open && (!chrome.nativeMenuBar)
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
                        active: importMenu.populated
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
                    FMenuItem { text: "Choose a file…"; onTriggered: win.guardUnsaved(() => importDialog.open()) }
                }
            }
            FButton { text: "Save"; visible: !settingsPage.open && (!chrome.nativeMenuBar); variant: "ghost"; tip: chrome.keys("Save (Ctrl+S)"); onClicked: win.save() }
            Item {
                visible: !settingsPage.open
                id: profileBox
                readonly property var list: { app.settings; return app.profiles() }
                implicitWidth: profileButton.implicitWidth
                implicitHeight: profileButton.implicitHeight
                function finish(err) { if (err) app.notify(err) }
                FButton {
                    id: profileButton
                    text: "Profile"
                    variant: "ghost"
                    onClicked: profileMenu.popup(profileButton, 0, profileButton.height + 4)
                }
                Popup {
                    id: profileName
                    popupType: Popup.Item
                    property string mode: "new"
                    function ask(m) { mode = m; nameIn.text = m === "rename" ? (app.settings.profile || "") : ""; err.text = ""; open() }
                    function apply() {
                        const n = nameIn.text.trim()
                        if (!n) return
                        const e = mode === "new" ? app.newProfile(n) : app.renameProfile(app.settings.profile, n)
                        if (e) err.text = e; else close()
                    }
                    y: profileButton.height + 4
                    width: 300
                    padding: 14
                    modal: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                    onOpened: { nameIn.selectAll(); nameIn.forceActiveFocus() }
                    background: Rectangle { radius: Pal.radius + 4; color: Pal.s4; border.width: 1; border.color: Pal.border }
                    contentItem: ColumnLayout {
                        spacing: 8
                        Text { text: profileName.mode === "new" ? "New profile" : "Rename profile"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 600 }
                        FInput { id: nameIn; Layout.fillWidth: true; placeholderText: "Name"; onAccepted: profileName.apply() }
                        Text {
                            visible: profileName.mode === "new"
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: "Copies " + (app.settings.profile || "Default") + "'s stream, video and recording settings."
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                        }
                        Text { id: err; visible: text !== ""; Layout.fillWidth: true; wrapMode: Text.WordWrap; color: Pal.danger; font.family: Pal.font; font.pixelSize: Pal.caption }
                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            FButton { text: "Cancel"; variant: "ghost"; onClicked: profileName.close() }
                            FButton { text: profileName.mode === "new" ? "Create" : "Rename"; variant: "primary"; enabled: nameIn.text.trim() !== ""; onClicked: profileName.apply() }
                        }
                    }
                }
                FMenu {
                    id: profileMenu
                    width: 240
                    Instantiator {
                        active: profileMenu.populated
                        model: profileBox.list
                        delegate: FMenuItem {
                            required property string modelData
                            text: modelData
                            selected: modelData === app.settings.profile
                            enabled: !app.live || modelData === app.settings.profile
                            onTriggered: profileBox.finish(app.switchProfile(modelData))
                        }
                        onObjectAdded: (i, o) => profileMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => profileMenu.removeItem(o)
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem { text: "New profile"; enabled: !app.live; onTriggered: profileName.ask("new") }
                    FMenuItem { text: "Rename"; onTriggered: profileName.ask("rename") }
                    Instantiator {
                        active: profileMenu.populated
                        model: profileBox.list.filter(n => n !== app.settings.profile)
                        delegate: FMenuItem { required property string modelData; text: "Delete " + modelData; onTriggered: profileBox.finish(app.deleteProfile(modelData)) }
                        onObjectAdded: (i, o) => profileMenu.addItem(o)
                        onObjectRemoved: (i, o) => profileMenu.removeItem(o)
                    }
                }
            }
            FButton {
                visible: !settingsPage.open && (["available", "downloading", "ready"].indexOf(appUpdater.state) >= 0)
                iconName: "refresh-cw"
                text: appUpdater.state === "downloading" ? "LOBS " + appUpdater.latest + " " + Math.round(appUpdater.progress * 100) + "%"
                    : appUpdater.state === "ready" ? "Install LOBS " + appUpdater.latest
                    : "LOBS " + appUpdater.latest + " available"
                variant: "ghost"
                tip: "A new version of LOBS"
                onClicked: updateDialog.open()
            }
            Rectangle { visible: !settingsPage.open && (!chrome.nativeMenuBar); width: 1; height: 18; color: Pal.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }
            FButton {
                visible: !settingsPage.open && (!chrome.nativeMenuBar)
                text: "Window"
                variant: "ghost"
                onClicked: windowMenu.popup(this, 0, height + 4)
                FMenu {
                    id: windowMenu
                    width: 240
                    Instantiator {
                        active: windowMenu.populated
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
                    MenuSeparator { visible: windowMenu.populated && app.pluginTools().length > 0; height: visible ? implicitHeight : 0; contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    Instantiator {
                        active: windowMenu.populated
                        model: windowMenu.populated ? app.pluginTools() : []
                        delegate: FMenuItem { required property string modelData; text: modelData; onTriggered: app.triggerPluginTool(modelData) }
                        onObjectAdded: (i, o) => windowMenu.addItem(o)
                        onObjectRemoved: (i, o) => windowMenu.removeItem(o)
                    }
                    FMenuItem { text: "Reset layout"; onTriggered: layoutSaver.restoreFromFile(win.defaultLayoutFile) }
                }
            }
            Rectangle { visible: !settingsPage.open; width: 1; height: 18; color: Pal.border; Layout.leftMargin: 6; Layout.rightMargin: 6 }
            FButton { visible: !settingsPage.open; iconName: "undo-2"; variant: "ghost"; enabled: app.canUndo; onClicked: app.undo(); tip: "Undo " + app.undoLabel + chrome.keys("  Ctrl Z") }
            FButton { visible: !settingsPage.open; iconName: "redo-2"; variant: "ghost"; enabled: app.canRedo; onClicked: app.redo(); tip: chrome.keys("Redo  Ctrl Shift Z") }
            Item { Layout.fillWidth: true }
            OutputControls { visible: !settingsPage.open }
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

    PermissionsBar {
        id: permissionsBar
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 6; bottomMargin: 0 }
    }

    KDDW.DockingArea {
        id: dockArea
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; top: permissionsBar.visible ? permissionsBar.bottom : parent.top }
        anchors.margins: 6
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
            ScenesPanel {
                id: scenesGuest
                anchors.fill: parent
                sceneView: win.sceneView
                onViewPicked: (v) => win.sceneView = v
            }
        }

        KDDW.DockWidget {
            id: poolDock
            uniqueName: "pool"
            title: "Sources"
            SourcesPanel {
                id: poolGuest
                anchors.fill: parent
                previewItem: win.previewItem
                ghostItem: ghost
                overlayItem: win.contentItem
                onAddSourceRequested: addDialog.open()
                onDeleteSourceRequested: (r) => deleteSourceDialog.ask(r)
            }
        }

        KDDW.DockWidget {
            id: assetsDock
            uniqueName: "assets"
            title: "Assets"
            AssetsPanel {
                id: assetsGuest
                anchors.fill: parent
                previewItem: win.previewItem
                ghostItem: ghost
                overlayItem: win.contentItem
            }
        }

        KDDW.DockWidget {
            id: layersDock
            uniqueName: "layers"
            title: "Layers"
            LayersPanel {
                id: layersGuest
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: inspectorDock
            uniqueName: "inspector"
            title: "Inspector"
            InspectorPanel {
                id: inspectorGuest
                anchors.fill: parent
            }
        }

        KDDW.DockWidget {
            id: historyDock
            uniqueName: "history"
            title: "History"
            Loader { id: historyGuest; property size kddockwidgets_min_size: Qt.size(240, 120); anchors.fill: parent; sourceComponent: historyView }
        }

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
        Instantiator {
            model: app.pluginDocks()
            delegate: KDDW.DockWidget {
                required property var modelData
                uniqueName: "plugin_" + modelData.id
                title: modelData.title
                PluginPanel {
                    property size kddockwidgets_min_size: Qt.size(240, 160)
                    anchors.fill: parent
                    dockId: modelData.id
                }
            }
            onObjectAdded: (i, o) => win.pluginPanels = win.pluginPanels.concat([o])
            onObjectRemoved: (i, o) => win.pluginPanels = win.pluginPanels.filter(d => d !== o)
        }
        KDDW.DockWidget {
            id: feedDock
            uniqueName: "twitchFeed"
            title: "Activity feed"
            Loader {
                property size kddockwidgets_min_size: Qt.size(260, 200)
                anchors.fill: parent
                active: feedDock.isOpen || item
                sourceComponent: TwitchPanel { kind: "feed"; live: feedDock.isOpen; channel: win.twitchChannel; onChannelPicked: (c) => win.setTwitchChannel(c) }
            }
        }

        KDDW.DockWidget {
            id: audioDock
            uniqueName: "audio"
            title: "Audio mixer"
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
            poolDock.addDockWidgetAsTab(audioDock)
            poolDock.dockWidget.setAsCurrentTab()
            layoutSaver.saveToFile(win.defaultLayoutFile)
            layoutSaver.restoreFromFile(win.layoutFile)
            if (!win.panels.some(d => d.isOpen))
                layoutSaver.restoreFromFile(win.defaultLayoutFile)
            if (app.firstTime("assetsDocked"))
                Qt.callLater(() => addDockWidget(assetsDock, KDDW.KDDockWidgets.Location_OnBottom, poolDock, Qt.size(0, 300)))
            win.layoutReady = true
        }
    }
    KDDW.LayoutSaver { id: layoutSaver }
    readonly property string layoutFile: app.dataFolder + "/layout-v2.json"
    readonly property string defaultLayoutFile: app.dataFolder + "/layout-default-v2.json"
    property bool layoutReady: false
    Timer { interval: 30000; repeat: true; running: win.layoutReady; onTriggered: layoutSaver.saveToFile(win.layoutFile) }
    onClosing: { if (layoutReady) layoutSaver.saveToFile(win.layoutFile); layoutReady = false; app.closeProjectors() }
    property var pluginPanels: []
    readonly property var panels: [scenesDock, poolDock, assetsDock, layersDock, inspectorDock, audioDock, historyDock, statsDock, captionsDock, chatDock, infoDock, feedDock].concat(pluginPanels)
    property Item previewItem: null

    readonly property var panelDrag: panelDragController
    PanelDragController {
        id: panelDragController
        parent: Overlay.overlay
        dockArea: dockArea
        panels: [
            { dock: videoDock, guest: videoGuest, icon: "monitor" },
            { help: [app.clickToShow ? "Click: switch to the scene." : "Click: edit a scene without showing it.", app.clickToShow ? "Right-click › Edit before showing: set one up first." : "Show or double-click: switch to the scene.", "Grid: live thumbnails of every scene.", "Right-click: rename, duplicate or delete."], dock: scenesDock, guest: scenesGuest, icon: "clapperboard" },
            { help: ["Sources: add once, use in any scene.", "Place it: drag onto the canvas or double-click.", "Per layer: size, crop and filters.", "Files are in Assets. Add text and color from Layers."], dock: poolDock, guest: poolGuest, icon: "folder" },
            { help: ["Files: this project's assets folder.", "Add files: drag in from File Explorer or click Add.", "Place it: drag onto the canvas or double-click.", "Right-click: show in File Explorer or move to Recycle Bin."], dock: assetsDock, guest: assetsGuest, icon: "image" },
            { help: ["Order: layers higher in the list draw on top.", "Linked: settings shared with another scene.", chrome.keys("Keys: Ctrl+C copy, Ctrl+V paste, Ctrl+Shift+V paste linked, Ctrl+D duplicate, Ctrl+G group, Delete remove."), "Copy to: copy layers to another scene."], dock: layersDock, guest: layersGuest, icon: "layers" },
            { help: ["This scene: layer changes apply here only.", "Linked: shared with another scene. Unlink to separate.", "Numbers: type math like 1920/2. Up and Down nudge, Shift for 10.", "Source: select it in Sources to edit it in every scene."], dock: inspectorDock, guest: inspectorGuest, icon: "sliders-horizontal" },
            { help: ["Global: Mic and Desktop Audio play in every scene.", "Scene audio: plays only in scenes that contain it.", "Active in: Recording and Stream. Turn off Recording to keep a sound, such as alerts, out of recordings.", "Monitor: hear it yourself. Monitor only keeps it off the stream and recording.", "Fader: set the level. Meter shows peak dB."], dock: audioDock, guest: audioGuest, icon: "audio-lines" },
            { help: [chrome.keys("Undo: Ctrl+Z; redo: Ctrl+Shift+Z or Ctrl+Y."), "Jump: click a step to go back to it."], dock: historyDock, guest: historyGuest, icon: "history" },
            { help: ["Frames missed: the PC couldn't render in time.", "Frames skipped: the encoder fell behind.", "Dropped frames: the network couldn't keep up.", "Orange: over 1%."], dock: statsDock, guest: statsGuest, icon: "gauge" },
            { help: ["Speech to text from one audio source, on this PC.", "Send to stream: viewers turn on CC to see them.", "Language: pick one or detect it automatically.", "Text appears a few seconds after speech."], dock: captionsDock, guest: captionsGuest, icon: "captions" }
        ]
    }

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

    Component { id: mixerView; MixerView {} }

    Component {
        id: historyView
        FluidList {
            model: RowsModel { rows: app.history }
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
    AddSourceDialog {
        id: addDialog
        onTuneMicRequested: (id) => addMicWizard.openFor(id)
    }

    MicWizard { id: addMicWizard; fromAdd: true }
    SettingsPage { id: settingsPage; parent: Overlay.overlay; anchors.fill: parent; anchors.topMargin: topBar.height; z: 500
        onUpdateRequested: { open = false; updateDialog.open() }
        onTwitchConnectRequested: twitchSignIn.begin() }

    FDialog {
        id: twitchSignIn
        title: "Connect with Twitch"
        width: 520
        z: 1000
        property string url: ""
        function begin() { url = twitch.beginSignIn(); if (url) open(); else failToast() }
        function failToast() { app.notify(twitch.error) }
        onClosed: { twitch.cancelSignIn(); url = "" }
        Connections {
            target: twitch
            function onSignedIn() { if (twitchSignIn.visible) { twitchSignIn.close(); app.notify("Connected as " + twitch.displayName) } }
            function onSignInFailed(why) { if (twitchSignIn.visible) twitchSignIn.close(); app.notify(why) }
        }
        ColumnLayout {
            anchors.fill: parent
            spacing: 10
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 600
                radius: Pal.radius; color: Pal.s1; clip: true
                BrowserView {
                    id: twitchPage
                    anchors.fill: parent
                    url: twitchSignIn.url
                    live: twitchSignIn.visible
                }
                Text {
                    visible: !twitchPage.ready || twitch.busy
                    anchors.centerIn: parent
                    text: twitch.busy ? "Getting your stream key…" : "Loading Twitch…"
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                }
            }
            RowLayout {
                Layout.fillWidth: true
                FButton { text: "−"; variant: "ghost"; tip: "Zoom out"; enabled: twitchPage.zoom > 0.51; onClicked: twitchPage.zoom -= 0.1 }
                FButton { text: Math.round(twitchPage.zoom * 100) + "%"; variant: "ghost"; tip: "Back to 100%"; onClicked: twitchPage.zoom = 1 }
                FButton { text: "+"; variant: "ghost"; tip: "Zoom in"; enabled: twitchPage.zoom < 2.49; onClicked: twitchPage.zoom += 0.1 }
                FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "Reload the page"; onClicked: twitchPage.reload() }
                Item { Layout.fillWidth: true }
                FButton { text: "Cancel"; variant: "ghost"; onClicked: twitchSignIn.close() }
            }
        }
    }
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
                text: appUpdater.state === "downloading" ? "Downloading… " + Math.round(appUpdater.progress * 100) + "%"
                    : appUpdater.state === "ready" ? "LOBS will close, install the update and reopen in about a minute. Your shows and settings are kept."
                    : appUpdater.message || ("You have " + appUpdater.current + ". The update downloads in the background.")
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
                text: "Stop streaming, recording, the replay buffer and the virtual camera first."
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
    FDialog {
        id: crashDialog
        title: "LOBS closed unexpectedly"
        width: 420
        closePolicy: Popup.NoAutoClose
        Timer { interval: 900; running: crashReport.pending && !setupTimer.running && !setupWizard.visible && !crashDialog.visible; onTriggered: crashDialog.open() }
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: "LOBS saved a crash report on this PC. Report it on GitHub and attach the log. Don't post the .dmp file: it can contain your stream key."
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
                text: "This show hasn't been saved. Your changes will be lost if you don't save it."
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
                text: "It's in " + (deleteSourceDialog.row.scenes || []).length + " scenes (" + (deleteSourceDialog.row.scenes || []).join(", ")
                      + ") and will be removed from all of them. You can undo this."
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
