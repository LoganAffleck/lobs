import QtQuick
import QtQuick.Window

// Lucide icon. Compact step: 14px; default step: 16px. Color follows the theme (fg/muted).
// An Item wrapper so layouts see `size`, not the DPR-scaled pixel size of the rendered image.
Item {
    id: ic
    property string name
    property color color: Pal.fg
    property int size: 14
    property real stroke: 2
    implicitWidth: size
    implicitHeight: size
    Image {
        anchors.fill: parent
        smooth: true
        cache: true
        fillMode: Image.PreserveAspectFit
        sourceSize: Qt.size(Math.ceil(ic.size * Screen.devicePixelRatio), Math.ceil(ic.size * Screen.devicePixelRatio))
        source: ic.name ? "image://icon/" + ic.name + "?c=" + String(ic.color).replace("#", "") + "&w=" + ic.stroke : ""
    }
}
