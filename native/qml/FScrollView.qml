import QtQuick
import QtQuick.Controls

// Desktop scrolling: hard stops at the top and bottom (no touch-style overscroll bounce),
// quick deceleration, and a thin scrollbar that fades in while scrolling or hovered.
ScrollView {
    id: sv
    Accessible.role: Accessible.Pane
    Accessible.name: "Scroll area"
    clip: true
    contentWidth: availableWidth
    Component.onCompleted: {
        const f = contentItem
        if (f && f.boundsBehavior !== undefined) {
            f.boundsBehavior = Flickable.StopAtBounds
            f.boundsMovement = Flickable.StopAtBounds
            f.flickDeceleration = 6000
            f.maximumFlickVelocity = 3000
        }
    }
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ScrollBar.vertical: ScrollBar {
        Accessible.name: "Scroll"
        id: bar
        parent: sv
        x: sv.width - width - 2
        y: 2
        height: sv.height - 4
        active: sv.hovered || pressed || sv.contentItem.moving
        padding: 0
        contentItem: Rectangle {
            implicitWidth: bar.pressed ? 6 : 4
            radius: width / 2
            color: bar.pressed ? Pal.muted : Pal.borderStrong
            opacity: bar.active && bar.size < 1 ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Pal.moderate } }
            Behavior on implicitWidth { NumberAnimation { duration: Pal.fast } }
        }
        background: Item {}
    }
}
