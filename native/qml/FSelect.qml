import QtQuick
import QtQuick.Controls

// Select: a field-styled button that opens an FMenu of options ([{name, value}]).
FButton {
    id: sel
    property var options: []
    property var currentValue
    property string placeholder: "Choose…"
    signal picked(var value)
    variant: "tertiary"
    pressDip: false
    readonly property var current: options.find(o => o.value === currentValue)
    text: current ? current.name : placeholder
    leftPadding: 10
    rightPadding: 8
    contentItem: Item {
        implicitWidth: 120
        Text {
            anchors.left: parent.left; anchors.right: chev.left; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
            text: sel.text; elide: Text.ElideRight
            color: sel.current ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
        }
        Text { id: chev; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "▾"; color: Pal.muted; font.pixelSize: 10 }
    }
    onClicked: menu.popup(sel, 0, sel.height + 4)
    FMenu {
        id: menu
        width: Math.max(sel.width, 220)
        Instantiator {
            model: sel.options
            delegate: FMenuItem {
                required property var modelData
                text: modelData.name
                selected: modelData.value === sel.currentValue
                onTriggered: sel.picked(modelData.value)
            }
            onObjectAdded: (i, o) => menu.insertItem(i, o)
            onObjectRemoved: (i, o) => menu.removeItem(o)
        }
    }
}
