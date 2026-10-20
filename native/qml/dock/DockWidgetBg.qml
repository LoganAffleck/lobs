import QtQuick
import LOBS

// Panel body background: surface 2, so panel content sits on the same step as the group card.
Rectangle {
    readonly property QtObject dockWidgetCpp: parent
    anchors.fill: parent
    color: "transparent" // the rounded group card behind it draws the surface
}
