import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Transitions: the show's list (left) and the selected one (right): what it is, a looping
// preview on two sample scenes, then its options in one labelled column.
FDialog {
    id: dlg
    title: "Transitions"
    width: 780
    height: 720
    property string selId: ""
    readonly property var sel: app.transitions.find(t => t.id === selId) || app.transitions[0] || null
    readonly property var st: sel ? sel.settings : ({})
    readonly property string type: sel ? sel.type : ""
    readonly property bool inUse: !!sel && sel.id === app.currentTransition
    readonly property var typeNames: ({ cut: "Cut", fade: "Fade", swipe: "Swipe", slide: "Slide", fade_to_color: "Fade to color", luma_wipe: "Luma wipe", stinger: "Stinger" })
    readonly property var typeIcons: ({ cut: "scissors", fade: "blend", swipe: "arrow-right", slide: "move-horizontal", fade_to_color: "circle", luma_wipe: "image", stinger: "film" })
    readonly property var typeHelp: ({
        cut: "Switches instantly, with no animation.",
        fade: "The new scene fades in over the old one.",
        swipe: "The new scene pushes in from one side.",
        slide: "Both scenes slide together, like turning a page.",
        fade_to_color: "Fades to a color, then into the new scene.",
        luma_wipe: "Wipes across in the shape of a pattern.",
        stinger: "Plays a video over the switch. The scene changes underneath it at the switch point."
    })
    function set(field, v) { if (sel) app.updateTransition(sel.id, field, v) }

    // ---- stinger video facts: path -> {exists, ok, durationMs, w, h, alpha} ----
    property var probes: ({})
    property int frameTick: 0
    readonly property var probe: type === "stinger" && st.path ? (probes[st.path] || null) : null
    readonly property int switchMs: st.transition_point === undefined ? 500 : st.transition_point
    function probeAll() {
        app.transitions.forEach(t => { if (t.type === "stinger" && t.settings.path) app.probeVideo(t.settings.path, t.settings.transition_point === undefined ? 500 : t.settings.transition_point) })
    }
    function problem(t) { // why a stinger can't play, or ""
        if (t.type !== "stinger") return ""
        if (!t.settings.path) return "No video chosen"
        const p = probes[t.settings.path]
        if (!p) return ""
        if (!p.exists) return "Video not found"
        if (!p.ok) return "Can't play this video"
        return ""
    }
    Connections {
        target: app
        function onVideoProbed(path, info) {
            if (info.stale) return
            const m = Object.assign({}, dlg.probes); m[path] = info; dlg.probes = m
            dlg.frameTick++
        }
    }

    // ---- preview: A to B and back, on a loop while the dialog is open ----
    function play() {
        if (!sel) return
        app.previewTransition(sel.id)
        pv.playing = true
        const len = type === "stinger" ? (probe && probe.ok ? probe.durationMs : 3000) : type === "cut" ? 0 : sel.durationMs
        pv.stopAt = Date.now() + len + 900 // hold on the new scene a moment
    }
    onOpened: { selId = app.currentTransition; probeAll(); Qt.callLater(play) }
    onClosed: { pv.playing = false; app.stopTransitionPreview() }
    onStChanged: if (visible) { replay.restart(); if (type === "stinger" && st.path) app.probeVideo(st.path, switchMs) }
    Timer { id: replay; interval: 150; onTriggered: dlg.play() }

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap }
    component FieldLabel: Text {
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
        Layout.preferredWidth: 120; Layout.alignment: Qt.AlignTop; Layout.topMargin: (Pal.ctl - font.pixelSize) / 2 - 2
    }
    component Heading: Text {
        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 600
        Layout.columnSpan: 2; Layout.topMargin: 8
    }

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
                    id: row
                    required property var modelData
                    readonly property bool isSel: dlg.sel && modelData.id === dlg.sel.id
                    readonly property bool inUse: modelData.id === app.currentTransition
                    readonly property string problem: { dlg.probes; return dlg.problem(modelData) }
                    width: ListView.view.width
                    height: 34
                    radius: Pal.radius
                    color: isSel ? Pal.selected : rowMouse.containsMouse ? Pal.hover : "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 10; rightMargin: 8 }
                        spacing: 8
                        FIcon { name: row.problem ? "triangle-alert" : (dlg.typeIcons[row.modelData.type] || "blend"); size: 14; color: row.problem ? Pal.danger : Pal.muted }
                        Text { text: row.modelData.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight; Layout.fillWidth: true }
                        FIcon { visible: row.inUse; name: "check"; size: 14; color: Pal.fg }
                    }
                    MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { dlg.selId = row.modelData.id; dlg.play() } }
                    ToolTip.visible: rowMouse.containsMouse && (row.problem !== "" || row.inUse)
                    ToolTip.delay: 500
                    ToolTip.text: row.problem || "Used for Show"
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
            // Header: what it is, its name, and whether Show uses it.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                FIcon { name: dlg.typeIcons[dlg.type] || "blend"; size: 18; color: Pal.fg }
                TextField { // renamed in place: reads as a title until you click it
                    id: nameField
                    Layout.fillWidth: true
                    implicitHeight: Pal.ctl
                    text: dlg.sel ? dlg.sel.name : ""
                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 600
                    leftPadding: 6; rightPadding: 6
                    selectByMouse: true; selectionColor: Pal.focus
                    Accessible.name: "Transition name"
                    background: Rectangle {
                        radius: Pal.radius
                        color: nameField.activeFocus ? Pal.s1 : "transparent"
                        border.width: nameField.activeFocus || nameField.hovered ? 1 : 0
                        border.color: nameField.activeFocus ? Pal.focus : Pal.border
                    }
                    onEditingFinished: if (dlg.sel && text.trim() !== "" && text !== dlg.sel.name) dlg.set("name", text.trim())
                }
                FBadge { visible: dlg.inUse; text: "Used for Show"; tone: "neutral" }
                FButton {
                    visible: !dlg.inUse
                    text: "Use for Show"; variant: "primary"
                    tip: "Show plays this one when it switches scenes"
                    onClicked: app.setCurrentTransition(dlg.sel.id)
                }
                FButton {
                    id: moreBtn
                    iconName: "ellipsis"; variant: "ghost"; tip: "More"
                    onClicked: moreMenu.popup(moreBtn, 0, moreBtn.height + 4)
                    FMenu {
                        id: moreMenu
                        FMenuItem {
                            text: "Delete transition"
                            enabled: app.transitions.length > 1
                            onTriggered: { const id = dlg.sel.id; app.removeTransition(id); dlg.selId = app.currentTransition }
                        }
                    }
                }
            }
            Caption { Layout.fillWidth: true; text: dlg.typeHelp[dlg.type] || "" }

            // Preview on two sample scenes, looping.
            Rectangle {
                Layout.preferredWidth: Math.min(parent.width, 300 * 16 / 9)
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: Layout.preferredWidth * 9 / 16
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
                        onTriggered: { pv.tick++; if (Date.now() > pv.stopAt) dlg.play() }
                    }
                }
                Rectangle { // what you're looking at
                    anchors { left: parent.left; top: parent.top; margins: 8 }
                    radius: Pal.radius; color: "#99000000"
                    implicitWidth: sampleText.implicitWidth + 14; implicitHeight: sampleText.implicitHeight + 8
                    Text { id: sampleText; anchors.centerIn: parent; text: "Preview · sample scenes A and B"; color: "white"; font.family: Pal.font; font.pixelSize: Pal.caption }
                }
            }

            FScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                GridLayout {
                    width: parent.width
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 10

                    // ---- duration (everything but Cut and Stinger) ----
                    FieldLabel { visible: dlg.type !== "cut" && dlg.type !== "stinger"; text: "Duration" }
                    FFillSlider {
                        visible: dlg.type !== "cut" && dlg.type !== "stinger"
                        Layout.fillWidth: true
                        from: 100; to: 3000; decimals: 0
                        value: dlg.sel ? dlg.sel.durationMs : 300
                        format: (v) => v >= 1000 ? (v / 1000).toFixed(2) + " s" : Math.round(v) + " ms"
                        onReleased: (v) => dlg.set("durationMs", Math.round(v / 50) * 50)
                    }

                    // ---- swipe / slide ----
                    FieldLabel { visible: dlg.type === "swipe" || dlg.type === "slide"; text: "Direction" }
                    RowLayout {
                        visible: dlg.type === "swipe" || dlg.type === "slide"
                        spacing: 4
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
                    Item { visible: dlg.type === "swipe"; implicitWidth: 1 }
                    FCheckbox {
                        visible: dlg.type === "swipe"
                        text: "Swipe in (the new scene moves over the old one)"
                        checked: !!dlg.st.swipe_in
                        onToggled: dlg.set("swipe_in", checked)
                    }

                    // ---- fade to color ----
                    FieldLabel { visible: dlg.type === "fade_to_color"; text: "Color" }
                    FColorField { visible: dlg.type === "fade_to_color"; Layout.fillWidth: true; color: dlg.st.color || "#000000"; onPicked: (c) => dlg.set("color", c) }
                    FieldLabel { visible: dlg.type === "fade_to_color"; text: "Switch point" }
                    FFillSlider {
                        visible: dlg.type === "fade_to_color"
                        Layout.fillWidth: true
                        from: 0; to: 100; decimals: 0
                        value: dlg.st.switch_point === undefined ? 50 : dlg.st.switch_point
                        format: (v) => Math.round(v) + "% through"
                        onReleased: (v) => dlg.set("switch_point", Math.round(v))
                    }

                    // ---- luma wipe ----
                    FieldLabel { visible: dlg.type === "luma_wipe"; text: "Pattern" }
                    FSelect {
                        visible: dlg.type === "luma_wipe"
                        Layout.fillWidth: true
                        options: app.lumaWipes().map(f => ({ name: f.replace(/\.png$/, "").replace(/-/g, " "), value: f }))
                        currentValue: dlg.st.luma_image || "linear-h.png"
                        onPicked: (v) => dlg.set("luma_image", v)
                    }
                    FieldLabel { visible: dlg.type === "luma_wipe"; text: "Softness" }
                    FFillSlider {
                        visible: dlg.type === "luma_wipe"
                        Layout.fillWidth: true
                        from: 0; to: 1; decimals: 2
                        value: dlg.st.luma_softness === undefined ? 0.03 : dlg.st.luma_softness
                        onReleased: (v) => dlg.set("luma_softness", v)
                    }
                    Item { visible: dlg.type === "luma_wipe"; implicitWidth: 1 }
                    FCheckbox {
                        visible: dlg.type === "luma_wipe"
                        text: "Invert"
                        checked: !!dlg.st.luma_invert
                        onToggled: dlg.set("luma_invert", checked)
                    }

                    // ---- stinger: the video ----
                    FieldLabel { visible: dlg.type === "stinger"; text: "Video" }
                    Rectangle {
                        id: videoCard
                        visible: dlg.type === "stinger"
                        Layout.fillWidth: true
                        readonly property string problem: { dlg.probes; return dlg.sel ? dlg.problem(dlg.sel) : "" }
                        readonly property bool ready: !!dlg.probe && dlg.probe.ok
                        implicitHeight: 72
                        radius: Pal.radius
                        color: Pal.s1
                        border.width: 1
                        border.color: problem ? Pal.danger : Pal.border
                        RowLayout {
                            anchors { fill: parent; margins: 8 }
                            spacing: 10
                            Rectangle { // the frame where the video covers the screen, on a checkerboard
                                Layout.preferredWidth: 96; Layout.preferredHeight: 54
                                radius: Pal.radius - 2; clip: true
                                color: "#2a2a2a"
                                Grid {
                                    anchors.fill: parent; columns: 12
                                    Repeater { model: 12 * 7; Rectangle { width: 8; height: 8; color: (index % 12 + Math.floor(index / 12)) % 2 ? "#3a3a3a" : "#2a2a2a" } }
                                }
                                Image {
                                    anchors.fill: parent
                                    visible: videoCard.ready
                                    fillMode: Image.PreserveAspectFit
                                    cache: false
                                    source: videoCard.ready ? "image://thumbs/probe:" + encodeURIComponent(dlg.st.path) + "?" + dlg.frameTick : ""
                                }
                                FIcon { anchors.centerIn: parent; visible: !videoCard.ready; name: videoCard.problem ? "triangle-alert" : "film"; size: 18; color: videoCard.problem ? Pal.danger : Pal.muted }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    Layout.fillWidth: true; elide: Text.ElideMiddle
                                    text: dlg.st.path ? dlg.st.path.split(/[\\/]/).pop() : "No video chosen"
                                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550
                                }
                                Text {
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                    text: videoCard.problem ? videoCard.problem
                                        : videoCard.ready ? (dlg.probe.durationMs / 1000).toFixed(2) + " s · " + (dlg.probe.alpha ? "Transparent" : "Not transparent: covers the screen") + " · " + dlg.probe.w + "×" + dlg.probe.h
                                        : dlg.st.path ? "Reading the video…" : "A video with transparency (.webm or .mov)"
                                    color: videoCard.problem ? Pal.danger : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                                }
                            }
                            FButton {
                                text: videoCard.problem === "Video not found" ? "Find video…" : dlg.st.path ? "Change…" : "Choose…"
                                variant: videoCard.problem ? "primary" : "secondary"
                                tip: dlg.st.path || ""
                                onClicked: stingerPicker.open()
                            }
                        }
                    }

                    // ---- stinger: where the scenes switch, on the video's own timeline ----
                    FieldLabel { visible: dlg.type === "stinger"; text: "Switch scenes at" }
                    ColumnLayout {
                        visible: dlg.type === "stinger"
                        Layout.fillWidth: true
                        spacing: 4
                        readonly property int len: videoCard.ready ? dlg.probe.durationMs : 5000
                        Item {
                            id: timeline
                            Layout.fillWidth: true
                            implicitHeight: Pal.ctl
                            readonly property int len: parent.len
                            property real dragMs: -1
                            readonly property real ms: dragMs >= 0 ? dragMs : Math.min(dlg.switchMs, len)
                            readonly property real frac: len > 0 ? ms / len : 0
                            Rectangle { // the video, start to end
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                                height: 8; radius: 4; color: Pal.s3
                                Rectangle { width: parent.width * timeline.frac; height: parent.height; radius: 4; color: Pal.s6 }
                            }
                            Rectangle { // the switch point
                                x: timeline.width * timeline.frac - width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 4; height: parent.height - 6; radius: 2
                                color: Pal.fg
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.SizeHorCursor
                                function at(x) { return Math.round(Math.max(0, Math.min(1, x / width)) * timeline.len / 10) * 10 }
                                onPressed: (m) => timeline.dragMs = at(m.x)
                                onPositionChanged: (m) => { if (pressed) timeline.dragMs = at(m.x) }
                                onReleased: { dlg.set("transition_point", Math.round(timeline.dragMs)); timeline.dragMs = -1 }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Caption { text: "Start" }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: Math.round(timeline.ms) + " ms" + (videoCard.ready ? " · " + Math.round(timeline.frac * 100) + "% through" : "")
                                color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption
                            }
                            Item { Layout.fillWidth: true }
                            Caption { text: videoCard.ready ? (timeline.len / 1000).toFixed(2) + " s" : "5 s" }
                        }
                    }

                    // ---- stinger: sound ----
                    Heading { visible: dlg.type === "stinger"; text: "Sound" }
                    FieldLabel { visible: dlg.type === "stinger"; text: "Video's sound" }
                    FTabs {
                        visible: dlg.type === "stinger"
                        readonly property var values: [2, 1, 0]
                        model: ["Viewers hear it", "Only you", "Off"]
                        currentIndex: Math.max(0, values.indexOf(dlg.st.audio_monitoring === undefined ? 0 : dlg.st.audio_monitoring))
                        onActivated: (i) => dlg.set("audio_monitoring", values[i])
                    }
                    FieldLabel { visible: dlg.type === "stinger"; text: "Scenes' sound" }
                    FTabs {
                        visible: dlg.type === "stinger"
                        model: ["Fade out, then in", "Crossfade"]
                        currentIndex: dlg.st.audio_fade_style === 1 ? 1 : 0
                        onActivated: (i) => dlg.set("audio_fade_style", i)
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
        FButton { text: "Done"; variant: "primary"; onClicked: dlg.close(); Layout.rightMargin: 16; Layout.bottomMargin: 14 }
    }
}
