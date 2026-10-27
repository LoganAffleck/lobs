import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// A browser source's page settings, all of OBS's, in two groups: the page (web address or file,
// and its size) always shows; how it runs (sound, frame rate, when hidden or live, access, CSS)
// folds into Advanced, whose header says what's set. The Add dialog and the Inspector both use
// it; each saves a change its own way (changed).
ColumnLayout {
    id: bo
    property var st: ({})
    property bool showRefresh: false // the Inspector: the page is running, so it can be reloaded
    property bool advancedOpen: app.uiFlag("browser/advancedOpen", false)
    signal changed(string key, var value)
    signal refreshRequested()
    spacing: 8
    Layout.fillWidth: true

    readonly property bool follows: st.responsive !== false
    readonly property int fps: st.fps || 0
    readonly property bool customCss: st.css !== undefined && st.css !== ""
    // What Advanced holds that isn't the default, so it can stay folded.
    readonly property string summary: {
        const out = []
        if (st.controlAudio === false) out.push("speakers")
        if (fps > 0) out.push(fps + " fps")
        if (st.shutdown) out.push("shuts down when hidden")
        if (st.restart) out.push("reloads on live")
        if (st.permissions !== undefined && st.permissions !== 1) out.push(["no access", "", "reads your info", "basic control", "advanced control", "full control"][st.permissions])
        if (customCss) out.push("CSS")
        return out.join(" · ")
    }

    component Label: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 72; elide: Text.ElideRight }

    // ---- the page ----
    RowLayout {
        Layout.fillWidth: true; spacing: 6
        FTabs { model: ["Web", "File"]; currentIndex: bo.st.local ? 1 : 0; onActivated: (i) => bo.changed("local", i === 1) }
        FInput {
            id: urlField
            visible: !bo.st.local
            Layout.fillWidth: true
            placeholderText: "https://"
            text: bo.st.url || ""
            rightPadding: bo.showRefresh ? 34 : 8
            onEditingFinished: if (text !== (bo.st.url || "")) bo.changed("url", text.trim())
            onTextEdited: urlTyping.restart()
            Timer { id: urlTyping; interval: 600; onTriggered: if (urlField.text !== (bo.st.url || "")) bo.changed("url", urlField.text.trim()) }
            FButton {
                visible: bo.showRefresh
                anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 2 }
                implicitHeight: parent.height - 4; implicitWidth: implicitHeight
                iconName: "refresh-cw"; variant: "ghost"
                tip: "Reload, ignoring the cache"
                onClicked: bo.refreshRequested()
            }
        }
        FInput {
            visible: !!bo.st.local
            Layout.fillWidth: true
            placeholderText: "Choose an .html file"
            text: bo.st.file || ""
            onEditingFinished: if (text !== (bo.st.file || "")) bo.changed("file", text.trim())
        }
        FButton { visible: !!bo.st.local; text: "Browse…"; variant: "secondary"; onClicked: pagePicker.open() }
        FButton { visible: !!bo.st.local && bo.showRefresh; iconName: "refresh-cw"; variant: "ghost"; tip: "Reload the page"; onClicked: bo.refreshRequested() }
        FileDialog {
            id: pagePicker
            fileMode: FileDialog.OpenFile
            nameFilters: ["Web pages (*.html *.htm)", "All files (*)"]
            onAccepted: bo.changed("file", String(selectedFile).replace(/^file:\/{3}/, ""))
        }
    }
    // Size: follows the layer (the page lays out like a resized browser window) or is set here.
    RowLayout {
        Layout.fillWidth: true; spacing: 6
        Label { text: "Size" }
        FInput {
            Layout.preferredWidth: 72
            enabled: !bo.follows
            placeholderText: "Canvas"
            validator: IntValidator { bottom: 1; top: 8192 }
            text: bo.st.width ? String(bo.st.width) : ""
            onEditingFinished: if (!bo.follows) bo.changed("width", parseInt(text) || undefined)
        }
        Text { text: "×"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
        FInput {
            Layout.preferredWidth: 72
            enabled: !bo.follows
            placeholderText: "Canvas"
            validator: IntValidator { bottom: 1; top: 8192 }
            text: bo.st.height ? String(bo.st.height) : ""
            onEditingFinished: if (!bo.follows) bo.changed("height", parseInt(text) || undefined)
        }
        FButton {
            iconName: bo.follows ? "link-2" : "unlink-2"
            text: bo.follows ? "Follows layer" : "Fixed"
            variant: "ghost"; selected: bo.follows
            tip: bo.follows ? "The page resizes with the layer. Click to set a fixed size."
                            : "The page keeps this size. Click to follow the layer."
            onClicked: bo.changed("responsive", !bo.follows)
        }
        Item { Layout.fillWidth: true }
    }

    // ---- advanced: folded, with what's set in its header ----
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border; Layout.topMargin: 2 }
    MouseArea {
        Layout.fillWidth: true
        implicitHeight: 26
        cursorShape: Qt.PointingHandCursor
        Accessible.role: Accessible.Button
        Accessible.name: "Advanced"
        onClicked: { bo.advancedOpen = !bo.advancedOpen; app.setUiFlag("browser/advancedOpen", bo.advancedOpen) }
        RowLayout {
            anchors.fill: parent; spacing: 6
            FIcon { name: bo.advancedOpen ? "chevron-down" : "chevron-right"; size: 12; color: Pal.muted }
            Text { text: "Advanced"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body }
            Text {
                Layout.fillWidth: true
                text: bo.summary || "Defaults"
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                elide: Text.ElideRight
            }
        }
    }
    GridLayout {
        visible: bo.advancedOpen
        Layout.fillWidth: true
        columns: 2; columnSpacing: 8; rowSpacing: 6
        Label { text: "Sound" }
        FSelect {
            Layout.fillWidth: true
            options: [{ name: "In the mixer (stream and recording)", value: true }, { name: "Your speakers only", value: false }]
            currentValue: bo.st.controlAudio !== false
            onPicked: (v) => bo.changed("controlAudio", v)
        }
        Label { text: "Frame rate" }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            FSelect {
                Layout.fillWidth: true
                options: [{ name: "Match the canvas", value: 0 }, { name: "30 fps", value: 30 }, { name: "60 fps", value: 60 }, { name: "Custom…", value: -1 }]
                currentValue: bo.fps === 0 ? 0 : bo.fps === 30 || bo.fps === 60 ? bo.fps : -1
                onPicked: (v) => bo.changed("fps", v === -1 ? (bo.fps > 0 && bo.fps !== 30 && bo.fps !== 60 ? bo.fps : 24) : v)
            }
            FNumField {
                visible: bo.fps > 0 && bo.fps !== 30 && bo.fps !== 60
                Layout.preferredWidth: 80
                suffix: " fps"; from: 1; to: 240
                value: bo.fps || 24
                onCommit: (v) => bo.changed("fps", Math.round(v))
            }
        }
        Label { text: "When hidden" }
        FSelect {
            Layout.fillWidth: true
            options: [{ name: "Keep running", value: false }, { name: "Shut down (reloads when shown)", value: true }]
            currentValue: !!bo.st.shutdown
            onPicked: (v) => bo.changed("shutdown", v)
        }
        Label { text: "When live" }
        FSelect {
            Layout.fillWidth: true
            options: [{ name: "Do nothing", value: false }, { name: "Reload the page", value: true }]
            currentValue: !!bo.st.restart
            onPicked: (v) => bo.changed("restart", v)
        }
        Label { text: "Page access" }
        FSelect {
            Layout.fillWidth: true
            options: [
                { name: "None", value: 0 },
                { name: "Read LOBS status", value: 1 },
                { name: "Read your info", value: 2 },
                { name: "Basic control of LOBS", value: 3 },
                { name: "Advanced control of LOBS", value: 4 },
                { name: "Full control of LOBS", value: 5 }]
            currentValue: bo.st.permissions !== undefined ? bo.st.permissions : 1
            onPicked: (v) => bo.changed("permissions", v)
        }
        Label { text: "CSS"; Layout.alignment: Qt.AlignTop; Layout.topMargin: 6 }
        ColumnLayout {
            Layout.fillWidth: true; spacing: 4
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                FButton {
                    text: bo.customCss ? "Custom (" + bo.st.css.split("\n").length + (bo.st.css.split("\n").length === 1 ? " line)" : " lines)") : "Default"
                    iconName: cssBox.visible ? "chevron-up" : "pencil"
                    variant: "ghost"
                    tip: "Edit the page's CSS"
                    onClicked: cssBox.visible = !cssBox.visible
                }
                FButton { visible: bo.customCss; text: "Reset"; variant: "ghost"; tip: "Reset to default (transparent page)"; onClicked: bo.changed("css", undefined) }
                Item { Layout.fillWidth: true }
            }
            Rectangle {
                id: cssBox
                visible: false
                Layout.fillWidth: true
                implicitHeight: 96
                radius: Pal.radius
                color: Pal.s1
                border.width: 1
                border.color: css.activeFocus ? Pal.focus : Pal.border
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 1
                    TextArea {
                        id: css
                        text: bo.st.css || ""
                        placeholderText: "Default: a transparent page with no margins or scrollbars"
                        placeholderTextColor: Pal.muted
                        wrapMode: TextEdit.Wrap
                        color: Pal.fg
                        font.family: Pal.mono; font.pixelSize: Pal.caption
                        selectByMouse: true
                        background: null
                        onEditingFinished: if (text !== (bo.st.css || "")) bo.changed("css", text === "" ? undefined : text)
                    }
                }
            }
        }
        Item { visible: bo.showRefresh; implicitWidth: 1 }
        FButton { visible: bo.showRefresh; Layout.alignment: Qt.AlignLeft; iconName: "refresh-cw"; text: "Refresh cache"; variant: "ghost"; onClicked: bo.refreshRequested() }
    }
}
