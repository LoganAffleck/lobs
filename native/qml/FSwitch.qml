import QtQuick
import QtQuick.Controls

// Switch: pill track turns blue when on; the thumb slides on the moderate spring.
AbstractButton {
    id: sw
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    checkable: true
    hoverEnabled: true
    implicitHeight: Pal.ctl
    implicitWidth: 32 + (text.length ? lbl.implicitWidth + 8 : 0)
    indicator: Rectangle {
        Accessible.role: Accessible.CheckBox
        Accessible.name: sw.text !== "" ? sw.text : "Switch"
        width: 32; height: 18; radius: 9
        anchors.verticalCenter: parent.verticalCenter
        color: sw.checked ? Pal.focus : Pal.s7
        Behavior on color { ColorAnimation { duration: Pal.moderate } }
        Rectangle {
            width: 14; height: 14; radius: 7; y: 2
            x: sw.checked ? parent.width - width - 2 : 2
            color: "white"
            Behavior on x { NumberAnimation { duration: Pal.moderate; easing.type: Easing.OutCubic } }
        }
    }
    contentItem: Text {
        id: lbl
        leftPadding: 40
        text: sw.text
        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: sw.checked ? 550 : 400
        verticalAlignment: Text.AlignVCenter
    }
}
