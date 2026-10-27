import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import LOBS

// The fixed central area: live-lock banner, scene header, the engine preview, status line.
// libobs renders into a native window that the app keeps glued to the "preview" item.
Rectangle {
    id: canvasArea
    // Panels around the canvas give way before the video does (a row of panels wider than its
    // column used to crush the preview to nothing).
    property size kddockwidgets_min_size: Qt.size(480, 320)
    anchors.fill: parent
    color: Pal.s1
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: app.live
            Layout.fillWidth: true
            implicitHeight: 34
            color: "#2a1212"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                spacing: 8
                Rectangle { width: 8; height: 8; radius: 4; color: Pal.live }
                Text {
                    Layout.fillWidth: true
                    color: "#fecaca"; font.family: Pal.font; font.pixelSize: Pal.body
                    text: (app.streaming ? "Live" : "Recording") + (app.liveLocked ? chrome.keys(" · editing what's on air is locked. Hold Alt, or unlock to edit.") : " · live lock is off, edits go on air.")
                }
                FButton { visible: app.settings.liveLock !== false; text: app.liveLocked ? "Unlock" : "Lock again"; variant: "ghost"; onClicked: app.setLiveUnlock(app.liveLocked) }
            }
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 44
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8
                Text { text: app.editSceneName || "No scene"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550 }
                FBadge { visible: app.editSceneId === app.programSceneId; text: (app.streaming || app.recording) ? "On air" : "Showing"; tone: (app.streaming || app.recording) ? "live" : "neutral" }
                Item { Layout.fillWidth: true }
                FHelp {
                        title: "Canvas"
                        points: [
                            "Move: drag a layer. Snaps to edges and layers.",
                            "Resize: drag a corner. Shift: free resize.",
                            chrome.keys("Ctrl: drag without snapping."),
                            "Groups: click selects the group. Each double-click goes one level deeper.",
                            "Nudge: arrow keys move 1 px, Shift+arrows 10 px.",
                            chrome.keys("Live lock: hold Alt to move layers while live."),
                            "Show: switch to this scene."
                        ]
                    }
                // Preview scale: pixel-perfect (exact ratios, crisp text) or plain fit.
                FButton {
                    readonly property int pct: app.previewPercent
                    readonly property bool exact: pct === 100 || pct === 200 || pct === 50 || pct === 33 || pct === 25 || pct === 300
                    text: pct + "%"
                    variant: "ghost"
                    selected: app.pixelPerfect && exact
                    tip: app.pixelPerfect
                         ? (exact ? "Pixel-perfect: each canvas pixel lands exactly on screen pixels, so text is crisp. Click to fit the panel exactly instead."
                                  : "Pixel-perfect is on, but no exact ratio fits well at this panel size, so it fits the panel. Make the panel a little larger to snap.")
                         : "Fits the panel exactly (may soften text). Click for pixel-perfect scaling."
                    onClicked: app.pixelPerfect = !app.pixelPerfect
                }
                // The show's transition for Show; the menu switches it or opens the editor.
                FButton {
                    id: trButton
                    readonly property var cur: app.transitions.find(t => t.id === app.currentTransition) || null
                    text: cur ? cur.name + (cur.type === "cut" || cur.type === "stinger" ? "" : " · " + (cur.durationMs >= 1000 ? (cur.durationMs / 1000) + " s" : cur.durationMs + " ms")) : "Transition"
                    variant: "ghost"
                    tip: "How Show switches scenes"
                    onClicked: trMenu.popup(trButton, 0, trButton.height + 4)
                    FMenu {
                        id: trMenu
                        Instantiator {
                            active: trMenu.populated // built while the menu is open
                            model: app.transitions
                            delegate: FMenuItem {
                                required property var modelData
                                text: modelData.name
                                selected: modelData.id === app.currentTransition
                                onTriggered: app.setCurrentTransition(modelData.id)
                            }
                            onObjectAdded: (i, o) => trMenu.insertItem(i, o)
                            onObjectRemoved: (i, o) => trMenu.removeItem(o)
                        }
                        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                        FMenuItem { text: "Edit transitions…"; onTriggered: canvasArea.Window.window.openTransitions() }
                    }
                }
                FButton { text: "Show"; variant: "primary"; visible: app.editSceneId !== app.programSceneId; onClicked: app.take(); tip: "Switch to this scene" }
            }
        }
        // The canvas, with the graphic tools pinned right under the picture (not the panel's
        // bottom edge). The preview is a native window that covers anything drawn over it, so
        // its box is sized to the picture and the tools sit just below it.
        Item {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property int toolsH: 56
            readonly property real availH: Math.max(40, height - toolsH)
            // Re-evaluated when the scale rule or canvas changes (previewPercent tracks both).
            readonly property real videoH: (app.previewPercent, app.previewVideoHeight(width - 16, availH - 16, app.pixelPerfect))
            Item {
                // libobs renders here (a native window is kept on top of this item).
                id: preview
                objectName: "preview"
                anchors { left: parent.left; right: parent.right; top: parent.top }
                // Centered vertically with the tools as one block.
                anchors.topMargin: Math.max(0, Math.floor((stage.availH - height) / 2))
                height: Math.min(stage.availH, Math.ceil(stage.videoH) + 16)
            }
            GraphicTools {
                id: tools
                anchors { horizontalCenter: parent.horizontalCenter; top: preview.bottom; topMargin: 4 }
            }
            Caption {
                anchors { left: parent.left; leftMargin: 14; right: tools.left; rightMargin: 12; verticalCenter: tools.verticalCenter }
                text: app.toast; color: Pal.fg; elide: Text.ElideRight
            }
        }
    }
    // The central item is created before it's placed in the window, so wait for one.
    Loader {
        // Frozen frame of the edited scene, rendered by the engine when the live view steps aside.
        // Only the native preview window steps aside; the in-scene preview never does.
        parent: preview
        anchors.fill: parent
        active: !app.previewInScene
        sourceComponent: Image {
            visible: app.previewHidden && status === Image.Ready
            fillMode: Image.PreserveAspectFit
            cache: false
            asynchronous: false
            sourceSize.width: Math.round(preview.width * Screen.devicePixelRatio)
            source: app.previewHidden && app.editSceneId ? "image://thumbs/" + app.editSceneId + "?freeze" + app.freezeTick : ""
        }
    }
    // Right-click on the canvas.
    Connections {
        target: app
        function onCanvasMenuRequested(x, y) { canvasMenu.popup(preview, x, y) }
    }
    FMenu {
        id: canvasMenu
        FCopyToMenu { onCopied: canvasMenu.close() }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
        FMenu {
            id: progMenu
            title: "Open projector"
            Instantiator {
                active: progMenu.populated // built while the menu is open
                model: app.screens()
                delegate: FMenuItem { required property var modelData; text: modelData.name; onTriggered: app.openProjector("", modelData.index) }
                onObjectAdded: (i, o) => progMenu.insertItem(i, o)
                onObjectRemoved: (i, o) => progMenu.removeItem(o)
            }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { text: "New window"; onTriggered: app.openProjector("", -1) }
        }
        FMenu {
            id: sceneMenu
            title: "Open scene projector"
            Instantiator {
                active: sceneMenu.populated // built while the menu is open
                model: app.screens()
                delegate: FMenuItem { required property var modelData; text: modelData.name; onTriggered: app.openProjector(app.editSceneId, modelData.index) }
                onObjectAdded: (i, o) => sceneMenu.insertItem(i, o)
                onObjectRemoved: (i, o) => sceneMenu.removeItem(o)
            }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { text: "New window"; onTriggered: app.openProjector(app.editSceneId, -1) }
        }
    }
    readonly property var hostWindow: preview.Window.window
    onHostWindowChanged: {
        if (!hostWindow) return
        hostWindow.previewItem = preview
        previewHost.attach(preview)
    }
}
