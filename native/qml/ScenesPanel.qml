import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOBS
import LOBS.Models

Rectangle {
    id: scenesGuest
    property int sceneView: 0
    signal viewPicked(int view)
    property string editingId: ""
    property size kddockwidgets_min_size: Qt.size(240, 160)
    property bool searching: false
    color: "transparent"

    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    RowsModel { id: sceneRows; rows: sceneSearch.query ? app.scenes.filter(r => sceneSearch.matches(r.name)) : app.scenes }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        FPanelHeader {
            leading: [
                FButton { iconName: "list"; variant: "ghost"; selected: scenesGuest.sceneView === 0; tip: "List"; onClicked: scenesGuest.viewPicked(0) },
                FButton { iconName: "layout-grid"; variant: "ghost"; selected: scenesGuest.sceneView === 1; tip: "Grid with live thumbnails"; onClicked: scenesGuest.viewPicked(1) }
            ]
            FButton {
                iconName: "search"; variant: "ghost"; selected: scenesGuest.searching
                tip: "Search scenes"
                onClicked: { scenesGuest.searching = !scenesGuest.searching; if (scenesGuest.searching) sceneSearch.forceActiveFocus(); else sceneSearch.text = "" }
            }
            FButton { iconName: "plus"; text: "Scene"; variant: "secondary"; onClicked: app.addScene(); tip: "Add a scene" }
        }
        FSearchField {
            id: sceneSearch
            visible: scenesGuest.searching
            Layout.fillWidth: true
            Layout.leftMargin: 10; Layout.rightMargin: 10; Layout.bottomMargin: 4
            implicitHeight: 26
            placeholderText: "Search scenes"
            onDismissed: { scenesGuest.searching = false; text = "" }
        }
        FMenu {
            id: sceneMenu
            property var row: ({})
            function openFor(r) { row = r; popup() }
            FMenuItem { text: "Show this scene"; onTriggered: { app.editScene(sceneMenu.row.id); app.take() } }
            FMenuItem { visible: app.clickToShow; height: visible ? implicitHeight : 0; text: "Edit before showing"; onTriggered: app.editScene(sceneMenu.row.id) }
            FMenuItem { text: "Rename"; onTriggered: scenesGuest.editingId = sceneMenu.row.id }
            FMenuItem { text: "Show in " + app.editSceneName; enabled: sceneMenu.row.id !== app.editSceneId; onTriggered: app.addSceneSource(sceneMenu.row.id) }
            FMenuItem { text: "Duplicate with linked layers"; onTriggered: app.duplicateScene(sceneMenu.row.id, true) }
            FMenuItem { text: "Duplicate"; onTriggered: app.duplicateScene(sceneMenu.row.id, false) }
            MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
            FMenuItem { text: "Delete scene"; enabled: app.scenes.length > 1; onTriggered: app.deleteScene(sceneMenu.row.id) }
        }
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: scenesGuest.sceneView
            FluidList {
                id: sceneList
                model: sceneRows
                topMargin: 2
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: Pal.ctl + 2
                    Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 1; anchors.bottomMargin: 1; radius: Pal.radius - 2; color: Pal.selected; visible: modelData.editing }
                    MouseArea {
                        Accessible.role: Accessible.ListItem
                        Accessible.name: String(modelData.name || "Scene")
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (m) => { if (m.button === Qt.RightButton) sceneMenu.openFor(modelData); else app.pickScene(modelData.id) }
                        onDoubleClicked: { app.editScene(modelData.id); app.take() }
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 12
                        spacing: 6
                        Body { visible: scenesGuest.editingId !== modelData.id; text: modelData.name; font.weight: modelData.editing ? 550 : 400; Layout.fillWidth: true }
                        InlineRename {
                            active: scenesGuest.editingId === modelData.id
                            original: modelData.name
                            Layout.fillWidth: true
                            onRenamed: (n) => app.renameScene(modelData.id, n)
                            onFinished: scenesGuest.editingId = ""
                        }
                        FBadge { visible: modelData.program; text: (app.streaming || app.recording) ? "On air" : "Showing"; tone: (app.streaming || app.recording) ? "live" : "neutral" }
                        Caption { text: modelData.count }
                    }
                }
            }
            GridView {
                id: sceneGrid
                Accessible.role: Accessible.List
                Accessible.name: "Scenes"
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                boundsMovement: Flickable.StopAtBounds
                flickDeceleration: 6000
                maximumFlickVelocity: 3000
                model: scenesGuest.sceneView === 1 ? sceneRows : null
                leftMargin: 8; rightMargin: 8; topMargin: 4
                cellWidth: (width - 16) / 2
                cellHeight: cellWidth * 9 / 16 + 30
                delegate: Item {
                    required property var modelData
                    width: sceneGrid.cellWidth
                    height: sceneGrid.cellHeight
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: Pal.radius
                        color: tileMouse.containsMouse ? Pal.s4 : Pal.s3
                        border.width: modelData.editing || modelData.program ? 2 : 1
                        border.color: modelData.program ? Pal.live : modelData.editing ? Pal.focus : Pal.border
                        Behavior on color { ColorAnimation { duration: Pal.fast } }
                        Rectangle {
                            id: thumbBox
                            x: 3; y: 3; width: parent.width - 6; height: width * 9 / 16
                            radius: Pal.radius - 3; color: "black"; clip: true
                            Image {
                                anchors.fill: parent
                                cache: false
                                fillMode: Image.PreserveAspectCrop
                                sourceSize.width: 320
                                source: scenesGuest.sceneView === 1 ? "image://thumbs/" + modelData.id + "?" + app.thumbTick : ""
                            }
                        }
                        RowLayout {
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: thumbBox.bottom
                            anchors.leftMargin: 8; anchors.rightMargin: 6; anchors.topMargin: 4
                            Body { visible: scenesGuest.editingId !== modelData.id; text: modelData.name; font.weight: modelData.editing ? 550 : 400; Layout.fillWidth: true }
                            InlineRename {
                                active: scenesGuest.editingId === modelData.id
                                original: modelData.name
                                Layout.fillWidth: true
                                onRenamed: (n) => app.renameScene(modelData.id, n)
                                onFinished: scenesGuest.editingId = ""
                            }
                            FBadge { visible: modelData.program; text: (app.streaming || app.recording) ? "On air" : "Showing"; tone: (app.streaming || app.recording) ? "live" : "neutral" }
                        }
                        MouseArea {
                            Accessible.role: Accessible.ListItem
                            Accessible.name: String(modelData.name || "Scene")
                            id: tileMouse
                            anchors.fill: parent
                            z: -1
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (m) => { if (m.button === Qt.RightButton) sceneMenu.openFor(modelData); else app.pickScene(modelData.id) }
                            onDoubleClicked: { app.editScene(modelData.id); app.take() }
                        }
                    }
                }
            }
        }
    }
}
