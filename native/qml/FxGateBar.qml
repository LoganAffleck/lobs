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
    readonly property real level: Math.max(lv[0], lv[1]) <= -59 ? lo : Math.max(lv[0], lv[1]) // the meters bottom out at -60: that's silence, not a level

    Rectangle {
        id: track
        Layout.fillWidth: true
        implicitHeight: Pal.ctl + 8
        radius: Pal.radius
        color: Pal.s1
        border.width: 1; border.color: drag.containsMouse || drag.pressed ? Pal.borderStrong : Pal.border
        clip: true
        readonly property color accent: Pal.link
        readonly property real xClose: track.width * gb.frac(gb.close.value)
        readonly property real xOpen: track.width * gb.frac(gb.open.value)
        // The live level fills the bar like a meter: blue while the gate is open, grey while closed.
        Rectangle {
            width: track.width * gb.frac(gb.level); height: parent.height
            color: gb.level >= gb.open.value ? Qt.rgba(track.accent.r, track.accent.g, track.accent.b, 0.35) : Pal.s4
            Behavior on width { NumberAnimation { duration: 60 } }
        }
        // The two zones, named where there's room: closed below the left handle, open above the right.
        Text {
            visible: track.xClose > implicitWidth + 16
            anchors.verticalCenter: parent.verticalCenter; x: 10
            text: "Closed"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        }
        Rectangle { x: track.xOpen; width: track.width - x; height: parent.height; color: Qt.rgba(track.accent.r, track.accent.g, track.accent.b, 0.10) }
        Text {
            visible: track.width - track.xOpen > implicitWidth + 16
            anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right; anchors.rightMargin: 10
            text: "Open"; color: Pal.link; font.family: Pal.font; font.pixelSize: Pal.caption
        }
        // The handles: a knob for each threshold.
        Repeater {
            model: [gb.close, gb.open]
            delegate: Rectangle {
                required property var modelData
                required property int index
                x: track.width * gb.frac(modelData.value) - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 10; height: parent.height - 6; radius: 5
                color: Pal.fg
                border.width: 1; border.color: Pal.s1
                Rectangle { anchors.centerIn: parent; width: 2; height: parent.height * 0.4; radius: 1; color: Pal.s3 } // grip
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
