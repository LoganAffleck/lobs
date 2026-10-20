import QtQuick
import QtQuick.Controls

// 14px checkbox (compact icon size). Fills with the foreground color when checked.
AbstractButton {
    id: cb
    checkable: true
    hoverEnabled: true
    implicitHeight: Pal.ctl
    implicitWidth: 14 + (text.length ? lbl.implicitWidth + 8 : 0)
    indicator: Rectangle {
        width: 14; height: 14; radius: 4
        anchors.verticalCenter: parent.verticalCenter
        color: cb.checked ? Pal.fg : "transparent"
        border.width: cb.checked ? 0 : 1
        border.color: cb.hovered ? Pal.muted : Pal.borderStrong
        Behavior on color { ColorAnimation { duration: Pal.fast } }
        Text { anchors.centerIn: parent; text: "✓"; visible: cb.checked; color: Pal.s1; font.pixelSize: 11; font.bold: true }
    }
    contentItem: Text {
        id: lbl
        leftPadding: 22
        text: cb.text
        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
        verticalAlignment: Text.AlignVCenter
    }
}
