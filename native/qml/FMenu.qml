import QtQuick
import QtQuick.Controls

// Dropdown menu: one surface step above the page, rounded, rows highlight on hover.
Menu {
    // Drawn in the window's own scene, never as a separate OS window: a new popup window
    // raced the main window's rendering (null swapchain in QRhi::beginFrame / grabWindow).
    popupType: Popup.Item
    id: m
    padding: 4
    z: 1000 // above full-page overlays like Settings
    delegate: FMenuItem {}
    // The engine's preview is a native window above all QML; step it aside while open.
    property bool coversPreview: false
    // True if this popup's rect overlaps the canvas preview (a native window above all QML).
    function overlapsPreview(popup) {
        if (app.previewInScene) return false // drawn in the scene: menus draw over it
        const w = popup.parent ? popup.parent.Window.window : null
        const p = w ? w.previewItem : null
        if (!p || !p.visible || !popup.parent) return false
        const a = p.mapToGlobal(0, 0), b = popup.parent.mapToGlobal(popup.x, popup.y)
        const pw = Math.max(popup.width, popup.implicitWidth), ph = Math.max(popup.height, popup.implicitHeight)
        return b.x < a.x + p.width && b.x + pw > a.x && b.y < a.y + p.height && b.y + ph > a.y
    }
    // Size and position settle after aboutToShow (popup() at the cursor, contents laid out),
    // so check again once they do; otherwise the native preview can cut the menu off.
    function recheck() {
        if (!visible || coversPreview) return
        if (overlapsPreview(m)) { coversPreview = true; app.popupOpened() }
    }
    // Lists of items (scenes, options...) are built only while the menu is open: Instantiators
    // use `active: menu.populated`. Otherwise every change rebuilt every menu's items.
    property bool populated: false
    onAboutToShow: { populated = true; coversPreview = overlapsPreview(m); if (coversPreview) app.popupOpened() }
    onClosed: populated = false
    onOpened: recheck()
    onXChanged: recheck()
    onYChanged: recheck()
    onWidthChanged: recheck()
    onHeightChanged: recheck()
    onAboutToHide: if (coversPreview) { coversPreview = false; app.popupClosed() }
    // Tells the canvas watchdog a menu is up, however the menu is shown or closed.
    onVisibleChanged: app.menuVisible(visible)
    enter: Transition {
        // A plain fade: scaling made menus wider than their field look like they slide in.
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Pal.fast }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Pal.fast } }
    background: Rectangle {
        implicitWidth: 220
        radius: Pal.radius + 2
        color: Pal.s4
        border.width: 1
        border.color: Pal.border
    }
}
