import QtQuick
import QtQuick.Layouts

// macOS privacy permissions the open show needs and doesn't have (see permissions.h): one row
// each, with the way to fix it. Empty, and so hidden, on Windows and once everything's allowed.
Rectangle {
    id: bar
    property bool dismissed: false // for this session; a newly missing permission brings it back
    readonly property var rows: permissions.missing
    onRowsChanged: if (rows.length === 0) dismissed = false
    visible: rows.length > 0 && !dismissed
    implicitHeight: visible ? col.implicitHeight + 12 : 0
    color: Pal.s3
    radius: Pal.radius
    border.color: "#4dfbbf24"

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 6 }
        spacing: 2
        Repeater {
            model: bar.rows
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                FIcon { name: "triangle-alert"; size: 15; color: "#fbbf24" }
                Text {
                    Layout.fillWidth: true
                    text: "<b>" + modelData.title + "</b>  " + modelData.detail
                    textFormat: Text.StyledText
                    color: Pal.fg
                    font.family: Pal.font
                    font.pixelSize: Pal.body
                    elide: Text.ElideRight
                }
                FButton {
                    text: modelData.action === "ask" ? "Allow…" : "Open System Settings"
                    variant: modelData.action === "ask" ? "primary" : "secondary"
                    onClicked: permissions.request(modelData.id)
                }
                FButton {
                    visible: index === 0
                    iconName: "x"
                    variant: "ghost"
                    tip: "Hide until LOBS restarts"
                    onClicked: bar.dismissed = true
                }
                Item { visible: index !== 0; implicitWidth: Pal.ctl }
            }
        }
    }
}
