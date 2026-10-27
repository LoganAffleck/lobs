import QtQuick
import QtQuick.Controls

// Dialog on the slow spring (with a little bounce); exits one tier faster.
Dialog {
    // Drawn in the window's own scene, never as a separate OS window: a new popup window
    // raced the main window's rendering (null swapchain in QRhi::beginFrame / grabWindow).
    popupType: Popup.Item
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
    // The video preview is a native window drawn over the QML, so it hides while any dialog is
    // up (a dialog over the canvas was invisible under it, leaving only the dimmed backdrop). Keyed,
    // not the shared popup count: a menu closing as its item opens a dialog can't cancel it.
    readonly property string coverKey: "dialog:" + d
    onVisibleChanged: app.setCovering(coverKey, visible)
    Component.onDestruction: app.setCovering(coverKey, false)
    background: Rectangle { radius: 12; color: Pal.s5; border.width: 1; border.color: Pal.border }
    header: Text {
        text: d.title; leftPadding: 16; topPadding: 16; bottomPadding: 4
        color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550
    }
}
