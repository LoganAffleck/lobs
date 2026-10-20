import QtQuick
import com.kdab.dockwidgets 2.0
import LOBS

// Gap between panels: the window background shows through; it lights up when you can drag it.
Rectangle {
    id: root
    Accessible.role: Accessible.Separator
    Accessible.name: "Resize panels"
    anchors.fill: parent
    readonly property SeparatorView kddwSeparator: parent
    color: mouse.containsMouse || mouse.pressed ? Pal.focus : Pal.s1
    Behavior on color { ColorAnimation { duration: Pal.fast } }
    MouseArea {
        id: mouse
        Accessible.role: Accessible.Separator
        Accessible.name: "Resize panels"
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.kddwSeparator && root.kddwSeparator.isVertical ? Qt.SizeVerCursor : Qt.SizeHorCursor
        onPressed: root.kddwSeparator.onMousePressed()
        onReleased: root.kddwSeparator.onMouseReleased()
        onPositionChanged: (m) => root.kddwSeparator.onMouseMoved(Qt.point(m.x, m.y))
        onDoubleClicked: root.kddwSeparator.onMouseDoubleClicked()
    }
}
