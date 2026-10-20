import QtQuick
import QtQuick.Layouts

// Panel title row: subtitle style (13px / 550), optional meta text, actions on the right.
Item {
    id: h
    property string title
    property string meta
    default property alias actions: row.data
    property alias leading: lead.data // controls on the left (e.g. a view switch)
    Layout.fillWidth: true
    implicitHeight: 40
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 6
        spacing: 2
        RowLayout { id: lead; spacing: 2; visible: children.length > 0; Layout.leftMargin: -6 }
        Text { visible: h.title.length > 0; text: h.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550 }
        Text { visible: h.meta.length > 0; text: h.meta; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.leftMargin: 6 }
        Item { Layout.fillWidth: true }
        RowLayout { id: row; spacing: 2 }
    }
}
