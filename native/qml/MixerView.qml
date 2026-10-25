import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "AudioMath.js" as AudioMath

// Audio mixer. Packs into whatever space the panel has, like OBS:
//  - wide and short (a bottom dock): vertical channel strips side by side,
//  - narrow or tall: one compact row per source.
// Each source shows its name, a stereo meter with the fader on it, mute and monitor. Clicking a
// source (or its "...") shows its monitoring, tracks, effects and warnings in the Inspector.
Item {
    id: mv
    function toDb(lin) { return AudioMath.toDb(lin) }
    function posFromLin(lin) { return AudioMath.posFromLin(lin) }
    function linFromPos(p) { return AudioMath.linFromPos(p) }
    // Selecting a source shows its monitoring, tracks and effects in the Inspector.
    signal inspectRequested()
    function inspect(id) { app.selectSource(id); inspectRequested() }
    // app.mixer converts the whole list to JavaScript on every read, so read it once per change:
    // src(i) on a big show used to rebuild it per call and left seconds of garbage (LOBS-025).
    readonly property var mix: app.mixer
    // Same for the meters: app.levels changes 20 times a second, and each strip reading it
    // converted the whole map, every source's levels, per strip (LOBS-025).
    readonly property var lv: app.levels
    // Drag to reorder (strips sideways, rows up and down). Off while the search filters the list.
    property string dragId: ""
    property int dropIndex: -1
    readonly property int dragFrom: dragId === "" ? -1 : mix.findIndex(m => m.id === dragId)
    readonly property bool canReorder: !searchField.query
    function finishDrag() {
        if (dragId !== "" && dropIndex >= 0) app.moveMixer(dragId, dropIndex)
        dragId = ""; dropIndex = -1
    }
    readonly property int count: mix.length
    // Layout: Auto picks strips for a wide, short panel, unless the strips would scroll
    // sideways while rows would fit; Rows / Strips force one (remembered).
    property string layoutMode: app.uiValue("mixer/layout", "auto")
    readonly property real bodyH: height - toolbar.height
    readonly property int shownCount: { let n = 0; for (let i = 0; i < count; ++i) if (matches(src(i))) ++n; return n }
    readonly property bool autoStrips: {
        const wideShort = width > bodyH * 1.3 && bodyH >= 170
        const stripsFit = width >= shownCount * 88 + 12
        const rowsFit = bodyH >= shownCount * 56 + 8
        return wideShort && (stripsFit || !rowsFit)
    }
    readonly property bool strips: layoutMode === "strips" || (layoutMode === "auto" && autoStrips)
    property bool searching: false
    function matches(s) { return !searchField.query || String(s.name || "").toLowerCase().includes(searchField.query) }
    // Flips between what's showing now and the other layout (Auto is only the starting choice).
    function cycleLayout() {
        layoutMode = strips ? "rows" : "strips"
        app.setUiValue("mixer/layout", layoutMode)
    }
    function src(i) { return mix[i] || ({ id: "", name: "", icon: "", color: "#555", volume: 0, muted: false, monitor: "off", tracks: [], global: true, warnings: [], fx: [], mono: false, balance: 0.5, syncMs: 0 }) }
    function monitorIcon(m) { return m === "off" ? "headphone-off" : "headphones" }
    function monitorTip(m) { return m === "off" ? "Not monitored. Click to hear it in your headphones (and keep it in the output)." : m === "monitor_only" ? "Monitor only: you hear it, viewers don't. Click to turn monitoring off." : "Monitored: you hear it, and it's in the output. Click to turn monitoring off." }
    function toggleMonitor(s) { app.setAudio(s.id, "monitor", s.monitor === "off" ? "monitor_and_output" : "off", true) }

    component IconToggle: AbstractButton {
        id: it
        property string iconName
        property color tint: Pal.muted
        property string tip
        implicitWidth: 24; implicitHeight: 24
        hoverEnabled: true
        background: Rectangle { radius: Pal.radius - 2; color: it.down ? Pal.s6 : it.hovered ? Pal.s5 : "transparent" }
        contentItem: Item { FIcon { anchors.centerIn: parent; name: it.iconName; size: 14; color: it.hovered && it.tint === Pal.muted ? Pal.fg : it.tint } }
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: tip
    }
    component Badges: Row {
        property var s
        spacing: 3
        Rectangle {
            visible: s.fx.length > 0
            width: fxt.implicitWidth + 8; height: 14; radius: 7; color: Pal.s5
            anchors.verticalCenter: parent.verticalCenter
            Text { id: fxt; anchors.centerIn: parent; text: "FX " + s.fx.length; color: Pal.muted; font.family: Pal.font; font.pixelSize: 9; font.weight: 600 }
        }
        Repeater {
            model: [s.mono ? "Mono" : "", Math.abs((s.balance === undefined ? 0.5 : s.balance) - 0.5) > 0.005 ? mv.balanceText(s.balance) : "", s.syncMs ? (s.syncMs > 0 ? "+" : "") + s.syncMs + " ms" : ""].filter(t => t)
            delegate: Rectangle {
                required property string modelData
                width: bt.implicitWidth + 8; height: 14; radius: 7; color: Pal.s5
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                Text { id: bt; anchors.centerIn: parent; text: modelData; color: Pal.muted; font.family: Pal.font; font.pixelSize: 9; font.weight: 600 }
            }
        }
        FIcon { visible: s.warnings.length > 0; name: "triangle-alert"; size: 12; color: "#fbbf24"; anchors.verticalCenter: parent.verticalCenter }
    }

    // ---------- toolbar: search, layout ----------
    Item {
        id: toolbar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 34
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 6
            spacing: 2
            FSearchField {
                id: searchField
                visible: mv.searching
                Layout.fillWidth: true
                implicitHeight: 26
                placeholderText: "Search audio"
                onDismissed: { mv.searching = false; text = "" }
            }
            Text {
                visible: !mv.searching
                Layout.fillWidth: true
                text: mv.count === 1 ? "1 input" : mv.count + " inputs"
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            }
            FButton {
                iconName: "search"; variant: "ghost"; selected: mv.searching
                tip: "Search"
                onClicked: { mv.searching = !mv.searching; if (mv.searching) searchField.forceActiveFocus(); else searchField.text = "" }
            }
            FButton {
                // Shows the layout in use: rows, or side-by-side strips (columns).
                iconName: mv.strips ? "columns-3" : "rows-3"
                variant: "ghost"
                tip: mv.strips ? "Strips. Click for rows." : "Rows. Click for strips."
                onClicked: mv.cycleLayout()
            }
        }
    }

    // ---------- rows (narrow / tall) ----------
    FScrollView {
        visible: !mv.strips
        anchors { left: parent.left; right: parent.right; top: toolbar.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: availableWidth
        Column {
            width: parent.width
            topPadding: 4; bottomPadding: 4
            Repeater {
                model: mv.count // by position, so faders aren't recreated mid-drag
                delegate: Item {
                    id: row
                    required property int index
                    readonly property var s: mv.src(index)
                    visible: mv.matches(s) // filtered in place: delegates stay put, so drags aren't interrupted
                    width: parent.width
                    height: 56
                    Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; radius: Pal.radius - 2; color: rowHover.hovered ? Pal.hover : "transparent" }
                    HoverHandler { id: rowHover }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 8; anchors.topMargin: 5; anchors.bottomMargin: 8
                        spacing: 3
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle { width: 6; height: 6; radius: 3; color: row.s.color }
                            FIcon { name: row.s.icon; size: 13; color: row.s.muted ? Pal.muted : Pal.fg }
                            Text {
                                text: row.s.name; color: row.s.muted ? Pal.muted : Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true
                                MouseArea { // drag the name to reorder
                                    anchors.fill: parent
                                    enabled: mv.canReorder
                                    cursorShape: mv.dragId === row.s.id ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    preventStealing: true
                                    onPressed: mv.dragId = row.s.id
                                    onPositionChanged: (m) => { if (pressed) mv.dropIndex = Math.max(0, Math.min(mv.count - 1, Math.floor(mapToItem(row.parent, m.x, m.y).y / row.height))) }
                                    onReleased: { if (mv.dropIndex < 0) mv.inspect(row.s.id); mv.finishDrag() }
                                    onCanceled: { mv.dragId = ""; mv.dropIndex = -1 }
                                }
                            }
                            FIcon { visible: !!row.s.pinned; name: "pin"; size: 12; color: Pal.muted }
                            Badges { s: row.s }
                            Text { text: mv.toDb(row.s.volume) + " dB"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
                            IconToggle { iconName: row.s.muted ? "volume-x" : "volume-2"; tint: row.s.muted ? Pal.danger : Pal.muted; tip: row.s.muted ? "Muted. Click to unmute." : "Mute"; onClicked: app.setAudio(row.s.id, "muted", !row.s.muted, true) }
                            IconToggle { iconName: mv.monitorIcon(row.s.monitor); tint: row.s.monitor === "off" ? Pal.muted : Pal.link; tip: mv.monitorTip(row.s.monitor); onClicked: mv.toggleMonitor(row.s) }
                            IconToggle { id: rowMore; iconName: "ellipsis"; tip: "Monitoring, tracks and effects (in the Inspector)"; onClicked: mv.inspect(row.s.id) }
                        }
                        MeterFader {
                            Layout.fillWidth: true
                            implicitHeight: 14
                            value: mv.posFromLin(row.s.volume)
                            levels: mv.lv[row.s.id] || [-60, -60]
                            muted: row.s.muted
                            onMoved: (v) => app.setAudio(row.s.id, "volume", mv.linFromPos(v), false)
                            onReleased: (v) => app.setAudio(row.s.id, "volume", mv.linFromPos(v), true)
                        }
                    }
                    Rectangle { visible: row.index < mv.count - 1; anchors.bottom: parent.bottom; x: 12; width: parent.width - 24; height: 1; color: Pal.border }
                    opacity: mv.dragId === row.s.id ? 0.5 : 1
                    Rectangle { // where the dragged row lands
                        visible: mv.dragId !== "" && mv.dropIndex === row.index && mv.dragId !== row.s.id
                        x: 8; width: parent.width - 16; height: 2; radius: 1; color: Pal.focus
                        y: mv.dragFrom < row.index ? parent.height - 1 : -1 // below when moving down, above when moving up
                    }
                }
            }
        }
    }

    // ---------- strips (wide / short) ----------
    ScrollView {
        Accessible.role: Accessible.Pane
        Accessible.name: "Audio mixer"
        visible: mv.strips
        anchors { left: parent.left; right: parent.right; top: toolbar.bottom; bottom: parent.bottom }
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        contentHeight: availableHeight
        Row {
            height: parent.height
            leftPadding: 6; topPadding: 6; bottomPadding: 6
            spacing: 4
            Repeater {
                model: mv.count
                delegate: Rectangle {
                    id: strip
                    required property int index
                    readonly property var s: mv.src(index)
                    visible: mv.matches(s)
                    width: 84
                    height: parent.height - 12
                    radius: Pal.radius
                    color: Pal.s3
                    border.width: 1
                    border.color: mv.dragId !== "" && mv.dropIndex === strip.index && mv.dragId !== strip.s.id ? Pal.focus : Pal.border // where it lands
                    opacity: mv.dragId === strip.s.id ? 0.5 : 1
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: strip.s.color }
                            Text {
                                text: strip.s.name; color: strip.s.muted ? Pal.muted : Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true
                                MouseArea { // drag the name to reorder
                                    anchors.fill: parent
                                    enabled: mv.canReorder
                                    cursorShape: mv.dragId === strip.s.id ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    preventStealing: true
                                    onPressed: mv.dragId = strip.s.id
                                    onPositionChanged: (m) => { if (pressed) mv.dropIndex = Math.max(0, Math.min(mv.count - 1, Math.floor((mapToItem(strip.parent, m.x, m.y).x - 6) / (strip.width + 4)))) }
                                    onReleased: { if (mv.dropIndex < 0) mv.inspect(strip.s.id); mv.finishDrag() }
                                    onCanceled: { mv.dragId = ""; mv.dropIndex = -1 }
                                }
                            }
                            FIcon { visible: !!strip.s.pinned; name: "pin"; size: 11; color: Pal.muted }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: mv.toDb(strip.s.volume) + " dB"; color: Pal.muted; font.family: Pal.font; font.pixelSize: 10; Layout.fillWidth: true }
                            Badges { s: strip.s }
                        }
                        MeterFader {
                            vertical: true
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            implicitWidth: 22
                            value: mv.posFromLin(strip.s.volume)
                            levels: mv.lv[strip.s.id] || [-60, -60]
                            muted: strip.s.muted
                            onMoved: (v) => app.setAudio(strip.s.id, "volume", mv.linFromPos(v), false)
                            onReleased: (v) => app.setAudio(strip.s.id, "volume", mv.linFromPos(v), true)
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 0
                            IconToggle { iconName: strip.s.muted ? "volume-x" : "volume-2"; tint: strip.s.muted ? Pal.danger : Pal.muted; tip: strip.s.muted ? "Muted. Click to unmute." : "Mute"; onClicked: app.setAudio(strip.s.id, "muted", !strip.s.muted, true) }
                            IconToggle { iconName: mv.monitorIcon(strip.s.monitor); tint: strip.s.monitor === "off" ? Pal.muted : Pal.link; tip: mv.monitorTip(strip.s.monitor); onClicked: mv.toggleMonitor(strip.s) }
                            IconToggle { id: stripMore; iconName: "ellipsis"; tip: "Monitoring, tracks and effects (in the Inspector)"; onClicked: mv.inspect(strip.s.id) }
                        }
                    }
                }
            }
        }
    }

    Text {
        visible: mv.shownCount === 0
        anchors.centerIn: parent
        width: parent.width - 48
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        text: mv.count && !mv.shownCount ? "No audio inputs match \u201c" + searchField.text + "\u201d." : "No audio sources. Add a Mic or Desktop audio from Sources."
    }
}
