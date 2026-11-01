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
    implicitHeight: Pal.ctl + 4 // as tall as toggle tiles and fields beside it
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
        FIcon { id: chev; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; name: "chevron-down"; size: 14; color: Pal.muted } // the same chevron as the Inspector's sections
    }
    onClicked: menu.popup(sel, 0, sel.height + 4)
    FMenu {
        id: menu
        width: Math.max(sel.width, 220)
        Instantiator {
            active: menu.populated // built while the menu is open
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
