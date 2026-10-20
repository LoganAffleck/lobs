import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Top of the layer inspector: the layer's source.
//  - Shared inputs (camera, capture, browser): a collapsible section, closed by default, that
//    says it's shared ("used in 3 scenes") and summarizes the input in one line when closed.
//  - Layers that are their own content (text, color, image or video file): the fields, inline.
ColumnLayout {
    id: ss
    property var src: ({})       // source info for the selected layer
    readonly property bool shared: !["text", "color", "image", "media", "shape"].includes(src.kind)
    readonly property var status: app.sourceStatus[src.id] || ({})
    property bool open: app.uiFlag("inspector/sourceOpen", false)
    readonly property bool showFields: !shared || open
    Layout.fillWidth: true
    spacing: 0
    property bool topDivider: true // off when it starts the panel (the tab bar draws that line)
    Rectangle { visible: ss.topDivider; Layout.fillWidth: true; height: 1; color: Pal.border }

    // Header (clickable when collapsible)
    Item {
        Layout.fillWidth: true
        implicitHeight: headCol.implicitHeight + 18
        Rectangle { anchors.fill: parent; color: headMouse.containsMouse && ss.shared ? Pal.hover : "transparent" }
        MouseArea {
            id: headMouse
            anchors.fill: parent
            enabled: ss.shared
            hoverEnabled: true
            cursorShape: ss.shared ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: { ss.open = !ss.open; app.setUiFlag("inspector/sourceOpen", ss.open) }
        }
        ColumnLayout {
            id: headCol
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 12 }
            spacing: 3
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                FIcon { visible: ss.shared; name: ss.open ? "chevron-down" : "chevron-right"; size: 12; color: Pal.muted }
                FIcon { name: ss.src.icon || ""; size: 14; color: Pal.fg }
                Text {
                    text: ss.shared ? "Source · " + (ss.src.name || "") : (ss.src.kindLabel || "Content")
                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550
                    elide: Text.ElideRight; Layout.fillWidth: true
                }
                // Shared: say so, in the same accent as linked sections.
                Rectangle {
                    visible: ss.shared || (ss.src.sceneCount || 0) > 1
                    implicitWidth: sharedRow.implicitWidth + 12; implicitHeight: 18; radius: 9
                    color: "#1a7aa7ff"; border.width: 1; border.color: "#407aa7ff"
                    Row {
                        id: sharedRow
                        anchors.centerIn: parent
                        spacing: 4
                        FIcon { name: "link-2"; size: 10; color: Pal.link; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: (ss.src.sceneCount || 0) > 1 ? "Used in " + ss.src.sceneCount + " scenes" : "Shared input"
                            color: Pal.link; font.family: Pal.font; font.pixelSize: 10; font.weight: 600
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    HoverHandler { id: sharedHover }
                    ToolTip.visible: sharedHover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: "Changes this input everywhere" + ((ss.src.scenes || []).length ? ": " + ss.src.scenes.join(", ") : "") + ". Size, crop and filters are per scene."
                }
            }
            // Closed: one line that usually answers the question (or the problem).
            Row {
                visible: ss.shared && !ss.open
                Layout.leftMargin: 38
                spacing: 6
                Rectangle {
                    visible: !!ss.status.status
                    width: probText.implicitWidth + 12; height: 16; radius: 8
                    color: "#3a2a0a"; border.width: 1; border.color: "#92400e"
                    Text { id: probText; anchors.centerIn: parent; text: ss.status.statusText || ""; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: 10; font.weight: 600 }
                }
                Text {
                    visible: !ss.status.status
                    text: ss.status.detail || ss.src.kindLabel || ""
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                }
            }
        }
    }
    ColumnLayout {
        visible: ss.showFields
        Layout.fillWidth: true
        Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.bottomMargin: 14
        spacing: 8
        SourceFields { d: ss.src; active: ss.showFields }
        RowLayout {
            visible: ss.shared
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: (ss.src.sceneCount || 0) > 1 ? "Changes here apply in every scene that uses " + ss.src.name + "." : "Changes here apply wherever " + ss.src.name + " is used."
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            }
            FButton { text: "Open in Sources"; variant: "ghost"; tip: "Show this input in the Sources panel"; onClicked: app.selectSource(ss.src.id) }
        }
    }
}
