import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "AudioMath.js" as AudioMath

// An audio source's settings: volume, where it goes (stream, recording), monitoring, then
// balance, sync, mono and extra tracks under Advanced, warnings and its effect chain. In the Inspector when an audio source is selected (in Sources or the mixer).
ColumnLayout {
    id: au
    required property var s // an app.mixer entry
    spacing: 10
    readonly property bool monitorOnly: au.s.monitor === "monitor_only"
    readonly property bool onStream: !!(au.s.tracks && au.s.tracks[0])
    readonly property bool inRecording: au.s.inRecording !== false
    // A tile that's on or off: see-through blue when on, plain and muted when off.
    component OutTile: Rectangle {
        id: tile
        property string iconName
        property string text
        property bool on
        signal toggled()
        readonly property bool hoverOn: tileMouse.containsMouse
        readonly property color accent: Pal.link // a color, so its channels can be read (Pal.link is a string)
        Layout.fillWidth: true
        implicitHeight: Pal.ctl + 4
        radius: Pal.radius
        enabled: !au.monitorOnly
        opacity: enabled ? 1 : 0.5
        color: on ? Qt.rgba(accent.r, accent.g, accent.b, tileMouse.containsMouse ? 0.30 : 0.20) : tileMouse.containsMouse ? Pal.hover : "transparent"
        border.width: 1
        border.color: on ? accent : Pal.border
        Behavior on color { ColorAnimation { duration: Pal.fast } }
        Row {
            anchors.centerIn: parent
            spacing: 8
            FIcon { visible: tile.iconName !== ""; anchors.verticalCenter: parent.verticalCenter; name: tile.iconName; size: 14; color: tile.on ? Pal.fg : Pal.muted }
            Text { anchors.verticalCenter: parent.verticalCenter; text: tile.text; color: tile.on ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
        }
        MouseArea { id: tileMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: tile.toggled() }
        Accessible.role: Accessible.CheckBox
        Accessible.name: text
        Accessible.checked: on
    }
    // Mute, the live meter (its handle is the volume, as in the mixer) and the level.
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        FButton {
            iconName: au.s.muted ? "volume-x" : "volume-2"; variant: "ghost"
            selected: !!au.s.muted
            tip: au.s.muted ? "Unmute" : "Mute"
            onClicked: app.setAudio(au.s.id, "muted", !au.s.muted, true)
        }
        MeterFader {
            id: vol
            Layout.fillWidth: true
            implicitHeight: 18
            value: AudioMath.posFromLin(au.s.volume)
            levels: app.levels[au.s.id] || [-60, -60]
            muted: !!au.s.muted
            onMoved: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), false)
            onReleased: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), true)
        }
        Text {
            text: au.s.muted ? "Muted" : AudioMath.toDb(AudioMath.linFromPos(vol.shown)) + " dB"
            color: au.s.muted ? Pal.danger : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight
        }
    }
    // Where it's heard: the stream (track 1) and the recording (its own mix, track 6).
    Text { text: "Active in"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: -6
        spacing: 6
        OutTile {
            iconName: "record"; text: "Recording"
            on: au.inRecording && !au.monitorOnly
            onToggled: app.setAudio(au.s.id, "inRecording", !au.inRecording, true)
        }
        OutTile {
            iconName: "radio-tower"; text: "Stream"
            on: au.onStream && !au.monitorOnly
            onToggled: app.setAudio(au.s.id, "track", 0, true)
        }
    }
    Text {
        visible: au.monitorOnly
        Layout.topMargin: -6
        text: "Monitor only: you hear it, the stream and recordings don't."
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
    }
    // Monitoring, in OBS's words: what you hear in your headphones.
    Text { text: "Monitor"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    FSelect {
        Layout.fillWidth: true
        Layout.topMargin: -6
        options: [{ name: "Off", value: "off" }, { name: "Monitor and output", value: "monitor_and_output" }, { name: "Monitor only", value: "monitor_only" }]
        currentValue: au.s.monitor
        onPicked: (v) => app.setAudio(au.s.id, "monitor", v, true)
    }
    // Set once, if ever: closed, it names what isn't at its default.
    readonly property var advancedChanged: [
        Math.abs((au.s.balance === undefined ? 0.5 : au.s.balance) - 0.5) > 0.001 ? "Balance" : "",
        (au.s.syncMs || 0) !== 0 ? "Sync" : "",
        au.s.mono ? "Mono" : "",
        (au.s.tracks || []).slice(1).some(t => t) ? "Tracks" : "",
        au.s.mutedHere ? "Muted in this scene" : "",
        au.s.pinned ? "Pinned" : ""].filter(t => t)
    property bool advancedOpen: false
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    MouseArea {
        Layout.fillWidth: true
        implicitHeight: advRow.implicitHeight + 4
        cursorShape: Qt.PointingHandCursor
        onClicked: au.advancedOpen = !au.advancedOpen
        RowLayout {
            id: advRow
            anchors.fill: parent
            spacing: 6
            FIcon { name: "sliders-horizontal"; size: 14; color: Pal.muted }
            Text { text: "Advanced"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true }
            Text { visible: !au.advancedOpen; text: au.advancedChanged.join(" · "); color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
            // Same height and edge as Effects' button below, so the two sections read alike.
            FButton { iconName: au.advancedOpen ? "chevron-up" : "chevron-down"; variant: "ghost"; tip: au.advancedOpen ? "Hide" : "Show"; onClicked: au.advancedOpen = !au.advancedOpen }
        }
    }
    // Labels sit outside the controls, as in the Inspector's Position and Size rows.
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Balance"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 72 }
        FFillSlider {
            Layout.fillWidth: true
            from: 0; to: 1
            fillFrom: 0.5 // from the middle out: which side it leans to
            value: au.s.balance === undefined ? 0.5 : au.s.balance
            format: (v) => AudioMath.balanceText(v)
            // Snaps to center near the middle, so it's easy to put back.
            function snap(v) { return Math.abs(v - 0.5) < 0.06 ? 0.5 : v }
            onMoved: (v) => app.setAudio(au.s.id, "balance", snap(v), false)
            onReleased: (v) => app.setAudio(au.s.id, "balance", snap(v), true)
        }
    }
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Sync offset"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 72 }
        FNumField {
            Layout.fillWidth: true
            label: "ms"
            value: au.s.syncMs || 0
            onCommit: (v) => app.setAudio(au.s.id, "sync", Math.round(v), true)
        }
    }
    // An on/off setting: its icon and name, the switch at the end (the row toggles it too).
    component OptionRow: MouseArea {
        id: opt
        property string iconName
        property string text
        property bool checked
        property string tip
        signal toggled()
        visible: au.advancedOpen
        Layout.fillWidth: true
        implicitHeight: Pal.ctl
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: toggled()
        RowLayout {
            anchors.fill: parent
            spacing: 8
            FIcon { name: opt.iconName; size: 14; color: opt.checked ? Pal.fg : Pal.muted }
            Text { text: opt.text; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight; Layout.fillWidth: true }
            FSwitch { checkable: false; checked: opt.checked; onClicked: opt.toggled() }
        }
        ToolTip.visible: containsMouse && tip !== ""
        ToolTip.delay: 500
        ToolTip.text: tip
    }
    OptionRow {
        iconName: "audio-lines"; text: "Mono"
        checked: !!au.s.mono
        tip: "The same mix on both sides. Fixes a mic heard on one side only."
        onToggled: app.setAudio(au.s.id, "mono", !au.s.mono, true)
    }
    // Mics and desktop audio play in every scene; this keeps one quiet while a scene is on air.
    OptionRow {
        visible: au.advancedOpen && !!au.s.global
        iconName: "volume-x"; text: "Always muted in “" + app.editSceneName + "”"
        checked: !!au.s.mutedHere
        tip: "Muted whenever this scene is on air. Other scenes still hear it."
        onToggled: app.setMutedInScene(au.s.id, !au.s.mutedHere)
    }
    OptionRow {
        iconName: "pin"; text: "Pinned to the front of the mixer"
        checked: !!au.s.pinned
        onToggled: app.setMixerPinned(au.s.id, !au.s.pinned)
    }
    Text { visible: au.advancedOpen; text: "Extra tracks"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        Layout.topMargin: -6
        spacing: 6
        Repeater {
            model: (au.s.tracks || []).length > 1 ? au.s.tracks.length - 1 : 0 // 2–5: 1 is the stream (above)
            delegate: OutTile {
                required property int index
                text: String(index + 2)
                on: !!au.s.tracks[index + 1]
                onToggled: app.setAudio(au.s.id, "track", index + 1, true)
                ToolTip.visible: hoverOn
                ToolTip.delay: 500
                ToolTip.text: "Track " + (index + 2) + ", for multitrack recordings and plugins"
            }
        }
    }
    Repeater {
        model: au.s.warnings
        delegate: RowLayout { required property var modelData; Layout.fillWidth: true; spacing: 6
            FIcon { name: "triangle-alert"; color: "#fbbf24"; size: 12; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
            Text { text: modelData; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    // Effects: the source's audio filter chain, top to bottom = processing order. Flat rows, like
    // Advanced: the name and menu on one line, then its slider (or a summary) and switch. One with
    // several settings opens (and closes) with a click.
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        FIcon { name: "wand-sparkles"; size: 14; color: Pal.muted }
        Text { text: "Effects" + (au.s.fx.length ? " · " + au.s.fx.length : ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
        // Gives way first when the panel is narrow.
        Text { text: au.s.fx.length > 1 ? "Runs top to bottom" : ""; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.leftMargin: 4; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
        FButton {
            id: addFx
            iconName: "plus"; text: "Effect"; variant: "secondary"
            tip: "Add an audio effect"
            onClicked: fxMenu.popup(addFx, 0, addFx.height + 4)
            FMenu {
                id: fxMenu
                Instantiator {
                    active: fxMenu.populated // built while the menu is open
                    model: app.audioFxTypes()
                    delegate: FMenuItem { required property var modelData; text: modelData.label; onTriggered: app.addAudioFilter(au.s.id, modelData.type) }
                    onObjectAdded: (i, o) => fxMenu.insertItem(i, o)
                    onObjectRemoved: (i, o) => fxMenu.removeItem(o)
                }
            }
        }
    }
    // Dragging an effect by its grip: it follows the cursor and the others slide aside to show
    // where it lands (as tabs do when they're dragged).
    ColumnLayout {
        id: fxChain
        Layout.fillWidth: true
        spacing: 0
        property int dragIndex: -1
        property real dragDy: 0
        property int dropIndex: -1
        function rowAt(i) { return fxRep.itemAt(i) }
        function updateDrop() {
            const me = rowAt(dragIndex)
            if (!me) return
            const mid = me.y + dragDy + me.height / 2
            let to = dragIndex
            for (let i = 0; i < fxRep.count; i++) {
                const r = rowAt(i)
                if (!r || i === dragIndex) continue
                const c = r.y + r.height / 2
                if (i < dragIndex && mid < c) to = Math.min(to, i)
                if (i > dragIndex && mid > c) to = Math.max(to, i)
            }
            dropIndex = to
        }
        function finish() {
            const from = dragIndex, to = dropIndex
            dragIndex = -1; dropIndex = -1; dragDy = 0
            for (let k = from; to >= 0 && k !== to; k += (to > k ? 1 : -1))
                app.moveAudioFilter(au.s.id, k, to > k ? 1 : -1)
        }
        Repeater {
            id: fxRep
            model: au.s.fx.length // by position, so sliders survive edits
            delegate: ColumnLayout {
                id: fxRow
                required property int index
                readonly property var fx: au.s.fx[fxRow.index] || ({ label: "", enabled: false, params: [], known: true })
                readonly property var params: fx.params || []
                readonly property bool inline: params.length === 1 && !fx.obs && fx.known !== false // its one slider on the row
                readonly property bool expandable: !inline && (params.length > 0 || !!fx.obs || fx.known === false)
                property bool open: false
                function fmt(p) { return p.kind === "percent" ? Math.round(p.value * 100) + "%" : Number(p.value).toFixed(p.decimals) + p.suffix }
                readonly property bool dragged: fxChain.dragIndex === index
                // While another row is dragged past this one, it slides a row's height out of the way.
                readonly property real shift: {
                    const d = fxChain.dragIndex, t = fxChain.dropIndex, me = fxChain.rowAt(d)
                    if (d < 0 || t < 0 || dragged || !me) return 0
                    if (d < t && index > d && index <= t) return -me.height
                    if (d > t && index < d && index >= t) return me.height
                    return 0
                }
                Layout.fillWidth: true
                spacing: 8
                z: dragged ? 2 : 0
                opacity: fx.enabled ? 1 : 0.55
                transform: Translate {
                    y: fxRow.dragged ? fxChain.dragDy : fxRow.shift
                    Behavior on y { enabled: !fxRow.dragged; NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border; visible: fxRow.index > 0 && !fxRow.dragged }
                Rectangle { // the row being dragged lifts off the list
                    Layout.fillWidth: true
                    implicitHeight: head.implicitHeight
                    radius: Pal.radius
                    color: fxRow.dragged ? Pal.s3 : "transparent"
                    border.width: fxRow.dragged ? 1 : 0; border.color: Pal.borderStrong
                    // Line 1: grip, the effect's name across the row, its menu. Line 2: its one slider
                    // (or, with several settings, a summary that opens them) and its switch.
                    MouseArea {
                        id: head
                        anchors.fill: parent
                        implicitHeight: headCol.implicitHeight + 8
                        hoverEnabled: true
                        cursorShape: fxRow.expandable ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: if (fxRow.expandable) fxRow.open = !fxRow.open
                        ColumnLayout {
                            id: headCol
                            anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 2; anchors.rightMargin: 2
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                FIcon {
                                    name: "grip-vertical"; size: 14
                                    color: grip.containsMouse || grip.pressed ? Pal.fg : Pal.muted
                                    visible: au.s.fx.length > 1
                                    MouseArea {
                                        id: grip
                                        anchors.fill: parent; anchors.margins: -6
                                        hoverEnabled: true
                                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                        preventStealing: true
                                        property real startY: 0
                                        onPressed: (m) => { startY = mapToItem(fxChain, m.x, m.y).y; fxChain.dragIndex = fxRow.index; fxChain.dropIndex = fxRow.index }
                                        onPositionChanged: (m) => { if (pressed) { fxChain.dragDy = mapToItem(fxChain, m.x, m.y).y - startY; fxChain.updateDrop() } }
                                        onReleased: fxChain.finish()
                                        onCanceled: { fxChain.dragIndex = -1; fxChain.dropIndex = -1; fxChain.dragDy = 0 }
                                    }
                                }
                                Text {
                                    text: fxRow.fx.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                FButton {
                                    id: fxMore
                                    iconName: "ellipsis"; variant: "ghost"; tip: "More"
                                    onClicked: rowMenu.popup(fxMore, 0, fxMore.height + 4)
                                    FMenu {
                                        id: rowMenu
                                        FMenuItem { text: "Move up"; enabled: fxRow.index > 0; onTriggered: app.moveAudioFilter(au.s.id, fxRow.index, -1) }
                                        FMenuItem { text: "Move down"; enabled: fxRow.index < au.s.fx.length - 1; onTriggered: app.moveAudioFilter(au.s.id, fxRow.index, 1) }
                                        FMenuItem { text: "Remove"; onTriggered: app.removeAudioFilter(au.s.id, fxRow.index) }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: au.s.fx.length > 1 ? 20 : 0 // under the name, past the grip
                                spacing: 8
                                // One setting: its control, unlabeled (the effect's name says it).
                                FxControl {
                                    visible: fxRow.inline
                                    Layout.fillWidth: true
                                    p: fxRow.params[0] || ({ key: "", label: "", suffix: "", min: 0, max: 1, decimals: 0, def: 0, kind: "level", presets: [], value: 0 })
                                    sourceId: au.s.id; fxIndex: fxRow.index
                                    showLabel: false
                                }
                                // Several: what they're set to, and the way to open them.
                                Text {
                                    visible: !fxRow.inline
                                    Layout.fillWidth: true
                                    text: fxRow.expandable && !fxRow.open ? fxRow.params.slice(0, 2).map(p => fxRow.fmt(p)).join(" · ") : ""
                                    elide: Text.ElideRight
                                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                                }
                                FButton {
                                    visible: fxRow.expandable
                                    iconName: fxRow.open ? "chevron-up" : "chevron-down"; variant: "ghost"
                                    tip: fxRow.open ? "Close" : "Open"
                                    onClicked: fxRow.open = !fxRow.open
                                }
                                FSwitch { checkable: false; checked: fxRow.fx.enabled; onClicked: app.setAudioFilterEnabled(au.s.id, fxRow.index, !fxRow.fx.enabled) }
                            }
                        }
                    }
                }
                // Open: its settings, each labeled (there are several), labels outside the controls.
                // A noise gate's thresholds share one bar over the live level; an EQ gets faders.
                readonly property bool gate: fx.type === "noise_gate" && params.length >= 2
                readonly property bool eq: fx.type === "eq"
                FxGateBar {
                    visible: fxRow.open && fxRow.gate
                    Layout.fillWidth: true; Layout.leftMargin: au.s.fx.length > 1 ? 20 : 0
                    close: fxRow.params[0] || ({ key: "", min: -96, max: 0, value: -32 })
                    open: fxRow.params[1] || ({ key: "", min: -96, max: 0, value: -26 })
                    sourceId: au.s.id; fxIndex: fxRow.index
                }
                FxEqFaders {
                    visible: fxRow.open && fxRow.eq
                    Layout.fillWidth: true; Layout.leftMargin: au.s.fx.length > 1 ? 20 : 0
                    bands: fxRow.eq ? fxRow.params : []
                    sourceId: au.s.id; fxIndex: fxRow.index
                }
                Repeater {
                    model: fxRow.open && !fxRow.inline && !fxRow.eq ? fxRow.params.length : 0
                    delegate: FxControl {
                        required property int index
                        visible: !(fxRow.gate && index < 2) // the gate's thresholds are on its bar
                        Layout.fillWidth: true; Layout.leftMargin: au.s.fx.length > 1 ? 20 : 0
                        p: fxRow.params[index] || ({ key: "", label: "", suffix: "", min: 0, max: 1, decimals: 0, def: 0, kind: "level", presets: [], value: 0 })
                        sourceId: au.s.id; fxIndex: fxRow.index
                    }
                }
                ObsProperties {
                    visible: fxRow.open && !!fxRow.fx.obs
                    active: visible
                    Layout.leftMargin: 22
                    scope: "audio"; targetId: au.s.id; index: fxRow.index
                }
                Text {
                    visible: fxRow.open && fxRow.fx.known === false
                    Layout.fillWidth: true; Layout.leftMargin: au.s.fx.length > 1 ? 20 : 0; wrapMode: Text.WordWrap
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: "Imported from OBS. No controls in LOBS yet."
                }
                Item { implicitHeight: 2 }
            }
        }
    }
}
