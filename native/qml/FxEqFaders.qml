import QtQuick
import QtQuick.Layouts

// A 3-band EQ as three short faders side by side, each filling up or down from 0 dB. Drag one
// (it snaps to 0 near the middle), double-click to reset it, or type its value underneath.
RowLayout {
    id: eq
    required property var bands // fx params: low, mid, high
    required property string sourceId
    required property int fxIndex
    spacing: 14

    Item { Layout.fillWidth: true }
    Repeater {
        model: eq.bands
        delegate: ColumnLayout {
            id: band
            required property var modelData
            readonly property var p: modelData
            spacing: 4
            function set(v, final) {
                const snapped = Math.abs(v) < (p.max - p.min) * 0.03 ? 0 : v
                app.setAudioFilterValue(eq.sourceId, eq.fxIndex, p.key, Math.round(Math.max(p.min, Math.min(p.max, snapped)) * 10) / 10, final)
            }
            Rectangle {
                id: track
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 26; implicitHeight: 96
                radius: Pal.radius
                color: Pal.s1
                border.width: 1; border.color: fader.containsMouse || fader.pressed ? Pal.borderStrong : Pal.border
                readonly property real zeroY: height * band.p.max / (band.p.max - band.p.min)
                readonly property real valY: height * (band.p.max - band.p.value) / (band.p.max - band.p.min)
                readonly property color accent: Pal.link
                Rectangle { x: 4; width: parent.width - 8; y: track.zeroY; height: 1; color: Pal.muted } // 0 dB
                Rectangle { // from 0 dB to the value
                    x: 1; width: parent.width - 2
                    y: Math.min(track.zeroY, track.valY); height: Math.abs(track.valY - track.zeroY)
                    color: Qt.rgba(track.accent.r, track.accent.g, track.accent.b, 0.35)
                }
                Rectangle { x: 3; width: parent.width - 6; y: track.valY - 1.5; height: 3; radius: 1.5; color: Pal.fg } // the handle
                MouseArea {
                    id: fader
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SizeVerCursor
                    function at(y) { return band.p.max - Math.max(0, Math.min(1, y / height)) * (band.p.max - band.p.min) }
                    onPressed: (m) => band.set(at(m.y), false)
                    onPositionChanged: (m) => { if (pressed) band.set(at(m.y), false) }
                    onReleased: (m) => band.set(at(m.y), true)
                    onDoubleClicked: band.set(0, true)
                }
            }
            Text { text: band.p.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.alignment: Qt.AlignHCenter }
            FNumField {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 72
                suffix: "dB"; step: 0.1; live: true
                from: band.p.min; to: band.p.max
                value: Number(band.p.value.toFixed(1))
                onCommit: (v) => band.set(v, true)
                onScrubbed: (v, final) => band.set(v, final)
            }
        }
    }
    Item { Layout.fillWidth: true }
}
