import QtQuick
import QtQuick.Controls

// Menu row. Hover is shown by the background only (changing weight on hover makes the label
// shift); the currently chosen option is semibold with a check.
MenuItem {
    id: it
    property bool selected: false
    implicitHeight: Pal.ctl
    leftPadding: 10
    rightPadding: 10
    hoverEnabled: true
    contentItem: Item {
        implicitWidth: label.implicitWidth + 24
        Text {
            id: label
            anchors.left: parent.left
            anchors.right: check.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: it.text
            color: it.enabled ? Pal.fg : Pal.muted
            font.family: Pal.font; font.pixelSize: Pal.body
            font.weight: it.selected ? 550 : 400
            elide: Text.ElideRight
        }
        FIcon { id: check; visible: it.selected; name: "check"; color: Pal.fg; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter }
    }
    arrow: FIcon {
        x: it.width - width - 8; anchors.verticalCenter: parent.verticalCenter
        visible: it.subMenu; name: "chevron-right"; color: Pal.muted
    }
    indicator: Item {}
    background: Rectangle {
        radius: Pal.radius - 2
        color: Pal.hover
        opacity: it.highlighted ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Pal.fast } }
    }
}
