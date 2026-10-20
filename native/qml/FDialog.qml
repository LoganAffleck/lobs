import QtQuick
import QtQuick.Controls

// Dialog on the slow spring (with a little bounce); exits one tier faster.
Dialog {
    id: d
    modal: true
    padding: 16
    topPadding: 8
    anchors.centerIn: Overlay.overlay
    Overlay.modal: Rectangle { color: "#99000000" }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Pal.slow }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Pal.slow; easing.type: Easing.OutBack; easing.overshoot: 1.2 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Pal.moderate }
        NumberAnimation { property: "scale"; to: 0.98; duration: Pal.moderate }
    }
    background: Rectangle { radius: 12; color: Pal.s5; border.width: 1; border.color: Pal.border }
    header: Text {
        text: d.title; leftPadding: 16; topPadding: 16; bottomPadding: 4
        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550
    }
}
