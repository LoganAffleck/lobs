import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// New project: a name and a folder. Creates <folder>/<name>/<name>.lobs.json plus its assets
// folder. The project you're leaving is saved first; an unsaved show gets a warning and a way
// to save it before it's replaced.
FDialog {
    id: dlg
    title: "New project"
    width: 520
    signal saveCurrentAs()
    readonly property bool currentSaved: app.projectFile !== ""
    readonly property string problem: app.checkNewProject(nameField.text, folderField.text)
    readonly property string projectPath: folderField.text.replace(/[\\/]+$/, "") + "\\" + nameField.text.trim()
    function create() { if (!problem && app.createProject(nameField.text, folderField.text)) close() }
    onOpened: {
        nameField.text = "Untitled project"
        if (!folderField.text) folderField.text = app.uiValue("projects/folder", app.defaultProjectsFolder())
        nameField.selectAll(); nameField.forceActiveFocus()
    }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    contentItem: ColumnLayout {
        spacing: 12
        // Leaving the current show.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: leaving.implicitHeight + 20
            radius: Pal.radius
            color: dlg.currentSaved ? Pal.s4 : "#2a2212"
            border.width: 1; border.color: dlg.currentSaved ? Pal.border : "#6b5320"
            RowLayout {
                id: leaving
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 12; rightMargin: 8 }
                spacing: 10
                FIcon { name: dlg.currentSaved ? "check" : "triangle-alert"; size: 15; color: dlg.currentSaved ? Pal.muted : "#facc15" }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
                    text: dlg.currentSaved
                          ? "Your current project is saved before the new one opens."
                          : "Your current show isn't saved as a project. Save it first, or it will be replaced."
                }
                FButton { visible: !dlg.currentSaved; text: "Save as…"; variant: "secondary"; onClicked: dlg.saveCurrentAs() }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 6
            Caption { text: "Name" }
            FInput { id: nameField; Layout.fillWidth: true; onAccepted: dlg.create() }
        }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 6
            Caption { text: "Location" }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                FInput { id: folderField; Layout.fillWidth: true; onAccepted: dlg.create() }
                FButton { text: "Browse…"; variant: "secondary"; onClicked: folderPicker.open() }
            }
        }
        Caption {
            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
            color: dlg.problem && nameField.text ? Pal.danger : Pal.muted
            text: dlg.problem ? dlg.problem : "Creates " + dlg.projectPath + " with the show file and an assets folder."
        }
    }
    footer: RowLayout {
        spacing: 6
        Item { Layout.fillWidth: true }
        FButton { text: "Cancel"; variant: "ghost"; onClicked: dlg.close(); Layout.bottomMargin: 14 }
        FButton {
            text: "Create project"; variant: "primary"
            enabled: !dlg.problem
            onClicked: { app.setUiValue("projects/folder", folderField.text); dlg.create() }
            Layout.rightMargin: 16; Layout.bottomMargin: 14
        }
    }
    FolderDialog {
        id: folderPicker
        title: "Where to keep the project"
        onAccepted: folderField.text = decodeURIComponent(selectedFolder.toString().replace(/^file:\/{3}/, "")).replace(/\//g, "\\")
    }
}
