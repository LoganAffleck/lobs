import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Graphic tools: a floating bar under the canvas. Pick a tool, drag on the canvas, get a layer
// in the scene you're editing. The laser draws a fading trail viewers see, and is not a layer.
// Colors and width live in the Inspector (popups here would open over the video).
Rectangle {
    id: bar
    implicitHeight: 40
    implicitWidth: row.implicitWidth + 12
    radius: Pal.radius + 4
    color: Pal.s3
    border.width: 1; border.color: Pal.border
    readonly property var tools: [
        { id: "select", icon: "mouse-pointer-2", key: "V", tip: "Select and move layers" },
        { id: "text", icon: "type", key: "T", tip: "Text: click or drag a box" },
        { id: "rect", icon: "square", key: "R", tip: "Rectangle: drag (Shift for a square)" },
        { id: "ellipse", icon: "circle", key: "O", tip: "Ellipse: drag (Shift for a circle)" },
        { id: "arrow", icon: "arrow-up-right", key: "A", tip: "Arrow (Shift snaps to 45°)" },
        { id: "pencil", icon: "pencil", key: "P", tip: "Pencil" },
        { id: "laser", icon: "siren", key: "L", tip: "Laser pointer (not added to the scene)" }
    ]
    component ToolButton: AbstractButton {
        id: tb
        property string iconName
        property bool active: false
        property string tip
        implicitWidth: 32; implicitHeight: 30
        hoverEnabled: true
        background: Rectangle {
            radius: Pal.radius
            color: tb.active ? Pal.fg : tb.down ? Pal.s6 : tb.hovered ? Pal.s5 : "transparent"
            Behavior on color { ColorAnimation { duration: Pal.fast } }
        }
        contentItem: Item { FIcon { anchors.centerIn: parent; name: tb.iconName; size: 15; color: tb.active ? Pal.s1 : tb.hovered ? Pal.fg : Pal.muted } }
        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: tip
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 2
        Repeater {
            model: bar.tools
            delegate: ToolButton {
                required property var modelData
                Accessible.name: modelData.tip
                iconName: modelData.icon
                active: app.tool === modelData.id
                tip: modelData.tip + "  " + modelData.key
                onClicked: app.tool = modelData.id
            }
        }
    }
}
