import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Effects
import LOBS

FocusScope {
    id: assetsGuest
    property Item previewItem: null
    property Item ghostItem: null
    property Item overlayItem: null
    property size kddockwidgets_min_size: Qt.size(240, 140)
    property int view: app.uiValue("assets/view", "grid") === "list" ? 0 : 1
    property int typeFilter: 0
    property string sortKey: app.uiValue("assets/sort", "name")
    property bool sortDesc: app.uiFlag("assets/sortDesc", false)
    property var selected: []
    property string anchorPath: ""
    property bool searching: false

    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    readonly property var raw: assetSearch.query ? (app.assets, app.searchAssets(assetSearch.query)) : app.assets
    readonly property var items: {
        const kinds = [null, "image", "video", "audio"]
        const want = kinds[typeFilter]
        const rows = raw.filter(a => !want || a.folder || a.kind === want)
        const key = sortKey, dir = sortDesc ? -1 : 1
        const cmp = (a, b) => key === "name" ? dir * a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: "base" })
                                             : dir * ((a[key] || 0) - (b[key] || 0)) || a.name.localeCompare(b.name)
        return rows.filter(a => a.folder).sort(cmp).concat(rows.filter(a => !a.folder).sort(cmp))
    }
    function isSel(path) { return selected.indexOf(path) >= 0 }
    function selectItem(a, mods) {
        forceActiveFocus()
        if (mods & Qt.ControlModifier) {
            selected = isSel(a.path) ? selected.filter(x => x !== a.path) : selected.concat([a.path])
        } else if ((mods & Qt.ShiftModifier) && anchorPath) {
            const paths = items.map(x => x.path), i = paths.indexOf(anchorPath), j = paths.indexOf(a.path)
            selected = paths.slice(Math.min(i, j), Math.max(i, j) + 1)
            return
        } else if (!isSel(a.path)) {
            selected = [a.path]
        }
        anchorPath = a.path
    }
    function targets(a) { return a && isSel(a.path) ? selected : a ? [a.path] : selected }
    property string editingPath: ""
    function rename(path) { editingPath = path }
    function newFolder() { const f = app.createAssetFolder(); if (f) { selected = [f]; Qt.callLater(rename, f) } }
    function currentFolderPath() { return app.assetPath ? app.assetsFolder + "/" + app.assetPath : app.assetsFolder }
    property string dropFolder: ""
    function folderAt(gx, gy) {
        const cp = crumbs.mapFromGlobal(gx, gy)
        const crumb = crumbs.childAt(cp.x, cp.y)
        if (crumb && crumb.modelData && !crumb.last) return crumb.modelData.dir
        if (view === 1) {
            const flow = assetGrid.headerItem ? assetGrid.headerItem.children[0] : null
            if (!flow) return ""
            const p = flow.mapFromGlobal(gx, gy)
            const it = flow.childAt(p.x, p.y)
            return it && it.modelData && it.modelData.folder ? it.modelData.path : ""
        }
        const p = assetListView.contentItem.mapFromGlobal(gx, gy)
        const it = assetListView.itemAt(p.x, p.y)
        return it && it.modelData && it.modelData.folder ? it.modelData.path : ""
    }
    function duration(ms) { const t = Math.round(ms / 1000); return Math.floor(t / 60) + ":" + String(t % 60).padStart(2, "0") }
    function detailOf(a) {
        if (a.folder) return a.size
        return [a.durationMs ? duration(a.durationMs) : "", a.dims || "", a.size].filter(t => t).join(" · ")
    }
    function setSort(k) {
        if (sortKey === k) sortDesc = !sortDesc; else { sortKey = k; sortDesc = k !== "name" }
        app.setUiValue("assets/sort", sortKey); app.setUiFlag("assets/sortDesc", sortDesc)
    }
    Connections { target: app; function onAssetsChanged() { assetsGuest.selected = assetsGuest.selected.filter(x => app.assets.some(a => a.path === x)) } }
    Keys.onPressed: (e) => {
        const ctrl = e.modifiers & Qt.ControlModifier
        if (ctrl && e.key === Qt.Key_C) { app.copyAssets(selected); e.accepted = true }
        else if (ctrl && e.key === Qt.Key_X) { app.cutAssets(selected); e.accepted = true }
        else if (ctrl && e.key === Qt.Key_V) { app.pasteAssets(); e.accepted = true }
        else if (ctrl && e.key === Qt.Key_A) { selected = items.map(x => x.path); e.accepted = true }
        else if (ctrl && e.key === Qt.Key_N) { newFolder(); e.accepted = true }
        else if (ctrl && e.key === Qt.Key_F) { searching = true; assetSearch.forceActiveFocus(); e.accepted = true }
        else if (e.key === Qt.Key_Delete && selected.length) { app.trashAssets(selected); e.accepted = true }
        else if (e.key === Qt.Key_F2 && selected.length === 1) { rename(selected[0]); e.accepted = true }
        else if (e.key === Qt.Key_Backspace) { app.assetUp(); e.accepted = true }
        else if ((e.key === Qt.Key_Return || e.key === Qt.Key_Enter) && selected.length === 1) {
            const a = items.find(x => x.path === selected[0])
            if (a) a.folder ? app.openAssetFolder(a.path) : a.placeable ? app.placeAsset(a.path) : undefined
            e.accepted = true
        }
    }
    FileDialog { id: assetPicker; fileMode: FileDialog.OpenFiles; title: "Add files to assets"; onAccepted: app.importAssets(selectedFiles) }
    FileDialog {
        id: relinkPicker
        property string sourceId
        title: "Find the missing file"
        onAccepted: app.relinkAsset(sourceId, String(selectedFile).replace(/^file:\/{3}/, ""))
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            Layout.leftMargin: 6; Layout.rightMargin: 6
            spacing: 0
            RowLayout {
                visible: assetsGuest.raw.length > 0
                spacing: 0
                FButton {
                    id: sortButton
                    text: ({ name: "Name", modified: "Date", bytes: "Size" })[assetsGuest.sortKey] + (assetsGuest.sortDesc ? " ↓" : " ↑")
                    variant: "ghost"; tip: "Sort"
                    onClicked: sortMenu.popup(sortButton, 0, sortButton.height + 4)
                    FMenu {
                        id: sortMenu
                        FMenuItem { text: "Name"; selected: assetsGuest.sortKey === "name"; onTriggered: assetsGuest.setSort("name") }
                        FMenuItem { text: "Date modified"; selected: assetsGuest.sortKey === "modified"; onTriggered: assetsGuest.setSort("modified") }
                        FMenuItem { text: "Size"; selected: assetsGuest.sortKey === "bytes"; onTriggered: assetsGuest.setSort("bytes") }
                        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                        FMenuItem { text: assetsGuest.sortDesc ? "Ascending" : "Descending"; onTriggered: { assetsGuest.sortDesc = !assetsGuest.sortDesc; app.setUiFlag("assets/sortDesc", assetsGuest.sortDesc) } }
                    }
                }
                FButton { iconName: "layout-grid"; variant: "ghost"; selected: assetsGuest.view === 1; tip: "Grid"; onClicked: { assetsGuest.view = 1; app.setUiValue("assets/view", "grid") } }
                FButton { iconName: "list"; variant: "ghost"; selected: assetsGuest.view === 0; tip: "List"; onClicked: { assetsGuest.view = 0; app.setUiValue("assets/view", "list") } }
            }
            Item { Layout.fillWidth: true }
            FButton {
                iconName: "search"; variant: "ghost"; selected: assetsGuest.searching
                tip: chrome.keys("Search all folders  Ctrl F")
                onClicked: { assetsGuest.searching = !assetsGuest.searching; if (assetsGuest.searching) assetSearch.forceActiveFocus(); else assetSearch.text = "" }
            }
            FButton {
                id: moreAssets
                iconName: "ellipsis"; variant: "ghost"; tip: "More"
                onClicked: bgMenu.popup(moreAssets, 0, moreAssets.height + 4)
            }
            FButton { iconName: "plus"; text: assetsGuest.width < 360 ? "" : "Add"; variant: "secondary"; tip: "Add files"; onClicked: assetPicker.open(); Layout.leftMargin: 2 }
        }
        RowLayout {
            visible: app.assetPath !== ""
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            Layout.leftMargin: 6; Layout.rightMargin: 6; Layout.bottomMargin: 2
            spacing: 0
            FButton { visible: app.assetPath !== ""; iconName: "corner-left-up"; variant: "ghost"; tip: "Up one folder  Backspace"; onClicked: app.assetUp() }
            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                implicitHeight: 28
                clip: true
                MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: bgMenu.popup() }
                Row {
                    id: crumbs
                    anchors.verticalCenter: parent.verticalCenter
                    x: Math.min(0, parent.width - width)
                    Repeater {
                        model: app.assetPath ? app.assetCrumbs : []
                        delegate: Row {
                            required property var modelData
                            required property int index
                            readonly property bool last: index === app.assetCrumbs.length - 1
                            spacing: 0
                            height: 28
                            FIcon { visible: index > 0; name: "chevron-right"; size: 12; color: Pal.muted; anchors.verticalCenter: parent.verticalCenter }
                            FButton { visible: !parent.last; text: modelData.name; variant: "ghost"; selected: assetsGuest.dropFolder === modelData.dir; implicitHeight: 24; anchors.verticalCenter: parent.verticalCenter; onClicked: app.goToAssetPath(modelData.path) }
                            Text { visible: parent.last; text: modelData.name; leftPadding: 8; rightPadding: 4; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }
            FButton { iconName: "folder-search"; variant: "ghost"; tip: "Show this folder in File Explorer"; onClicked: app.revealAsset(assetsGuest.currentFolderPath()) }
        }
        FSearchField {
            id: assetSearch
            visible: assetsGuest.searching
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
            implicitHeight: 26
            placeholderText: "Search all folders"
            onDismissed: { assetsGuest.searching = false; text = ""; assetsGuest.forceActiveFocus() }
        }
        Rectangle {
            visible: app.missingAssets.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: 8; Layout.rightMargin: 8; Layout.bottomMargin: 6
            implicitHeight: missRow.implicitHeight + 12
            radius: Pal.radius
            color: "#2a2008"; border.width: 1; border.color: "#92400e"
            RowLayout {
                id: missRow
                anchors.fill: parent; anchors.margins: 6; anchors.leftMargin: 10
                spacing: 6
                FIcon { name: "triangle-alert"; size: 14; color: "#fbbf24" }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#fde68a"; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: app.missingAssets.length === 1
                          ? app.missingAssets[0].name + " was moved or deleted outside LOBS."
                          : app.missingAssets.length + " files were moved or deleted outside LOBS."
                }
                FButton { text: "Find it"; variant: "secondary"; tip: "Search assets by file name"; onClicked: app.findMissingAssets() }
                FButton {
                    text: "Choose file…"; variant: "ghost"
                    tip: "Choose a new file for " + (app.missingAssets[0] ? app.missingAssets[0].name : "it")
                    onClicked: { relinkPicker.sourceId = app.missingAssets[0].sourceId; relinkPicker.open() }
                }
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (m) => { assetsGuest.forceActiveFocus(); assetsGuest.selected = []; if (m.button === Qt.RightButton) bgMenu.popup() }
            }
            Component {
                id: dropHint
                Item {
                    width: ListView.view ? ListView.view.width : (GridView.view ? GridView.view.width : 200)
                    height: assetsGuest.raw.length ? 44 : 0
                    visible: assetsGuest.raw.length > 0 && !assetSearch.query
                    Rectangle {
                        anchors.fill: parent; anchors.margins: 4
                        radius: Pal.radius
                        color: hintMouse.pressed ? Pal.s5 : hintMouse.containsMouse ? Pal.s4 : "transparent"
                        border.width: 1; border.color: hintMouse.containsMouse ? Pal.borderStrong : Pal.border
                        Behavior on color { ColorAnimation { duration: Pal.fast } }
                        Text { anchors.centerIn: parent; text: "Drop files here, or click to add them"; color: hintMouse.containsMouse ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                        MouseArea { id: hintMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: assetPicker.open() }
                    }
                }
            }
            FluidList {
                id: assetListView
                visible: assetsGuest.view === 0
                anchors.fill: parent
                topMargin: 2
                model: assetsGuest.items
                footer: dropHint
                delegate: Item {
                    required property var modelData
                    width: ListView.view.width
                    height: 40
                    Rectangle {
                        readonly property bool target: modelData.folder && assetsGuest.dropFolder === modelData.path
                        visible: assetsGuest.isSel(modelData.path) || target
                        anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1
                        radius: Pal.radius - 2; color: target ? Pal.s6 : Pal.selected
                        border.width: target ? 1 : 0; border.color: Pal.focus
                    }
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 12
                        spacing: 10
                        Rectangle {
                            implicitWidth: 44; implicitHeight: 28; radius: 4
                            color: modelData.folder ? "transparent" : Pal.s1
                            Image {
                                id: rowThumb
                                anchors.fill: parent
                                visible: !!modelData.thumb && status === Image.Ready
                                source: modelData.thumb || ""
                                sourceSize.width: 88
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                layer.enabled: assetsGuest.visible && rowThumb.visible
                                layer.effect: MultiEffect { maskEnabled: true; maskSource: rowMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                            }
                            Rectangle { id: rowMask; anchors.fill: parent; radius: 4; visible: false; layer.enabled: assetsGuest.visible && rowThumb.visible; layer.smooth: true }
                            FIcon { visible: !rowThumb.visible; anchors.centerIn: parent; name: modelData.icon; size: modelData.folder ? 18 : 14; color: modelData.folder ? (modelData.color || "#e9b949") : Pal.muted }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Body { visible: assetsGuest.editingPath !== modelData.path; text: modelData.name; Layout.fillWidth: true; elide: Text.ElideRight }
                            InlineRename {
                                active: assetsGuest.editingPath === modelData.path
                                original: modelData.name
                                onRenamed: (n) => app.renameAsset(modelData.path, n)
                                onFinished: assetsGuest.editingPath = ""
                                Layout.fillWidth: true
                            }
                            Caption { text: (assetSearch.query ? (modelData.where || "Assets") + " · " : "") + assetsGuest.detailOf(modelData); Layout.fillWidth: true; elide: Text.ElideRight }
                        }
                        Rectangle { visible: !modelData.folder && modelData.uses > 0; width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                    }
                    AssetMouse {
                        enabled: assetsGuest.editingPath !== modelData.path
                        asset: modelData; previewItem: assetsGuest.previewItem; ghostItem: assetsGuest.ghostItem; overlayItem: assetsGuest.overlayItem
                        folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                        onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                        onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                        onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                    }
                }
            }
            GridView {
                id: assetGrid
                Accessible.role: Accessible.List
                Accessible.name: "Assets"
                visible: assetsGuest.view === 1
                anchors.fill: parent
                anchors.leftMargin: 6; anchors.rightMargin: 6; anchors.topMargin: 2; anchors.bottomMargin: 6
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                readonly property int cols: Math.max(2, Math.floor(width / 120))
                cellWidth: Math.floor(width / cols)
                cellHeight: Math.round(cellWidth * 0.62) + 52
                model: assetsGuest.items.filter(a => !a.folder)
                footer: dropHint
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; Accessible.name: "Scroll" }
                header: Item {
                    width: assetGrid.width
                    readonly property var folders: assetsGuest.items.filter(a => a.folder)
                    height: folders.length ? folderFlow.implicitHeight + 8 : 0
                    Flow {
                        id: folderFlow
                        objectName: "assetFolderFlow"
                        width: parent.width
                        Repeater {
                            model: parent.parent.folders
                            delegate: Item {
                                required property var modelData
                                width: assetGrid.cellWidth
                                height: Math.max(40, Math.round(assetGrid.cellHeight / 3))
                                Rectangle {
                                    readonly property bool sel: assetsGuest.isSel(modelData.path)
                                    anchors.fill: parent; anchors.margins: 4
                                    radius: Pal.radius
                                    readonly property bool target: assetsGuest.dropFolder === modelData.path
                                    color: target ? Pal.s6 : sel ? Pal.s5 : folderMouse.containsMouse ? Pal.s4 : Pal.s3
                                    border.width: sel || target ? 2 : 1
                                    border.color: sel || target ? Pal.focus : Pal.border
                                    Behavior on color { ColorAnimation { duration: Pal.fast } }
                                    RowLayout {
                                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 10
                                        spacing: 8
                                        FIcon { name: "folder"; size: 15; color: modelData.color || "#e9b949" }
                                        Text { visible: assetsGuest.editingPath !== modelData.path; text: modelData.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true }
                                        InlineRename {
                                            active: assetsGuest.editingPath === modelData.path
                                            original: modelData.name
                                            onRenamed: (n) => app.renameAsset(modelData.path, n)
                                            onFinished: assetsGuest.editingPath = ""
                                            Layout.fillWidth: true; font.pixelSize: Pal.caption
                                        }
                                        Rectangle { visible: modelData.uses > 0; width: 7; height: 7; radius: 3.5; color: "#22c55e" }
                                    }
                                    AssetMouse {
                                        enabled: assetsGuest.editingPath !== modelData.path
                                        id: folderMouse
                                        asset: modelData; previewItem: assetsGuest.previewItem; ghostItem: assetsGuest.ghostItem; overlayItem: assetsGuest.overlayItem
                                        folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                                        onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                                        onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                                        onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                                    }
                                }
                            }
                        }
                    }
                }
                delegate: Item {
                    required property var modelData
                    width: assetGrid.cellWidth
                    height: assetGrid.cellHeight
                    Rectangle {
                        id: tile
                        readonly property bool sel: assetsGuest.isSel(modelData.path)
                        anchors.fill: parent
                        anchors.margins: 4
                        radius: Pal.radius
                        color: sel ? Pal.s5 : tileMouse.containsMouse ? Pal.s4 : "transparent"
                        border.width: sel ? 2 : 0
                        border.color: Pal.focus
                        Behavior on color { ColorAnimation { duration: Pal.fast } }
                        Rectangle {
                            id: thumbBox
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 4 }
                            height: Math.round(width * 0.62)
                            radius: Pal.radius - 2
                            color: modelData.folder ? "transparent" : Pal.s1
                            border.width: modelData.folder ? 0 : 1; border.color: Pal.border
                            Image {
                                id: tileThumb
                                visible: !!modelData.thumb && status === Image.Ready
                                anchors.fill: parent; anchors.margins: 1
                                source: modelData.thumb || ""
                                sourceSize.width: 260
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                layer.enabled: assetsGuest.visible && tileThumb.visible
                                layer.effect: MultiEffect { maskEnabled: true; maskSource: tileMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                            }
                            Rectangle { id: tileMask; anchors.fill: tileThumb; radius: Pal.radius - 3; visible: false; layer.enabled: assetsGuest.visible && tileThumb.visible; layer.smooth: true }
                            FIcon {
                                visible: !tileThumb.visible
                                anchors.centerIn: parent
                                name: modelData.icon
                                size: modelData.folder ? 38 : 22
                                color: modelData.folder ? (modelData.color || "#e9b949") : Pal.muted
                            }
                            Rectangle {
                                visible: !modelData.folder && modelData.uses > 0
                                anchors { left: parent.left; top: parent.top; margins: 6 }
                                width: 9; height: 9; radius: 4.5; color: "#22c55e"
                                border.width: 2; border.color: "#cc000000"
                            }
                            Rectangle {
                                visible: !modelData.folder && (modelData.kind === "video" || modelData.kind === "audio" || modelData.kind === "other")
                                anchors { right: parent.right; bottom: parent.bottom; margins: 5 }
                                width: typeRow.implicitWidth + 10; height: 17; radius: 4; color: "#cc000000"
                                Row {
                                    id: typeRow
                                    anchors.centerIn: parent
                                    spacing: 3
                                    FIcon { visible: modelData.kind === "video"; name: "film"; size: 10; color: Pal.fg; anchors.verticalCenter: parent.verticalCenter }
                                    FIcon { visible: modelData.kind === "audio"; name: "audio-lines"; size: 10; color: Pal.fg; anchors.verticalCenter: parent.verticalCenter }
                                    Text {
                                        text: modelData.durationMs ? assetsGuest.duration(modelData.durationMs) : (modelData.ext || "")
                                        color: Pal.fg; font.family: Pal.font; font.pixelSize: 10; font.weight: 600
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                        InlineRename {
                            active: assetsGuest.editingPath === modelData.path
                            original: modelData.name
                            onRenamed: (n) => app.renameAsset(modelData.path, n)
                            onFinished: assetsGuest.editingPath = ""
                            anchors { left: parent.left; right: parent.right; top: thumbBox.bottom; topMargin: 5; leftMargin: 6; rightMargin: 6 }
                            font.pixelSize: Pal.caption; horizontalAlignment: TextInput.AlignHCenter
                        }
                        Text {
                            visible: assetsGuest.editingPath !== modelData.path
                            anchors { left: parent.left; right: parent.right; top: thumbBox.bottom; topMargin: 5; leftMargin: 6; rightMargin: 6 }
                            text: (assetSearch.query && modelData.where ? modelData.where.split("/").pop() + " / " : "") + modelData.name
                            color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 2
                            elide: Text.ElideRight
                            horizontalAlignment: Text.AlignHCenter
                            lineHeight: 1.05
                        }
                        AssetMouse {
                            enabled: assetsGuest.editingPath !== modelData.path
                            id: tileMouse
                            asset: modelData; previewItem: assetsGuest.previewItem; ghostItem: assetsGuest.ghostItem; overlayItem: assetsGuest.overlayItem
                            folderAt: assetsGuest.folderAt; hoverFolder: (f) => assetsGuest.dropFolder = f
                            onSelectRequested: (a, mods) => assetsGuest.selectItem(a, mods)
                            onMenuRequested: (a) => { if (!assetsGuest.isSel(a.path)) assetsGuest.selected = [a.path]; assetMenu.asset = a; assetMenu.popup() }
                            onMovedToFolder: (a, f) => app.moveAssets(assetsGuest.targets(a), f)
                        }
                    }
                }
            }
            Caption {
                visible: assetsGuest.items.length === 0
                anchors.centerIn: parent
                width: parent.width - 40
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: assetSearch.query ? "Nothing in assets matches \u201c" + assetSearch.text + "\u201d."
                    : assetsGuest.raw.length ? "No " + ["", "images", "videos", "audio files"][assetsGuest.typeFilter] + " here."
                    : app.assetPath ? chrome.keys("Empty folder. Drop files here or paste with Ctrl+V.")
                    : "Drop images, video or audio here."
            }
            DropArea {
                id: assetDrop
                anchors.fill: parent
                onEntered: (d) => d.accepted = d.hasUrls
                onDropped: (d) => { if (d.hasUrls) { app.importAssets(d.urls); d.acceptProposedAction() } }
            }
            Rectangle {
                visible: assetDrop.containsDrag
                anchors.fill: parent; anchors.margins: 6
                radius: Pal.radius + 2
                color: "#1a6b97ff"
                border.width: 2; border.color: Pal.focus
                Text { anchors.centerIn: parent; text: "Drop to copy into " + (app.assetPath ? app.assetPath.split("/").pop() : "assets"); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
            }
            FMenu {
                id: assetMenu
                property var asset: ({})
                readonly property var sel: assetsGuest.targets(asset)
                readonly property bool many: sel.length > 1
                FMenuItem { visible: !!assetMenu.asset.folder && !assetMenu.many; height: visible ? implicitHeight : 0; text: "Open"; onTriggered: app.openAssetFolder(assetMenu.asset.path) }
                FMenuItem { visible: !assetMenu.asset.folder && !assetMenu.many; height: visible ? implicitHeight : 0; text: "Place on " + app.editSceneName; enabled: !!assetMenu.asset.placeable; onTriggered: app.placeAsset(assetMenu.asset.path) }
                FMenuItem { visible: !assetMenu.many; height: visible ? implicitHeight : 0; text: "Rename  F2"; onTriggered: assetsGuest.rename(assetMenu.asset.path) }
                Item {
                    visible: !!assetMenu.asset.folder
                    width: parent ? parent.width : 220
                    height: visible ? 34 : 0
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 12
                        spacing: 6
                        Repeater {
                            model: ["", "#ef4444", "#f97316", "#22c55e", "#14b8a6", "#3b82f6", "#8b5cf6", "#ec4899", "#a3a3a3"]
                            delegate: Rectangle {
                                required property string modelData
                                readonly property bool current: (assetMenu.asset.color || "") === modelData
                                width: 18; height: 18; radius: 9
                                color: modelData || "#e9b949"
                                border.width: current ? 2 : 0
                                border.color: Pal.fg
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: { for (const p of assetMenu.sel) app.setFolderColor(p, modelData); assetMenu.close() }
                                }
                            }
                        }
                    }
                }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                FMenuItem { text: (assetMenu.many ? "Copy " + assetMenu.sel.length + " items" : "Copy") + chrome.keys("  Ctrl C"); onTriggered: app.copyAssets(assetMenu.sel) }
                FMenuItem { text: (assetMenu.many ? "Cut " + assetMenu.sel.length + " items" : "Cut") + chrome.keys("  Ctrl X"); onTriggered: app.cutAssets(assetMenu.sel) }
                FMenuItem { text: chrome.keys("Paste here  Ctrl V"); enabled: app.clipboardHasFiles(); onTriggered: app.pasteAssets() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                FMenuItem { visible: !assetMenu.many; height: visible ? implicitHeight : 0; text: "Show in File Explorer"; onTriggered: app.revealAsset(String(assetMenu.asset.path || "")) }
                FMenuItem {
                    text: assetMenu.asset.uses && !assetMenu.many ? "In use. Remove its layers first" : "Move to Recycle Bin  Del"
                    enabled: !(assetMenu.asset.uses && !assetMenu.many)
                    onTriggered: app.trashAssets(assetMenu.sel)
                }
            }
            FMenu {
                id: bgMenu
                FMenuItem { text: chrome.keys("New folder  Ctrl N"); onTriggered: assetsGuest.newFolder() }
                FMenuItem { text: chrome.keys("Paste  Ctrl V"); enabled: app.clipboardHasFiles(); onTriggered: app.pasteAssets() }
                FMenuItem { text: "Add files…"; onTriggered: assetPicker.open() }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                FMenuItem { text: "Show folder in File Explorer"; onTriggered: app.revealAsset(assetsGuest.currentFolderPath()) }
            }
        }
    }
}
