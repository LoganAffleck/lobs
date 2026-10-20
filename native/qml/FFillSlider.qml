import QtQuick

// Fill slider (the "Volume" style): label left, value right, the control fills as you drag.
// Drag anywhere on it; live values while dragging, final value on release (one undo step).
Item {
    id: s
    property string label
    property real from: 0
    property real to: 1
    property real value: 0
    property int decimals: 2
    property string suffix: ""
    property real displayScale: 1
    property var format: null // optional fn(value) -> string for the value label
    signal moved(real v)
    signal released(real v)
    implicitHeight: Pal.ctl
    implicitWidth: 200
    readonly property real shown: dragging ? dragValue : value
    readonly property real frac: Math.max(0, Math.min(1, (shown - from) / (to - from)))
    property real dragValue: value
    readonly property bool dragging: mouse.pressed
    property bool valueMoving: false
    onValueChanged: { valueMoving = true; valueMoveEnd.restart() }
    Timer { id: valueMoveEnd; interval: Pal.moderate + 40; onTriggered: s.valueMoving = false }

    Rectangle {
        anchors.fill: parent
        radius: Pal.radius
        color: Pal.s1
        border.width: 1
        border.color: mouse.containsMouse || s.dragging ? Pal.borderStrong : Pal.border
        Behavior on border.color { ColorAnimation { duration: Pal.fast } }
        clip: true
        Rectangle {
            width: Math.max(0, parent.width * s.frac)
            height: parent.height
            radius: Pal.radius
            color: Pal.s6
            // Animate value changes only, never layout (a slider appearing in a popup would
            // otherwise grow its fill from zero).
            Behavior on width { enabled: !s.dragging && s.valueMoving; NumberAnimation { duration: Pal.moderate; easing.type: Easing.OutCubic } }
        }
        Rectangle { // thin grip at the fill edge
            x: Math.max(4, parent.width * s.frac - 6)
            anchors.verticalCenter: parent.verticalCenter
            width: 2; height: parent.height * 0.5; radius: 1
            color: Pal.fg; opacity: mouse.containsMouse || s.dragging ? 0.9 : 0.35
            Behavior on opacity { NumberAnimation { duration: Pal.fast } }
        }
    }
    Text { text: s.label; x: 10; anchors.verticalCenter: parent.verticalCenter; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body }
    Text {
        anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
        text: s.format ? s.format(s.shown) : (s.shown * s.displayScale).toFixed(s.decimals) + s.suffix
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        function at(x) { return s.from + Math.max(0, Math.min(1, x / width)) * (s.to - s.from) }
        onPressed: (m) => { s.dragValue = at(m.x); s.moved(s.dragValue) }
        onPositionChanged: (m) => { if (pressed) { s.dragValue = at(m.x); s.moved(s.dragValue) } }
        onReleased: s.released(s.dragValue)
        onCanceled: s.released(s.dragValue) // e.g. focus lost mid-drag: still close the edit
    }
}
