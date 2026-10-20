import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Inspector section. The header shows only what's actionable for this section:
//  - a link button when the same source is placed in other scenes (link this section with one),
//  - a "Linked · Scene" chip when it's shared (shows where; Unlink from its menu),
//  - a reset button when the values differ from the defaults.
ColumnLayout {
    id: sec
    property string title
    property string group
    property string linkedWith            // kept for compatibility; derived from `sections` below
    property var sections: ({})
    default property alias content: body.data
    readonly property var info: sections && sections[group] ? sections[group] : null
    readonly property var peers: info ? info.peers : []
    readonly property var candidates: info ? info.candidates : []
    readonly property bool linked: peers.length > 0
    Layout.fillWidth: true
    spacing: 0
    Rectangle { Layout.fillWidth: true; height: 1; color: Pal.border }
    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.topMargin: 10; Layout.bottomMargin: 14
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Pal.ctl
            spacing: 2
            Text { text: sec.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.fillWidth: true }

            FButton {
                visible: sec.info !== null && !sec.info.isDefault
                iconName: "rotate-ccw"
                variant: "ghost"
                tip: "Reset " + sec.title.toLowerCase()
                onClicked: app.resetGroup(sec.group)
            }
            // Linked: chip naming where it's shared; its menu lists the scenes and offers Unlink.
            FButton {
                id: linkedChip
                visible: sec.linked
                iconName: "link-2"
                iconColor: Pal.link
                text: "Linked · " + (sec.peers.length === 1 ? sec.peers[0].scene : sec.peers.length + " scenes")
                variant: "ghost"
                tip: "Shared with other scenes"
                onClicked: linkedMenu.popup(linkedChip, 0, linkedChip.height + 4)
                FMenu {
                    id: linkedMenu
                    Instantiator {
                        model: sec.peers
                        delegate: FMenuItem { required property var modelData; text: "Shared with " + modelData.scene; enabled: false }
                        onObjectAdded: (i, o) => linkedMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => linkedMenu.removeItem(o)
                    }
                    MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                    FMenuItem { text: "Unlink in this scene"; onTriggered: app.unlinkGroup(sec.group) }
                }
            }
            // Not linked, but linkable: the same source is placed elsewhere.
            FButton {
                id: linkButton
                visible: !sec.linked && sec.candidates.length > 0
                iconName: "link-2"
                variant: "ghost"
                tip: "Link " + sec.title.toLowerCase() + " with this source in another scene"
                onClicked: linkMenu.popup(linkButton, 0, linkButton.height + 4)
                FMenu {
                    id: linkMenu
                    width: 260
                    Instantiator {
                        model: sec.candidates
                        delegate: FMenu {
                            required property var modelData
                            title: "Link with " + modelData.scene
                            FMenuItem { text: "Keep this scene's values"; onTriggered: app.linkGroup(sec.group, modelData.id, true) }
                            FMenuItem { text: "Use " + modelData.scene + "'s values"; onTriggered: app.linkGroup(sec.group, modelData.id, false) }
                        }
                        onObjectAdded: (i, o) => linkMenu.insertMenu(i, o)
                        onObjectRemoved: (i, o) => linkMenu.removeMenu(o)
                    }
                }
            }
        }
        ColumnLayout { id: body; Layout.fillWidth: true; spacing: 6 }
    }
}
