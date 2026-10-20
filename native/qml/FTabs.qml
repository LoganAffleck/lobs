import QtQuick

// Segmented tabs: a padded list with one pill that slides to the selected tab (moderate spring).
// Hovering glides a lighter highlight along the strip (fluid hover on the x axis).
Rectangle {
    id: t
    property var model: []
    property int currentIndex: 0
    property bool fillSlot: false // in settings rows, keep natural width
    signal activated(int index)
    implicitHeight: Pal.ctl
    implicitWidth: row.implicitWidth + 4
    radius: Pal.radius
    color: Pal.s1
    border.width: 1
    border.color: Pal.border
    property int hoverIndex: -1
    function itemX(i) { const it = rep.itemAt(i); return it ? it.x + 2 : 0 }
    function itemW(i) { const it = rep.itemAt(i); return it ? it.width : 0 }

    Rectangle { // hover glide
        opacity: t.hoverIndex >= 0 && t.hoverIndex !== t.currentIndex ? 1 : 0
        x: t.itemX(Math.max(0, t.hoverIndex)); width: t.itemW(Math.max(0, t.hoverIndex))
        y: 2; height: parent.height - 4
        radius: Pal.radius - 2
        color: Pal.hover
        Behavior on x { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Pal.fast } }
    }
    Rectangle { // selected pill
        x: t.itemX(t.currentIndex); width: t.itemW(t.currentIndex)
        y: 2; height: parent.height - 4
        radius: Pal.radius - 2
        color: Pal.s6
        Behavior on x { NumberAnimation { duration: Pal.moderate; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Pal.moderate; easing.type: Easing.OutCubic } }
    }
    Row {
        id: row
        x: 2
        height: parent.height
        Repeater {
            id: rep
            model: t.model
            delegate: Item {
                required property var modelData
                required property int index
                width: txt.implicitWidth + 20
                height: row.height
                Text {
                    id: txt
                    anchors.centerIn: parent
                    text: modelData
                    color: index === t.currentIndex ? Pal.fg : Pal.muted
                    font.family: Pal.font; font.pixelSize: Pal.body
                    font.weight: index === t.currentIndex ? 550 : 400
                    Behavior on color { ColorAnimation { duration: Pal.moderate } }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: t.hoverIndex = index
                    onExited: if (t.hoverIndex === index) t.hoverIndex = -1
                    onClicked: { t.currentIndex = index; t.activated(index) }
                }
            }
        }
    }
}
