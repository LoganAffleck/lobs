import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Names a source's effects as a preset of your own, listed under + Effect → Start from a preset.
FDialog {
    id: dlg
    title: "Save as a preset"
    width: 360
    z: 1100 // over the mic wizard
    property string sourceId: ""
    function ask(id, name) { sourceId = id; nameIn.text = name ? name + " sound" : "My mic"; open() }
    function save() { if (nameIn.text.trim() && app.saveAudioPreset(sourceId, nameIn.text)) close() }
    onOpened: { nameIn.selectAll(); nameIn.forceActiveFocus() }
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        FInput { id: nameIn; Layout.fillWidth: true; placeholderText: "Name"; onAccepted: dlg.save() }
        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            FButton { text: "Cancel"; variant: "ghost"; onClicked: dlg.close() }
            FButton { text: "Save"; variant: "primary"; enabled: nameIn.text.trim().length > 0; onClicked: dlg.save() }
        }
    }
}
