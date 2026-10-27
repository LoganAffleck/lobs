import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import LOBS.Models

// A plugin source's or filter's own settings, as OBS lists them (App::obsProperties). Listed
// again after every edit: a plugin can show, hide or change its settings when one changes.
// Plugins' own windows (Qt dialogs, docks) aren't shown; only what obs_properties describes.
ColumnLayout {
    id: op
    property string scope          // "source" | "filter" | "audio"
    property string targetId       // source id, or layer id for "filter"
    property int index: -1         // filter position for "filter" and "audio"
    property bool active: true
    property var rows: []
    spacing: 6
    Layout.fillWidth: true

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }

    function refresh() { rows = active && targetId ? app.obsProperties(scope, targetId, index) : [] }
    function set(key, v, final) { app.setObsProperty(scope, targetId, index, key, v, final) }
    onTargetIdChanged: again.restart()
    onIndexChanged: again.restart()
    onActiveChanged: again.restart()
    Component.onCompleted: again.restart()
    // After the engine has applied the edit, so the list reflects the plugin's new state.
    Timer { id: again; interval: 30; onTriggered: op.refresh() }
    Connections { target: app; function onDocChanged() { if (op.active) again.restart() } }

    FileDialog {
        id: fileDlg
        property string key
        onAccepted: op.set(key, decodeURIComponent(selectedFile.toString().replace(/^file:\/{3}/, "")), true)
    }
    FolderDialog {
        id: folderDlg
        property string key
        onAccepted: op.set(key, decodeURIComponent(selectedFolder.toString().replace(/^file:\/{3}/, "")), true)
    }

    Repeater {
        model: RowsModel { rows: op.rows } // rows stay alive across edits, so a slider survives its own drag
        delegate: Loader {
            id: row
            required property var modelData
            readonly property var p: modelData
            Layout.fillWidth: true
            Layout.leftMargin: (p.depth || 0) * 10
            visible: p.visible
            active: p.visible
            enabled: p.enabled
            opacity: p.enabled ? 1 : 0.5
            sourceComponent: ({ bool: boolRow, number: numRow, text: textRow, info: infoRow, path: pathRow, list: listRow,
                                color: colorRow, font: fontRow, button: buttonRow, group: groupRow })[p.type] || unsupportedRow
            Component {
                id: boolRow
                FSwitch { text: row.p.label; checkable: false; checked: !!row.p.value; onClicked: op.set(row.p.key, !row.p.value, true) }
            }
            Component {
                id: numRow
                Item {
                    implicitHeight: (useSlider ? slider : field).implicitHeight
                    // A slider when the plugin asks for one or the range is small; else a number field.
                    readonly property bool useSlider: row.p.slider || (row.p.max - row.p.min) <= 1000
                    FFillSlider {
                        id: slider
                        visible: parent.useSlider
                        width: parent.width
                        label: row.p.label
                        from: row.p.min; to: row.p.max; decimals: row.p.decimals; suffix: row.p.suffix || ""
                        value: Number(row.p.value)
                        onMoved: (v) => op.set(row.p.key, v, false)
                        onReleased: (v) => op.set(row.p.key, v, true)
                    }
                    FNumField {
                        id: field
                        visible: !parent.useSlider
                        width: parent.width
                        label: row.p.label; suffix: row.p.suffix || ""
                        from: row.p.min; to: row.p.max; step: row.p.step || 1
                        value: Number(row.p.value)
                        onCommit: (v) => op.set(row.p.key, v, true)
                    }
                }
            }
            Component {
                id: textRow
                ColumnLayout {
                    spacing: 3
                    Caption { text: row.p.label; visible: text !== "" }
                    FInput {
                        Layout.fillWidth: true
                        text: String(row.p.value)
                        echoMode: row.p.password ? TextInput.Password : TextInput.Normal
                        onEditingFinished: if (text !== String(row.p.value)) op.set(row.p.key, text, true)
                    }
                }
            }
            Component {
                id: infoRow
                Caption { text: row.p.label + (row.p.value ? "\n" + row.p.value : "") }
            }
            Component {
                id: pathRow
                ColumnLayout {
                    spacing: 3
                    Caption { text: row.p.label; visible: text !== "" }
                    RowLayout {
                        spacing: 6
                        FInput {
                            Layout.fillWidth: true
                            text: String(row.p.value)
                            onEditingFinished: if (text !== String(row.p.value)) op.set(row.p.key, text, true)
                        }
                        FButton {
                            iconName: "folder-open"; variant: "ghost"; tip: "Choose " + row.p.label.toLowerCase()
                            onClicked: {
                                if (row.p.folder) {
                                    folderDlg.key = row.p.key; folderDlg.title = row.p.label; folderDlg.open()
                                } else {
                                    fileDlg.key = row.p.key; fileDlg.title = row.p.label
                                    fileDlg.fileMode = row.p.save ? FileDialog.SaveFile : FileDialog.OpenFile
                                    fileDlg.nameFilters = row.p.filters
                                    fileDlg.open()
                                }
                            }
                        }
                    }
                }
            }
            Component {
                id: listRow
                RowLayout {
                    spacing: 6
                    Caption { text: row.p.label; Layout.fillWidth: false; Layout.preferredWidth: 90; elide: Text.ElideRight; wrapMode: Text.NoWrap }
                    FSelect {
                        Layout.fillWidth: true
                        options: row.p.options
                        currentValue: String(row.p.value)
                        placeholder: String(row.p.value)
                        onPicked: (v) => op.set(row.p.key, v, true)
                    }
                }
            }
            Component {
                id: colorRow
                RowLayout {
                    spacing: 6
                    Caption { text: row.p.label }
                    FColorField { color: String(row.p.value); onPicked: (c) => op.set(row.p.key, c, true) }
                }
            }
            Component {
                id: fontRow
                ColumnLayout {
                    spacing: 3
                    Caption { text: row.p.label }
                    FFontPicker { Layout.fillWidth: true; currentFont: String(row.p.value); onPicked: (f) => op.set(row.p.key, f, true) }
                }
            }
            Component {
                id: buttonRow
                FButton {
                    text: row.p.label; variant: "secondary"
                    onClicked: row.p.url ? Qt.openUrlExternally(row.p.url) : app.clickObsProperty(op.scope, op.targetId, op.index, row.p.key)
                }
            }
            Component {
                id: groupRow
                RowLayout {
                    spacing: 6
                    Text { text: row.p.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true; Layout.topMargin: 4 }
                    FSwitch { visible: !!row.p.checkable; checkable: false; checked: !!row.p.value; onClicked: op.set(row.p.key, !row.p.value, true) }
                }
            }
            Component {
                id: unsupportedRow
                Caption { text: row.p.label + ": can't be edited in LOBS yet." }
            }
        }
    }
    Caption { visible: op.active && op.rows.length === 0; text: "This plugin has no settings LOBS can show." }
}
