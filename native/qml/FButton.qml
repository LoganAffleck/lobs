import QtQuick
import QtQuick.Controls

// Fluid Functionalism Button. Variants: primary (light on dark), secondary, tertiary, ghost, danger.
// Compact step: 28px tall, 12px text, 14px Lucide icon, 10px side padding (square when icon-only).
// Hover tints on the fast spring; press dips.
AbstractButton {
    id: b
    property string variant: "secondary"
    property string tip: ""
    property bool selected: false
    property string iconName: ""
    property color iconColor: !enabled ? Pal.muted : variant === "primary" ? Pal.s1 : variant === "danger" ? Pal.danger : (selected || hovered ? Pal.fg : Pal.muted)
    readonly property bool iconOnly: iconName !== "" && text === ""
    implicitHeight: Pal.ctl
    implicitWidth: iconOnly ? Pal.ctl : Math.max(Pal.ctl, row.implicitWidth + 20)
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    property bool pressDip: true // fields that open popups turn this off (the dip reads as a shrink)
    scale: down && pressDip ? 0.97 : 1
    Behavior on scale { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }

    readonly property color baseColor: variant === "primary" ? Pal.fg
        : variant === "secondary" ? Pal.s6
        : variant === "danger" ? Pal.dangerBg
        : "transparent"
    background: Rectangle {
        radius: Pal.radius
        color: b.baseColor
        border.width: b.variant === "tertiary" ? 1 : 0
        border.color: Pal.border
        opacity: b.enabled ? 1 : 0.5
        Rectangle { // hover / selected tint
            anchors.fill: parent
            radius: parent.radius
            color: b.variant === "primary" ? "#000000" : "#ffffff"
            opacity: b.selected ? 0.10 : b.hovered ? (b.variant === "primary" ? 0.08 : 0.06) : 0
            Behavior on opacity { NumberAnimation { duration: Pal.fast } }
        }
        Rectangle { // focus ring
            anchors.fill: parent; anchors.margins: -2
            radius: parent.radius + 2; color: "transparent"
            border.width: 2; border.color: Pal.focus
            visible: b.visualFocus
        }
    }
    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            FIcon { visible: b.iconName !== ""; name: b.iconName; color: b.iconColor; anchors.verticalCenter: parent.verticalCenter }
            Text {
                visible: b.text !== ""
                text: b.text
                font.family: Pal.font
                font.pixelSize: Pal.body
                font.weight: b.selected ? 550 : 450
                color: !b.enabled ? Pal.muted : b.variant === "primary" ? Pal.s1 : b.variant === "danger" ? Pal.danger : Pal.fg
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    ToolTip.visible: tip !== "" && hovered
    ToolTip.delay: 500
    ToolTip.text: tip
}
