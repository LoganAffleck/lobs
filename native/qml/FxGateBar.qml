import QtQuick
import QtQuick.Layouts

// A noise gate's two thresholds on one bar, over the source's live level: it closes below the
// left marker and opens above the right one, so you can see where your voice sits against it.
// Drag a marker, or type either value below.
ColumnLayout {
    id: gb
    required property var close   // fx params: close_threshold, open_threshold
    required property var open
    required property string sourceId
    required property int fxIndex
    spacing: 6

    readonly property real lo: close.min
    readonly property real hi: close.max
    function frac(db) { return Math.max(0, Math.min(1, (db - lo) / (hi - lo))) }
    function dbAt(x, w) { return lo + Math.max(0, Math.min(1, x / w)) * (hi - lo) }
    function set(p, v, final) { app.setAudioFilterValue(sourceId, fxIndex, p.key, Math.round(v * 10) / 10, final) }
    readonly property var lv: app.levels[sourceId] || [-60, -60]
    readonly property real level: Math.max(lv[0], lv[1])

    Rectangle {
        id: track
        Layout.fillWidth: true
        implicitHeight: Pal.ctl + 4
        radius: Pal.radius
        color: Pal.s1
        border.width: 1; border.color: drag.containsMouse || drag.pressed ? Pal.borderStrong : Pal.border
        clip: true
        readonly property color accent: Pal.link
        // Open above the right marker.
        Rectangle {
            x: track.width * gb.frac(gb.open.value); width: track.width - x; height: parent.height
            color: Qt.rgba(track.accent.r, track.accent.g, track.accent.b, 0.18)
        }
        // The live level, under the markers.
        Rectangle {
            width: track.width * gb.frac(gb.level); height: 4; radius: 2
            anchors.bottom: parent.bottom; anchors.bottomMargin: 4
            color: gb.level >= gb.open.value ? Pal.link : Pal.muted
        }
        Repeater {
            model: [gb.close, gb.open]
            delegate: Rectangle {
                required property var modelData
                x: track.width * gb.frac(modelData.value) - 1.5
                y: 4; width: 3; height: parent.height - 8; radius: 1.5
                color: Pal.fg
            }
        }
        MouseArea {
            id: drag
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property var which: null // the marker being dragged
            onPressed: (m) => {
                const v = gb.dbAt(m.x, width)
                which = Math.abs(v - gb.close.value) <= Math.abs(v - gb.open.value) ? gb.close : gb.open
                move(m.x, false)
            }
            onPositionChanged: (m) => { if (pressed) move(m.x, false) }
            onReleased: (m) => { move(m.x, true); which = null }
            function move(x, final) {
                if (!which) return
                let v = gb.dbAt(x, width)
                // The close marker stays at or below the open one.
                v = which === gb.close ? Math.min(v, gb.open.value) : Math.max(v, gb.close.value)
                gb.set(which, v, final)
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        FNumField {
            Layout.fillWidth: true
            label: "Close"; suffix: "dB"; step: 0.1; live: true; Layout.minimumWidth: 60
            from: gb.lo; to: gb.open.value
            value: Number(gb.close.value.toFixed(1))
            onCommit: (v) => gb.set(gb.close, v, true)
            onScrubbed: (v, final) => gb.set(gb.close, v, final)
        }
        FNumField {
            Layout.fillWidth: true
            label: "Open"; suffix: "dB"; step: 0.1; live: true; Layout.minimumWidth: 60
            from: gb.close.value; to: gb.hi
            value: Number(gb.open.value.toFixed(1))
            onCommit: (v) => gb.set(gb.open, v, true)
            onScrubbed: (v, final) => gb.set(gb.open, v, final)
        }
    }
}
