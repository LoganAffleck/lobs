import QtQuick

// ListView with fluid hover: one highlight glides to the row nearest the cursor
// (no dead zones between rows, no blinking) on the fast spring. Rows stay transparent;
// a delegate can opt out with `property bool hoverable: false` (e.g. bin headers).
ListView {
    id: lv
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    boundsMovement: Flickable.StopAtBounds
    flickDeceleration: 6000
    maximumFlickVelocity: 3000
    property int hoverIndex: -1

    Rectangle {
        id: glide
        parent: lv.contentItem
        z: -1
        x: 4
        width: lv.width - 8
        radius: Pal.radius - 2
        color: Pal.hover
        property Item target: lv.hoverIndex >= 0 ? lv.itemAtIndex(lv.hoverIndex) : null
        opacity: target ? 1 : 0
        y: target ? target.y : y
        height: target ? target.height : height
        Behavior on y { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Pal.fast } }
    }
    HoverHandler {
        onPointChanged: {
            const y = point.position.y + lv.contentY - lv.originY
            let i = lv.indexAt(10, y)
            if (i < 0) { // nearest row wins
                let best = -1, bd = 1e9
                for (let k = 0; k < lv.count; k++) {
                    const it = lv.itemAtIndex(k); if (!it) continue
                    const d = Math.abs(it.y + it.height / 2 - y); if (d < bd) { bd = d; best = k }
                }
                i = bd < 24 ? best : -1
            }
            const it = i >= 0 ? lv.itemAtIndex(i) : null
            lv.hoverIndex = it && it.hoverable !== false ? i : -1
        }
        onHoveredChanged: if (!hovered) lv.hoverIndex = -1
    }
}
