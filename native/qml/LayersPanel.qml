import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOBS
import LOBS.Models

Rectangle {
    id: layersGuest
    property size kddockwidgets_min_size: Qt.size(280, 140)
    property bool searching: false
    color: "transparent"

    function rename(id) { layerList.rename(id) }

    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        FPanelHeader {
            title: ""
            meta: app.editSceneName
            FButton {
                iconName: "search"; variant: "ghost"; selected: layersGuest.searching
                tip: "Search layers"
                onClicked: { layersGuest.searching = !layersGuest.searching; if (layersGuest.searching) layerSearch.forceActiveFocus(); else layerSearch.text = "" }
            }
        }
        FSearchField {
            id: layerSearch
            visible: layersGuest.searching
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
            implicitHeight: 26
            placeholderText: "Search layers"
            onDismissed: { layersGuest.searching = false; text = "" }
        }
        FMenu {
            id: layerMenu
            property var row: ({})
            FMenuItem { text: "Rename"; onTriggered: layerList.rename(layerMenu.row.id) }
            FMenuItem { text: chrome.keys("Copy    Ctrl C"); onTriggered: app.copyLayers() }
            FMenuItem { text: chrome.keys("Cut    Ctrl X"); visible: !layerMenu.row.background; height: visible ? implicitHeight : 0; onTriggered: app.cutLayers() }
            FMenuItem { text: chrome.keys("Paste    Ctrl V"); enabled: app.canPaste; onTriggered: app.pasteLayers(false) }
            FMenuItem { text: chrome.keys("Paste linked    Ctrl Shift V"); enabled: app.canPaste; onTriggered: app.pasteLayers(true) }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { text: "Duplicate"; onTriggered: app.duplicateSelected() }
            FCopyToMenu { onCopied: layerMenu.close() }
            FMenuItem { text: "Group selection"; onTriggered: app.groupSelected() }
            FMenuItem { text: "Ungroup"; enabled: layerMenu.row.group; onTriggered: app.ungroupSelected() }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { text: chrome.keys("Bring to front    Ctrl Shift ]"); onTriggered: app.moveSelected(-1000) }
            FMenuItem { text: chrome.keys("Bring forward    Ctrl ]"); onTriggered: app.moveSelected(-1) }
            FMenuItem { text: chrome.keys("Send backward    Ctrl ["); onTriggered: app.moveSelected(1) }
            FMenuItem { text: chrome.keys("Send to back    Ctrl Shift ["); onTriggered: app.moveSelected(1000) }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { visible: !layerMenu.row.background; height: visible ? implicitHeight : 0; text: "Delete"; onTriggered: app.deleteSelected() }
        }
        FluidList {
            id: layerList
            reuseItems: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            readonly property bool filtered: layerSearch.query !== ""
            model: RowsModel { rows: layerList.filtered ? layerList.found() : app.layers }
            function found() {
                const out = [], groups = []
                for (const r of (app.layers, app.allLayers())) {
                    groups.length = r.depth
                    if (layerSearch.matches(r.name)) out.push(Object.assign({}, r, { depth: 0, path: groups.join("  ›  ") }))
                    if (r.group) groups[r.depth] = r.name
                }
                return out
            }
            topMargin: 2
            MouseArea { parent: layerList; anchors.fill: parent; z: -1; onPressed: (m) => { app.clearSelection(); m.accepted = false } }
            property string dragId: ""
            property string editingId: ""
            function rename(id) { editingId = id; const i = app.layers.findIndex(r => r.id === id); if (i >= 0) positionViewAtIndex(i, ListView.Contain) }
            property int dropIndex: -1
            property string dropMode: ""
            property string waitId: ""
            function rowAt(yInContent) {
                const i = indexAt(20, yInContent)
                if (i >= 0) return i
                return yInContent > 0 ? app.layers.length - 1 : 0
            }
            function dragTo(yInContent) {
                const L = app.layers, i = rowAt(yInContent)
                const row = L[i], item = itemAtIndex(i)
                if (!row || !item) { dropIndex = -1; return }
                const from = L.findIndex(r => r.id === dragId)
                let end = from + 1
                while (from >= 0 && end < L.length && L[end].depth > L[from].depth) end++
                const inside = from >= 0 && i >= from && i < end
                if (inside) { dropIndex = -1; dropMode = ""; return }
                const f = (yInContent - item.y) / item.height
                dropIndex = i
                if (row.background) { dropMode = "above"; waitId = "" }
                else if (f < 0.28) { dropMode = "above"; waitId = "" }
                else if (f > 0.72) { dropMode = "below"; waitId = "" }
                else if (row.group) { dropMode = "into"; waitId = "" }
                else if (waitId !== row.id) { waitId = row.id; dropMode = f < 0.5 ? "above" : "below"; groupTimer.restart() }
            }
            function endDrag() {
                const row = app.layers[dropIndex]
                if (dragId && row && dropMode) app.dropLayer(dragId, row.id, dropMode)
                dragId = ""; dropIndex = -1; dropMode = ""; waitId = ""; groupTimer.stop()
            }
            Timer { id: groupTimer; interval: 300; onTriggered: if (layerList.dragId && layerList.waitId) layerList.dropMode = "group" }
            Rectangle {
                parent: layerList.contentItem
                readonly property var it: layerList.dropIndex >= 0 ? layerList.itemAtIndex(layerList.dropIndex) : null
                readonly property var row: layerList.dropIndex >= 0 ? app.layers[layerList.dropIndex] : null
                visible: !!it && !!row && (layerList.dropMode === "above" || layerList.dropMode === "below")
                x: row ? 10 + row.depth * 16 : 0
                width: layerList.width - x - 10
                height: 2; radius: 1
                y: it ? (layerList.dropMode === "above" ? it.y - 1 : it.y + it.height - 1) : 0
                color: Pal.focus
                z: 10
            }
            Rectangle {
                parent: layerList.contentItem
                readonly property var it: layerList.dropIndex >= 0 ? layerList.itemAtIndex(layerList.dropIndex) : null
                visible: !!it && (layerList.dropMode === "into" || layerList.dropMode === "group")
                x: 4; width: layerList.width - 8
                y: it ? it.y + 1 : 0; height: it ? it.height - 2 : 0
                radius: Pal.radius - 2
                color: "transparent"
                border.width: 2; border.color: Pal.focus
                z: 10
                Text {
                    anchors { right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    text: layerList.dropMode === "group" ? "New group" : "Move into group"
                    color: Pal.focus; font.family: Pal.font; font.pixelSize: Pal.caption
                }
            }
            delegate: Item {
                required property var modelData
                required property int index
                width: ListView.view.width
                height: Pal.ctl + 2
                // A recycled row's index is -1 for a moment, the same as "no row hovered": checking
                // hoverIndex >= 0 keeps every row's buttons from flashing on a scene or layout switch.
                readonly property bool ready: !!modelData && modelData.id !== undefined
                readonly property bool showTools: ready && ((layerList.hoverIndex >= 0 && layerList.hoverIndex === index) || modelData.selected === true)
                // A recycled row waiting in the pool is hidden and gives up focus.
                ListView.onPooled: { visible = false; focus = false }
                ListView.onReused: visible = true
                Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1; radius: Pal.radius - 2; color: Pal.selected; visible: modelData.selected }
                opacity: layerList.dragId === modelData.id ? 0.45 : 1
                MouseArea {
                    Accessible.role: Accessible.ListItem
                    Accessible.name: String(modelData.name || "Layer")
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    property real pressY: 0
                    property bool dragged: false
                    preventStealing: dragged
                    onPressed: (m) => { pressY = m.y; dragged = false }
                    onPositionChanged: (m) => {
                        if (!(m.buttons & Qt.LeftButton)) return
                        if (!dragged && !layerList.filtered && Math.abs(m.y - pressY) > 5 && !modelData.background) { dragged = true; layerList.dragId = modelData.id }
                        if (dragged) layerList.dragTo(mapToItem(layerList.contentItem, m.x, m.y).y)
                    }
                    onReleased: if (dragged) layerList.endDrag()
                    onCanceled: { dragged = false; layerList.dragId = ""; layerList.dropIndex = -1 }
                    onClicked: (m) => {
                        if (dragged) return
                        if (m.modifiers & Qt.ShiftModifier) app.selectLayerRange(modelData.id)
                        else app.selectLayer(modelData.id, m.modifiers & Qt.ControlModifier)
                        if (m.button === Qt.RightButton) { layerMenu.row = modelData; layerMenu.popup() }
                    }
                    onDoubleClicked: layerList.rename(modelData.id)
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12 + modelData.depth * 16
                    anchors.rightMargin: 8
                    spacing: 6
                    Rectangle { width: 6; height: 6; radius: 3; color: modelData.color; visible: !modelData.group }
                    FIcon {
                        visible: modelData.group
                        name: modelData.collapsed ? "chevron-right" : "chevron-down"
                        color: chevronMouse.containsMouse ? Pal.fg : Pal.muted
                        Layout.leftMargin: -6
                        MouseArea {
                            id: chevronMouse
                            anchors.fill: parent; anchors.margins: -4
                            hoverEnabled: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.collapsed ? "Show the group's layers" : "Hide the group's layers"
                            onClicked: app.setGroupCollapsed(modelData.id, !modelData.collapsed)
                        }
                    }
                    FIcon { name: modelData.icon; color: modelData.visible ? Pal.fg : Pal.muted }
                    Item {
                        id: nameBox
                        visible: layerList.editingId !== modelData.id
                        Layout.fillWidth: true
                        implicitHeight: nameText.implicitHeight
                        readonly property real sepW: pathText.visible ? sepText.implicitWidth + 8 : 0
                        implicitWidth: (pathText.visible ? pathText.implicitWidth : 0) + sepW + nameText.implicitWidth
                        clip: true
                        Body {
                            id: pathText
                            visible: !!modelData.path
                            text: modelData.path || ""
                            color: Pal.muted; elide: Text.ElideRight
                            width: Math.max(0, Math.min(implicitWidth, nameBox.width - nameText.width - nameBox.sepW))
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Body {
                            id: sepText
                            visible: pathText.visible
                            x: pathText.width + 4
                            text: "›"; color: Pal.muted
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Body {
                            id: nameText
                            x: pathText.visible ? pathText.width + nameBox.sepW : 0
                            text: modelData.name + (modelData.collapsed ? "  ·  " + modelData.childCount : "")
                            color: modelData.visible ? Pal.fg : Pal.muted; font.weight: modelData.selected ? 550 : 400
                            width: Math.min(implicitWidth, nameBox.width - (pathText.visible ? Math.min(pathText.implicitWidth, 24) + nameBox.sepW : 0))
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    InlineRename {
                        active: layerList.editingId === modelData.id
                        original: modelData.name
                        Layout.fillWidth: true
                        onRenamed: (n) => app.renameLayer(modelData.id, n)
                        onFinished: layerList.editingId = ""
                    }
                    FIcon { visible: modelData.linked; name: "link-2"; color: Pal.link; ToolTip.text: "Linked with another scene" }
                    FBadge { visible: modelData.fx > 0; text: modelData.fx + " fx" }
                    Row {
                        id: rowTools
                        spacing: 0
                        opacity: showTools ? 1 : 0
                        Layout.preferredWidth: showTools ? implicitWidth : 0
                        clip: true
                        property bool settled: false
                        Component.onCompleted: Qt.callLater(() => settled = true)
                        Behavior on opacity { enabled: rowTools.settled; NumberAnimation { duration: Pal.fast } }
                        FButton { visible: ready && !modelData.background; iconName: "chevron-up"; variant: "ghost"; onClicked: app.moveLayer(modelData.id, -1); tip: "Move up" }
                        FButton { visible: ready && !modelData.background; iconName: "chevron-down"; variant: "ghost"; onClicked: app.moveLayer(modelData.id, 1); tip: "Move down" }
                        FButton { iconName: modelData.locked ? "lock" : "lock-open"; variant: "ghost"; onClicked: app.toggleLock(modelData.id); tip: modelData.locked ? "Unlock" : "Lock" }
                    }
                    FButton { visible: ready && modelData.locked === true && !showTools; iconName: "lock"; variant: "ghost"; onClicked: app.toggleLock(modelData.id); tip: "Unlock" }
                    FButton { iconName: modelData.visible ? "eye" : "eye-off"; variant: "ghost"; onClicked: app.toggleVisible(modelData.id); tip: modelData.visible ? "Hide" : "Show" }
                }
            }
            Caption {
                visible: app.layers.length === 0
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                text: "Empty scene.\nDrag a source from Sources onto the canvas."
            }
        }
    }
}
