import QtQuick

// Level meter and fader in one control, the way OBS packs them: a stereo peak meter fills
// the track (green, amber, red zones) and the fader handle sits on top. Horizontal or
// vertical. Drag or wheel to change; live values while dragging, final value on release.
Item {
    id: mf
    property bool vertical: false
    property real value: 0                 // fader position 0..1 (-60..0 dB)
    property var levels: [-60, -60]        // peak dBFS, left/right
    property bool muted: false
    signal moved(real v)
    signal released(real v)
    readonly property bool dragging: mouse.pressed
    property real dragValue: value
    readonly property real shown: dragging ? dragValue : value
    implicitWidth: vertical ? 26 : 160
    implicitHeight: vertical ? 120 : 16

    function frac(db) { return Math.max(0, Math.min(1, (db + 60) / 60)) }
    readonly property real len: vertical ? height : width
    readonly property real thick: vertical ? width : height

    // Track background with zone tints so the scale reads at a glance.
    Rectangle {
        anchors.fill: parent
        radius: 3
        color: Pal.s1
        border.width: 1; border.color: mouse.containsMouse || mf.dragging ? Pal.borderStrong : Pal.border
        Behavior on border.color { ColorAnimation { duration: Pal.fast } }
    }
    // Stereo meter: two bars across the track's thickness.
    Repeater {
        model: 2
        delegate: Item {
            id: bar
            required property int index
            readonly property real lvl: mf.frac(mf.levels[index] || -60)
            readonly property real gap: 3
            readonly property real barThick: (mf.thick - gap * 3) / 2
            x: mf.vertical ? gap + index * (barThick + gap) : 2
            y: mf.vertical ? 2 : gap + index * (barThick + gap)
            width: mf.vertical ? barThick : mf.width - 4
            height: mf.vertical ? mf.height - 4 : barThick
            clip: true
            opacity: mf.muted ? 0.3 : 1
            // Unlit zones, always visible faintly, so the scale reads even in silence (like OBS).
            Rectangle {
                anchors.fill: parent
                radius: 1
                opacity: 0.16
                gradient: Gradient {
                    orientation: mf.vertical ? Gradient.Vertical : Gradient.Horizontal
                    GradientStop { position: mf.vertical ? 1.0 : 0.0; color: "#16a34a" }
                    GradientStop { position: mf.vertical ? 0.31 : 0.69; color: "#22c55e" }
                    GradientStop { position: mf.vertical ? 0.30 : 0.70; color: "#f59e0b" }
                    GradientStop { position: mf.vertical ? 0.11 : 0.89; color: "#f59e0b" }
                    GradientStop { position: mf.vertical ? 0.10 : 0.90; color: "#ef4444" }
                    GradientStop { position: mf.vertical ? 0.0 : 1.0; color: "#ef4444" }
                }
            }
            // Full-length gradient, revealed up to the level (so colors stay put, like a real meter).
            Item {
                anchors.fill: parent
                Rectangle {
                    width: mf.vertical ? parent.width : parent.width * bar.lvl
                    height: mf.vertical ? parent.height * bar.lvl : parent.height
                    anchors.bottom: mf.vertical ? parent.bottom : undefined
                    radius: 1
                    clip: true
                    Rectangle { // the zones: -60..-18 green, -18..-6 amber, -6..0 red
                        width: mf.vertical ? parent.width : mf.width - 4
                        height: mf.vertical ? mf.height - 4 : parent.height
                        anchors.bottom: mf.vertical ? parent.bottom : undefined
                        gradient: Gradient {
                            orientation: mf.vertical ? Gradient.Vertical : Gradient.Horizontal
                            GradientStop { position: mf.vertical ? 1.0 : 0.0; color: "#16a34a" }
                            GradientStop { position: mf.vertical ? 0.31 : 0.69; color: "#22c55e" }
                            GradientStop { position: mf.vertical ? 0.30 : 0.70; color: "#f59e0b" }
                            GradientStop { position: mf.vertical ? 0.11 : 0.89; color: "#f59e0b" }
                            GradientStop { position: mf.vertical ? 0.10 : 0.90; color: "#ef4444" }
                            GradientStop { position: mf.vertical ? 0.0 : 1.0; color: "#ef4444" }
                        }
                    }
                    Behavior on width { enabled: !mf.vertical; NumberAnimation { duration: 60 } }
                    Behavior on height { enabled: mf.vertical; NumberAnimation { duration: 60 } }
                }
            }
        }
    }
    // Fader handle: a light bar across the track at the volume position.
    Rectangle {
        readonly property real pos: mf.shown * (mf.len - 6)
        x: mf.vertical ? -2 : pos
        y: mf.vertical ? mf.height - 6 - pos : -2
        width: mf.vertical ? mf.width + 4 : 6
        height: mf.vertical ? 6 : mf.height + 4
        radius: 3
        color: mf.dragging || mouse.containsMouse ? "#ffffff" : "#e5e5e5"
        border.width: 1; border.color: "#66000000"
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.margins: -4
        hoverEnabled: true
        cursorShape: mf.vertical ? Qt.SizeVerCursor : Qt.SizeHorCursor
        preventStealing: true
        function at(m) {
            const p = mf.vertical ? 1 - (m.y - 4) / (mf.height) : (m.x - 4) / (mf.width)
            return Math.max(0, Math.min(1, p))
        }
        onPressed: (m) => { mf.dragValue = at(m); mf.moved(mf.dragValue) }
        onPositionChanged: (m) => { if (pressed) { mf.dragValue = at(m); mf.moved(mf.dragValue) } }
        onReleased: mf.released(mf.dragValue)
        onCanceled: mf.released(mf.dragValue)
        onDoubleClicked: { mf.dragValue = 1; mf.released(1) } // back to 0 dB
        onWheel: (w) => {
            const v = Math.max(0, Math.min(1, mf.value + (w.angleDelta.y > 0 ? 1 : -1) * 0.02))
            mf.released(v)
        }
    }
}
