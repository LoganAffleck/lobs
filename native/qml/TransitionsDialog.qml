import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Transitions: the show's list (left) and the selected one's options with a live A→B preview
// (right). The one marked "In use" is what Show plays. Everything is saved in the show.
FDialog {
    id: dlg
    title: "Transitions"
    width: 760
    height: 600
    property string selId: ""
    readonly property var sel: app.transitions.find(t => t.id === selId) || app.transitions[0] || null
    readonly property var st: sel ? sel.settings : ({})
    readonly property var typeNames: ({ cut: "Cut", fade: "Fade", swipe: "Swipe", slide: "Slide", fade_to_color: "Fade to color", luma_wipe: "Luma wipe", stinger: "Stinger" })
    readonly property var typeIcons: ({ cut: "scissors", fade: "blend", swipe: "arrow-right", slide: "move-horizontal", fade_to_color: "circle", luma_wipe: "image", stinger: "film" })
    function set(field, v) { if (sel) app.updateTransition(sel.id, field, v) }
    function play() { if (sel) { app.previewTransition(sel.id); pv.playing = true; pv.stopAt = Date.now() + (sel.type === "stinger" ? 6000 : sel.durationMs + 400) } }
    onOpened: { selId = app.currentTransition; Qt.callLater(play) }
    onClosed: { pv.playing = false; app.stopTransitionPreview() }
    // Replay when an option changes, so you see the effect right away.
    onStChanged: if (visible) replay.restart()
    Timer { id: replay; interval: 150; onTriggered: dlg.play() }

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    component Label: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 96 }

    contentItem: RowLayout {
        spacing: 14
        // ---- list ----
        ColumnLayout {
            Layout.preferredWidth: 210
            Layout.fillHeight: true
            spacing: 6
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: app.transitions
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool isSel: dlg.sel && modelData.id === dlg.sel.id
                    readonly property bool inUse: modelData.id === app.currentTransition
                    width: ListView.view.width
                    height: 34
                    radius: Pal.radius
                    color: isSel ? Pal.selected : rowMouse.containsMouse ? Pal.hover : "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                        spacing: 8
                        FIcon { name: dlg.typeIcons[modelData.type] || "blend"; size: 14; color: Pal.muted }
                        Text { text: modelData.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight; Layout.fillWidth: true }
                        FBadge { visible: inUse; text: "In use"; tone: "neutral" }
                    }
                    MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { dlg.selId = modelData.id; dlg.play() }
                        onDoubleClicked: app.setCurrentTransition(modelData.id) }
                }
            }
            FButton {
                id: addBtn
                Layout.fillWidth: true
                iconName: "plus"; text: "Add transition"; variant: "secondary"
                onClicked: addMenu.popup(addBtn, 0, addBtn.height + 4)
                FMenu {
                    id: addMenu
                    Instantiator {
                        active: addMenu.populated // built while the menu is open
                        model: ["fade", "cut", "swipe", "slide", "fade_to_color", "luma_wipe", "stinger"]
                        delegate: FMenuItem {
                            required property string modelData
                            text: dlg.typeNames[modelData]
                            onTriggered: { const id = app.addTransition(modelData); if (id) { dlg.selId = id; Qt.callLater(dlg.play) } }
                        }
                        onObjectAdded: (i, o) => addMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => addMenu.removeItem(o)
                    }
                }
            }
        }
        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Pal.border }
        // ---- details ----
        ColumnLayout {
            visible: !!dlg.sel
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10
            // Preview: A (blue) to B (green), back and forth on each play.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: width * 9 / 16
                radius: Pal.radius
                color: "#000000"
                clip: true
                Image {
                    id: pv
                    anchors.fill: parent
                    property bool playing: false
                    property real stopAt: 0
                    property int tick: 0
                    cache: false
                    asynchronous: false
                    fillMode: Image.PreserveAspectFit
                    sourceSize.width: 480
                    source: dlg.visible ? "image://thumbs/__transition?" + tick : ""
                    Timer {
                        interval: 33; repeat: true
                        running: pv.playing && dlg.visible
                        onTriggered: { pv.tick++; if (Date.now() > pv.stopAt) pv.playing = false }
                    }
                }
                FButton {
                    anchors { right: parent.right; bottom: parent.bottom; margins: 8 }
                    iconName: "play"; text: "Preview"; variant: "secondary"
                    onClicked: dlg.play()
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                FInput {
                    id: nameField
                    Layout.fillWidth: true
                    text: dlg.sel ? dlg.sel.name : ""
                    onEditingFinished: if (dlg.sel && text !== dlg.sel.name) dlg.set("name", text)
                }
                FButton {
                    text: dlg.sel && dlg.sel.id === app.currentTransition ? "In use" : "Use for Show"
                    variant: dlg.sel && dlg.sel.id === app.currentTransition ? "ghost" : "primary"
                    enabled: !!dlg.sel && dlg.sel.id !== app.currentTransition
                    tip: "Used when Show switches scenes"
                    onClicked: app.setCurrentTransition(dlg.sel.id)
                }
                FButton {
                    iconName: "trash-2"; variant: "ghost"
                    enabled: app.transitions.length > 1
                    tip: "Delete this transition"
                    onClicked: { const id = dlg.sel.id; app.removeTransition(id); dlg.selId = app.currentTransition }
                }
            }
            FScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: parent.width
                    spacing: 10
                    readonly property string type: dlg.sel ? dlg.sel.type : ""
                    Caption {
                        visible: parent.type === "cut"
                        text: "Switches instantly, with no animation."
                    }
                    FFillSlider {
                        visible: parent.type !== "cut" && parent.type !== "stinger"
                        Layout.fillWidth: true
                        label: "Duration"
                        from: 100; to: 3000; decimals: 0
                        value: dlg.sel ? dlg.sel.durationMs : 300
                        format: (v) => v >= 1000 ? (v / 1000).toFixed(2) + " s" : Math.round(v) + " ms"
                        onReleased: (v) => dlg.set("durationMs", Math.round(v / 50) * 50)
                    }
                    // Swipe / slide
                    RowLayout {
                        visible: parent.type === "swipe" || parent.type === "slide"
                        spacing: 6
                        Label { text: "Direction" }
                        Repeater {
                            model: [{ v: "left", i: "arrow-left" }, { v: "right", i: "arrow-right" }, { v: "up", i: "arrow-up" }, { v: "down", i: "arrow-down" }]
                            delegate: FButton {
                                required property var modelData
                                iconName: modelData.i; variant: "ghost"
                                selected: (dlg.st.direction || "left") === modelData.v
                                tip: modelData.v.charAt(0).toUpperCase() + modelData.v.slice(1)
                                onClicked: dlg.set("direction", modelData.v)
                            }
                        }
                    }
                    FCheckbox {
                        visible: parent.type === "swipe"
                        text: "Swipe in (the new scene moves over the old one)"
                        checked: !!dlg.st.swipe_in
                        onToggled: dlg.set("swipe_in", checked)
                    }
                    // Fade to color
                    RowLayout {
                        visible: parent.type === "fade_to_color"
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Color" }
                        FColorField { Layout.fillWidth: true; color: dlg.st.color || "#000000"; onPicked: (c) => dlg.set("color", c) }
                    }
                    FFillSlider {
                        visible: parent.type === "fade_to_color"
                        Layout.fillWidth: true
                        label: "Switch point"
                        from: 0; to: 100; decimals: 0
                        value: dlg.st.switch_point === undefined ? 50 : dlg.st.switch_point
                        format: (v) => Math.round(v) + "%"
                        onReleased: (v) => dlg.set("switch_point", Math.round(v))
                    }
                    // Luma wipe
                    RowLayout {
                        visible: parent.type === "luma_wipe"
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Pattern" }
                        FSelect {
                            Layout.fillWidth: true
                            options: app.lumaWipes().map(f => ({ name: f.replace(/\.png$/, "").replace(/-/g, " "), value: f }))
                            currentValue: dlg.st.luma_image || "linear-h.png"
                            onPicked: (v) => dlg.set("luma_image", v)
                        }
                    }
                    FFillSlider {
                        visible: parent.type === "luma_wipe"
                        Layout.fillWidth: true
                        label: "Softness"
                        from: 0; to: 1; decimals: 2
                        value: dlg.st.luma_softness === undefined ? 0.03 : dlg.st.luma_softness
                        onReleased: (v) => dlg.set("luma_softness", v)
                    }
                    FCheckbox {
                        visible: parent.type === "luma_wipe"
                        text: "Invert"
                        checked: !!dlg.st.luma_invert
                        onToggled: dlg.set("luma_invert", checked)
                    }
                    // Stinger
                    RowLayout {
                        visible: parent.type === "stinger"
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Video" }
                        FInput { Layout.fillWidth: true; placeholderText: "A video with transparency (.webm, .mov)"; text: dlg.st.path || ""; onEditingFinished: if (text !== (dlg.st.path || "")) dlg.set("path", text) }
                        FButton { text: "Browse"; variant: "secondary"; onClicked: stingerPicker.open() }
                    }
                    FFillSlider {
                        visible: parent.type === "stinger"
                        Layout.fillWidth: true
                        label: "Switch scenes at"
                        from: 0; to: 5000; decimals: 0
                        value: dlg.st.transition_point === undefined ? 500 : dlg.st.transition_point
                        format: (v) => Math.round(v) + " ms"
                        onReleased: (v) => dlg.set("transition_point", Math.round(v / 10) * 10)
                    }
                    RowLayout {
                        visible: parent.type === "stinger"
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Audio" }
                        FSelect {
                            Layout.fillWidth: true
                            options: [{ name: "Viewers hear it", value: 2 }, { name: "Only you hear it", value: 1 }, { name: "Silent", value: 0 }]
                            currentValue: dlg.st.audio_monitoring === undefined ? 0 : dlg.st.audio_monitoring
                            onPicked: (v) => dlg.set("audio_monitoring", v)
                        }
                    }
                    RowLayout {
                        visible: parent.type === "stinger"
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Scene audio" }
                        FSelect {
                            Layout.fillWidth: true
                            options: [{ name: "Fade out, then in", value: 0 }, { name: "Crossfade", value: 1 }]
                            currentValue: dlg.st.audio_fade_style === undefined ? 0 : dlg.st.audio_fade_style
                            onPicked: (v) => dlg.set("audio_fade_style", v)
                        }
                    }
                    Caption {
                        visible: parent.type === "stinger"
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                        text: "Plays a video over the switch. The scene changes at the time above."
                    }
                }
            }
        }
    }
    FileDialog {
        id: stingerPicker
        title: "Stinger video"
        nameFilters: ["Videos (*.webm *.mov *.mp4 *.mkv *.avi)", "All files (*)"]
        onAccepted: dlg.set("path", selectedFile.toString().replace(/^file:\/{3}/, ""))
    }
    footer: RowLayout {
        Item { Layout.fillWidth: true }
        Caption { text: "Double-click a transition to use it for Show."; Layout.rightMargin: 8 }
        FButton { text: "Done"; variant: "primary"; onClicked: dlg.close(); Layout.rightMargin: 16; Layout.bottomMargin: 14 }
    }
}
