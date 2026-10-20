import QtQuick

// Pointer handling for an Asset Library item (list row or grid tile).
//  - click selects (Ctrl toggles, Shift extends); double-click places a file or opens a folder
//  - drag a file onto the canvas to place it, or onto a folder to move it there
//  - right-click for its menu
MouseArea {
    id: am
    required property var asset
    required property Item previewItem   // the canvas preview
    required property Item ghostItem     // drag ghost (has show(icon, name, x, y))
    required property Item overlayItem   // coordinate space for the ghost
    property var folderAt: null          // fn(globalX, globalY) -> folder path under the point, or ""
    signal menuRequested(var asset)
    signal selectRequested(var asset, int modifiers)
    signal movedToFolder(var asset, string folder)
    property point start
    property bool dragging: false
    anchors.fill: parent
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    hoverEnabled: true
    function inPreview(p) { return !!previewItem && p.x >= 0 && p.y >= 0 && p.x <= previewItem.width && p.y <= previewItem.height }
    function targetFolder(m) {
        if (!folderAt) return ""
        const g = mapToGlobal(m.x, m.y)
        const f = folderAt(g.x, g.y)
        return f && f !== asset.path ? f : ""
    }
    onPressed: (m) => { start = Qt.point(m.x, m.y); dragging = false; if (m.button === Qt.LeftButton) am.selectRequested(asset, m.modifiers) }
    onPositionChanged: (m) => {
        if (!(m.buttons & Qt.LeftButton)) return
        if (!dragging && Math.hypot(m.x - start.x, m.y - start.y) > 6) dragging = true
        if (!dragging) return
        const g = mapToItem(overlayItem, m.x, m.y)
        const into = targetFolder(m)
        ghostItem.show(into ? "folder" : asset.icon, into ? "Move to " + into.split("/").pop() : asset.name, g.x, g.y)
        const p = mapToItem(previewItem, m.x, m.y)
        if (!into && asset.placeable && inPreview(p)) app.assetDragMove(asset.path, p.x, p.y)
        else app.poolDragCancel()
    }
    onReleased: (m) => {
        if (!dragging) return
        dragging = false
        ghostItem.visible = false
        const into = targetFolder(m)
        if (into) { app.poolDragCancel(); am.movedToFolder(asset, into); return }
        const p = mapToItem(previewItem, m.x, m.y)
        if (asset.placeable && inPreview(p)) app.assetDrop(asset.path, p.x, p.y)
        else app.poolDragCancel()
    }
    onClicked: (m) => { if (m.button === Qt.RightButton) am.menuRequested(asset) }
    onDoubleClicked: asset.folder ? app.openAssetFolder(asset.path) : asset.placeable ? app.placeAsset(asset.path) : undefined
}
