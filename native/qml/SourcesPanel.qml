import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import LOBS
import LOBS.Models

Rectangle {
    id: poolGuest
    property Item previewItem: null
    property Item ghostItem: null
    property Item overlayItem: null
    signal addSourceRequested()
    signal deleteSourceRequested(var row)
    property size kddockwidgets_min_size: Qt.size(240, 160)
    property int filter: 0
    property bool searching: false
    clip: true
    color: "transparent"

    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        FPanelHeader {
            leading: [
                FTabs { model: ["All", "Video", "Audio"]; currentIndex: poolGuest.filter; onActivated: (i) => poolGuest.filter = i; Layout.leftMargin: 6 }
            ]
            FButton {
                iconName: "search"; variant: "ghost"; selected: poolGuest.searching
                tip: "Search sources"
                onClicked: { poolGuest.searching = !poolGuest.searching; if (poolGuest.searching) poolSearch.forceActiveFocus(); else poolSearch.text = "" }
            }
            FButton { iconName: "plus"; text: poolGuest.width < 330 ? "" : "Source"; variant: "secondary"; onClicked: poolGuest.addSourceRequested(); tip: "Add a source" }
        }
        FSearchField {
            id: poolSearch
            visible: poolGuest.searching
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
            implicitHeight: 26
            placeholderText: "Search by name or device"
            onDismissed: { poolGuest.searching = false; text = "" }
        }
        FluidList {
            id: poolList
            property string editingId: ""
            readonly property var levels: poolGuest.visible ? app.levels : ({})
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: RowsModel { rows: poolList.poolRowsFn() }
            readonly property var status: app.sourceStatus
            function poolRowsFn() {
                const st = (poolGuest.searching && poolSearch.text !== "") ? poolList.status : ({})
                const rows = app.pool.filter(r => !r.header && poolSearch.matches(r.name, (st[r.id] || {}).detail, r.kindLabel))
                const video = rows.filter(r => r.video), audio = rows.filter(r => !r.video)
                if (poolGuest.filter === 1) return video
                if (poolGuest.filter === 2) return audio
                return video.length && audio.length ? video.concat([{ divider: true }], audio) : video.concat(audio)
            }
            topMargin: 4
            delegate: Item {
                id: srow
                required property var modelData
                required property int index
                property bool hoverable: !modelData.divider
                readonly property var st: modelData.divider ? ({}) : (poolList.status[modelData.id] || ({}))
                readonly property bool problem: !!st.status
                readonly property bool inScene: !!modelData.inScene
                width: ListView.view.width
                height: modelData.divider ? 13 : 46
                Rectangle { visible: !!modelData.divider; anchors.centerIn: parent; width: parent.width - 24; height: 1; color: Pal.border }
                Rectangle {
                    visible: !modelData.divider && (modelData.selected || rowHover.hovered)
                    anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1
                    radius: Pal.radius - 2
                    color: modelData.selected ? Pal.selected : Pal.hover
                }
                HoverHandler { id: rowHover; enabled: !modelData.divider }
                RowLayout {
                    visible: !modelData.divider
                    anchors.fill: parent
                    anchors.leftMargin: 12; anchors.rightMargin: 8
                    spacing: 10
                    Rectangle {
                        implicitWidth: 52; implicitHeight: 30
                        radius: 4
                        color: Pal.s1
                        border.width: 1; border.color: srow.problem ? "#92400e" : Pal.border
                        clip: true
                        Image {
                            id: thumbImg
                            retainWhileLoading: true
                            property bool hadFrame: false
                            onStatusChanged: if (status === Image.Ready) hadFrame = true
                            onSourceChanged: if (!source.toString()) hadFrame = false
                            visible: modelData.video === true && hadFrame
                            anchors.fill: parent; anchors.margins: 1
                            fillMode: Image.PreserveAspectCrop
                            cache: false
                            asynchronous: true
                            sourceSize.width: 104
                            source: poolGuest.visible && modelData.video && srow.st.hasSize ? "image://thumbs/src:" + modelData.id + "?" + app.sourceTick : ""
                            layer.enabled: poolGuest.visible && thumbImg.visible
                            layer.effect: MultiEffect { maskEnabled: true; maskSource: thumbMask; maskThresholdMin: 0.5; maskSpreadAtMin: 1.0 }
                        }
                        Rectangle { id: thumbMask; anchors.fill: thumbImg; radius: 3; visible: false; layer.enabled: poolGuest.visible && thumbImg.visible; layer.smooth: true }
                        FIcon {
                            visible: !modelData.video || !thumbImg.hadFrame
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: modelData.video ? 0 : -4
                            name: srow.problem ? "triangle-alert" : (modelData.icon || "")
                            size: 14
                            color: srow.problem ? "#fbbf24" : Pal.muted
                        }
                        Row {
                            visible: !modelData.video
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 4 }
                            spacing: 2
                            Repeater {
                                model: 2
                                delegate: Rectangle {
                                    required property int index
                                    readonly property real db: (poolList.levels[modelData.id] || [-60, -60])[index]
                                    width: (parent.width - 2) / 2; height: 3; radius: 1.5; color: Pal.s4
                                    Rectangle {
                                        width: parent.width * Math.max(0, Math.min(1, (parent.db + 60) / 60)); height: parent.height; radius: 1.5
                                        color: modelData.muted ? Pal.muted : parent.db > -6 ? "#ef4444" : parent.db > -18 ? "#f59e0b" : "#22c55e"
                                    }
                                }
                            }
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Body { visible: poolList.editingId !== modelData.id; text: modelData.name || ""; font.weight: 550; Layout.fillWidth: true; color: modelData.muted ? Pal.muted : Pal.fg }
                        InlineRename {
                            active: poolList.editingId === modelData.id
                            original: modelData.name || ""
                            Layout.fillWidth: true
                            onRenamed: (n) => app.renameSource(modelData.id, n)
                            onFinished: poolList.editingId = ""
                        }
                        Row {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle {
                                visible: srow.problem
                                width: probText.implicitWidth + 12; height: 16; radius: 8
                                color: "#3a2a0a"; border.width: 1; border.color: "#92400e"
                                Text { id: probText; anchors.centerIn: parent; text: srow.st.statusText || ""; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: 10; font.weight: 600 }
                            }
                            Caption {
                                visible: !srow.problem
                                width: parent.width
                                elide: Text.ElideRight
                                text: srow.st.detail || modelData.kindLabel || ""
                            }
                        }
                    }
                    FButton {
                        visible: !modelData.video
                        iconName: modelData.muted ? "volume-x" : "volume-2"
                        iconColor: modelData.muted ? Pal.danger : Pal.muted
                        variant: "ghost"
                        tip: modelData.muted ? "Unmute" : "Mute"
                        onClicked: app.setAudio(modelData.id, "muted", !modelData.muted, true)
                    }
                    AbstractButton {
                        id: sceneToggle
                        visible: modelData.video === true
                        hoverEnabled: true
                        implicitHeight: Pal.ctl
                        implicitWidth: toggleRow.implicitWidth + 16
                        readonly property bool showAction: hovered || rowHover.hovered
                        onClicked: app.toggleInScene(modelData.id)
                        background: Rectangle {
                            radius: Pal.radius
                            color: sceneToggle.hovered ? (srow.inScene ? Pal.dangerBg : Pal.s6) : sceneToggle.showAction && !srow.inScene ? Pal.s5 : "transparent"
                            Behavior on color { ColorAnimation { duration: Pal.fast } }
                        }
                        contentItem: Item {
                            Row {
                                id: toggleRow
                                anchors.centerIn: parent
                                spacing: 5
                                Rectangle {
                                    visible: srow.inScene && !sceneToggle.hovered
                                    width: 6; height: 6; radius: 3; color: "#22c55e"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                FIcon {
                                    visible: !srow.inScene || sceneToggle.hovered
                                    name: srow.inScene ? "x" : "plus"
                                    size: 12
                                    color: srow.inScene && sceneToggle.hovered ? Pal.danger : Pal.fg
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: srow.inScene ? (sceneToggle.hovered ? "Remove" : "In scene") : (sceneToggle.showAction ? "Add to scene" : "")
                                    color: srow.inScene && !sceneToggle.hovered ? Pal.muted : srow.inScene ? Pal.danger : Pal.fg
                                    font.family: Pal.font; font.pixelSize: Pal.caption
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        ToolTip.visible: hovered
                        ToolTip.delay: 700
                        ToolTip.text: srow.inScene ? "Remove from " + app.editSceneName : "Add to " + app.editSceneName
                    }
                    FButton {
                        id: moreButton
                        iconName: "ellipsis"
                        variant: "ghost"
                        opacity: rowHover.hovered || poolMenu.visible ? 1 : 0.45
                        tip: "More"
                        onClicked: poolMenu.popup(moreButton, 0, moreButton.height + 4)
                    }
                }
                MouseArea {
                    Accessible.role: Accessible.ListItem
                    Accessible.name: String(modelData.name || "Source")
                    enabled: !modelData.divider && poolList.editingId !== modelData.id
                    anchors.fill: parent
                    anchors.rightMargin: 150
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    property point start
                    property bool dragging: false
                    function inPreview(p) { return poolGuest.previewItem && p.x >= 0 && p.y >= 0 && p.x <= poolGuest.previewItem.width && p.y <= poolGuest.previewItem.height }
                    onPressed: (m) => { start = Qt.point(m.x, m.y); dragging = false }
                    onPositionChanged: (m) => {
                        if (!(m.buttons & Qt.LeftButton) || !modelData.video) return
                        if (!dragging && Math.hypot(m.x - start.x, m.y - start.y) > 6) dragging = true
                        if (!dragging) return
                        const g = mapToItem(poolGuest.overlayItem, m.x, m.y)
                        if (poolGuest.ghostItem) poolGuest.ghostItem.show(modelData.icon, modelData.name, g.x, g.y)
                        const p = mapToItem(poolGuest.previewItem, m.x, m.y)
                        if (inPreview(p)) app.poolDragMove(modelData.id, p.x, p.y)
                        else app.poolDragCancel()
                    }
                    onReleased: (m) => {
                        if (!dragging) return
                        dragging = false
                        if (poolGuest.ghostItem) poolGuest.ghostItem.visible = false
                        const p = mapToItem(poolGuest.previewItem, m.x, m.y)
                        if (inPreview(p)) app.poolDrop(modelData.id, p.x, p.y)
                        else app.poolDragCancel()
                    }
                    onClicked: (m) => { app.selectSource(modelData.id); if (m.button === Qt.RightButton) poolMenu.popup() }
                    onDoubleClicked: if (modelData.video) app.placeSource(modelData.id)
                }
                FMenu {
                    id: poolMenu
                    FMenuItem {
                        visible: modelData.video === true
                        height: visible ? implicitHeight : 0
                        text: modelData.inScene ? "Remove from " + app.editSceneName : "Add to " + app.editSceneName
                        onTriggered: app.toggleInScene(modelData.id)
                    }
                    FMenuItem { text: "Settings…"; onTriggered: app.selectSource(modelData.id) }
                    FMenuItem { text: "Rename"; onTriggered: poolList.editingId = modelData.id }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem {
                        enabled: false
                        text: !modelData.video ? "Plays in every scene"
                            : (modelData.scenes || []).length ? "Used in " + modelData.scenes.join(", ") : "Not in any scene"
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem {
                        text: "Delete source"
                        onTriggered: (modelData.scenes || []).length > 1 ? poolGuest.deleteSourceRequested(modelData) : app.deleteSource(modelData.id)
                    }
                }
            }
            Caption {
                visible: poolList.count === 0
                anchors.centerIn: parent
                horizontalAlignment: Text.AlignHCenter
                text: poolSearch.query ? "No sources match \u201c" + poolSearch.text + "\u201d." : poolGuest.filter === 2 ? "No audio sources yet." : poolGuest.filter === 1 ? "No video sources yet." : "Add each camera, capture or mic once,\nthen drag it onto any scene."
            }
        }
    }
}
