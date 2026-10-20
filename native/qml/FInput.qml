import QtQuick
import QtQuick.Controls

// Text field: sits one step below its surface, 12% border, blue focus ring.
TextField {
    id: f
    implicitHeight: Pal.ctl
    font.family: Pal.font
    font.pixelSize: Pal.body
    color: Pal.fg
    placeholderTextColor: Pal.muted
    selectionColor: Pal.focus
    selectedTextColor: "white"
    selectByMouse: true
    leftPadding: 8
    rightPadding: 8
    verticalAlignment: TextInput.AlignVCenter
    background: Rectangle {
        radius: Pal.radius
        color: Pal.s1
        border.width: 1
        border.color: f.activeFocus ? Pal.focus : f.hovered ? Pal.borderStrong : Pal.border
        Behavior on border.color { ColorAnimation { duration: Pal.fast } }
    }
}
