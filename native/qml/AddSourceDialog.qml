import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import LOBS
import LOBS.Web

FDialog {
    id: addDialog
    title: "Add a source"
    width: 600
    signal tuneMicRequested(string micId)

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }

    property string kind: app.uiValue("addSource/kind", "camera")
    readonly property bool picksDevice: ["camera", "display", "window", "mic", "desktop_audio"].indexOf(kind) >= 0
    readonly property bool audioKind: kind === "mic" || kind === "desktop_audio"
    property var devices: []
    property string device: ""
    property string deviceName: ""
    property bool mono: false
    property var browserSt: ({})
    property bool adding: false
    property var goodLists: ({})
    property int retries: 0
    Timer { id: deviceRetry; interval: 400; onTriggered: addDialog.refreshDevices(true) }
    function refreshDevices(retry) {
        const k = kind
        if (!retry) { retries = 0; deviceRetry.stop() }
        let list = picksDevice ? app.devicesForKind(k) : []
        if (k !== kind)
            return
        if (list.length) {
            const g = Object.assign({}, goodLists); g[k] = list; goodLists = g
        } else if (picksDevice && k !== "window") {
            if (retries++ < 5) deviceRetry.restart()
            list = goodLists[k] || []
        }
        if (retry && list.length && devices.length && device)
            return
        const mine = d => onlyOnce && app.sourceUsingDevice(k, d.value) !== ""
        devices = list.map(d => mine(d) ? Object.assign({}, d, { name: d.name + " (in your show)" })
                              : d.inUse ? Object.assign({}, d, { name: d.name + " (in use)" }) : d)
        const real = d => d.name.toLowerCase().indexOf("virtual") < 0
        const first = list.find(d => real(d) && !d.inUse && !mine(d)) || list.find(d => real(d) && !mine(d)) || list.find(real) || list[0]
        device = k !== "window" && first ? first.value : ""
        deviceName = k !== "window" && first ? first.name : ""
    }
    function setBrowser(key, value) {
        const s = Object.assign({}, browserSt)
        if (value === undefined) delete s[key]; else s[key] = value
        browserSt = s
    }
    onKindChanged: { app.setUiValue("addSource/kind", kind); devices = []; device = ""; Qt.callLater(refreshDevices) }
    onOpened: { adding = false; mono = false; browserSt = ({}); Qt.callLater(refreshDevices); nameField.forceActiveFocus() }
    readonly property bool ready: kind === "window" ? device !== ""
        : kind === "browser" ? !!(browserSt.local ? browserSt.file : browserSt.url) : true
    readonly property bool onlyOnce: kind === "camera" || audioKind
    readonly property string inUseBy: onlyOnce && visible ? app.sourceUsingDevice(kind, device) : ""
    readonly property bool busyElsewhere: kind === "camera" && !inUseBy && !!(devices.find(d => d.value === device) || {}).inUse
    readonly property var previewSettings: kind === "browser" ? browserSt : device ? ({ device: device }) : ({})
    function create() {
        if (!ready)
            return
        if (inUseBy) {
            if (!audioKind && placeBox.checked) app.placeSource(inUseBy)
            nameField.text = ""; close()
            return
        }
        adding = true
        if (kind === "camera") {
            pendingCreate.restart()
            return
        }
        finishCreate()
    }
    Timer { id: pendingCreate; interval: 700; onTriggered: addDialog.finishCreate() }
    function finishCreate() {
        const name = nameField.text || (kind === "window" ? deviceName.slice(0, 48) : "")
        const extra = picksDevice ? device : kind === "browser" ? (browserSt.url || "") : extraField.text
        const more = kind === "browser" ? browserSt : audioKind ? ({ mono: mono }) : ({})
        const before = app.mixer.map(m => m.id)
        app.addSource(kind, name, extra, placeBox.checked, more)
        const added = kind === "mic" && tuneBox.checked ? app.mixer.find(m => m.kind === "mic" && before.indexOf(m.id) < 0) : null
        nameField.text = ""; extraField.text = ""; close()
        if (added)
            Qt.callLater(() => addDialog.tuneMicRequested(added.id))
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Caption { text: "Add each source once and use it in any scene."; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        GridLayout {
            columns: 5; columnSpacing: 6; rowSpacing: 6
            Repeater {
                model: [["camera", "video", "Camera"], ["display", "monitor", "Display"], ["window", "app-window", "Window"], ["game", "gamepad-2", "Game"],
                        ["browser", "globe", "Browser"], ["mic", "mic", "Mic"], ["desktop_audio", "volume-2", "Sound"]]
                       .filter(k => app.kindAvailable(k[0]))
                       .concat(addDialog.visible ? app.pluginSourceTypes().map(t => [t.kind, t.icon, t.label]) : [])
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool on: addDialog.kind === modelData[0]
                    width: 96; height: 62; radius: Pal.radius + 2
                    color: on ? Pal.s7 : kindMouse.containsMouse ? Pal.s6 : Pal.s4
                    border.width: 1
                    border.color: on ? Pal.borderStrong : Pal.border
                    Behavior on color { ColorAnimation { duration: Pal.fast } }
                    Column {
                        anchors.centerIn: parent; spacing: 4
                        FIcon { name: modelData[1]; size: 18; color: on ? Pal.fg : Pal.muted; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: modelData[2]; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: on ? 550 : 400; anchors.horizontalCenter: parent.horizontalCenter
                               width: Math.min(implicitWidth, 88); elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter }
                    }
                    MouseArea { Accessible.role: Accessible.Button; Accessible.name: String(modelData[2]); id: kindMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { addDialog.kind = modelData[0]; Qt.callLater(addDialog.refreshDevices) } }
                }
            }
        }
        FScrollView {
            id: addScroll
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(addMiddle.implicitHeight, Math.max(160, (Overlay.overlay ? Overlay.overlay.height : 900) - 420))
            ColumnLayout {
                id: addMiddle
                width: addScroll.availableWidth
                spacing: 12
                FSelect {
                    visible: addDialog.picksDevice && addDialog.kind !== "window"
                    Layout.fillWidth: true
                    placeholder: "No " + ({ camera: "camera", display: "display", mic: "microphone", desktop_audio: "sound output" })[addDialog.kind] + " found"
                    options: addDialog.devices
                    currentValue: addDialog.device
                    onPicked: (v) => { addDialog.device = v; addDialog.deviceName = (addDialog.devices.find(d => d.value === v) || {}).name || "" }
                    onDownChanged: if (down) addDialog.refreshDevices()
                }
                Rectangle {
                    visible: ["camera", "display", "browser"].indexOf(addDialog.kind) >= 0 || (addDialog.kind === "window" && addDialog.device !== "")
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(width * 9 / 16 * (addDialog.kind === "browser" ? 0.45 : 0.62))
                    radius: Pal.radius
                    color: Pal.s1
                    border.color: Pal.border
                    clip: true
                    SourcePreview {
                        id: addPreview
                        anchors.fill: parent; anchors.margins: 1
                        kind: addDialog.kind
                        settings: addDialog.previewSettings
                        sourceId: addDialog.inUseBy
                        live: addDialog.visible && !addDialog.adding && parent.visible && !addDialog.busyElsewhere
                    }
                    Caption {
                        visible: addDialog.busyElsewhere
                        anchors.centerIn: parent
                        text: "Another app is using this camera. Close that app first."
                    }
                    Caption {
                        visible: addDialog.inUseBy !== "" && addPreview.ready
                        anchors { left: parent.left; bottom: parent.bottom; margins: 8 }
                        text: "Already in your show. A camera opens in one place at a time, so this is that feed."
                        color: Pal.fg
                        Rectangle { anchors.fill: parent; anchors.margins: -4; z: -1; radius: 4; color: "#b0000000" }
                    }
                    Caption {
                        visible: !addPreview.ready && !addDialog.busyElsewhere
                        anchors.centerIn: parent
                        text: addDialog.kind === "browser" ? "Enter a URL or choose a file" : addDialog.device ? "Starting the preview…" : "Nothing to preview"
                    }
                    FButton {
                        visible: addDialog.kind === "browser" && addPreview.ready
                        anchors { top: parent.top; right: parent.right; margins: 6 }
                        iconName: "refresh-cw"; variant: "secondary"; tip: "Reload the page"
                        onClicked: addPreview.reload()
                    }
                }
                RowLayout {
                    visible: addDialog.audioKind
                    Layout.fillWidth: true
                    spacing: 10
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        Repeater {
                            model: [{ side: "L", db: audioMeter.levelL }, { side: "R", db: audioMeter.levelR }]
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 6
                                Caption { text: modelData.side; Layout.preferredWidth: 10 }
                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 8; radius: 4
                                    color: Pal.s1
                                    border.color: Pal.border
                                    Rectangle {
                                        readonly property real db: addDialog.mono ? audioMeter.level : modelData.db
                                        readonly property real f: Math.max(0, Math.min(1, (db + 60) / 60))
                                        width: (parent.width - 2) * f; height: parent.height - 2; x: 1; y: 1; radius: 3
                                        color: db > -9 ? "#e5484d" : db > -20 ? "#f5a524" : "#30a46c"
                                    }
                                }
                            }
                        }
                        SourcePreview {
                            id: audioMeter
                            visible: false
                            kind: addDialog.audioKind ? addDialog.kind : ""
                            settings: addDialog.previewSettings
                            live: addDialog.visible && !addDialog.adding && addDialog.audioKind
                        }
                    }
                    Caption { text: audioMeter.level > -59 ? Math.round(audioMeter.level) + " dB" : addDialog.kind === "mic" ? "Say something" : "Play something"; Layout.preferredWidth: 84 }
                    FCheckbox { text: "Mono"; checked: addDialog.mono; onClicked: addDialog.mono = checked }
                }
                Caption {
                    readonly property bool lopsidedNow: audioMeter.level > -45 && Math.abs(audioMeter.levelL - audioMeter.levelR) > 15
                    id: oneSide
                    property bool lopsided: false
                    onLopsidedNowChanged: { lopsidedTimer.restart() }
                    Timer { id: lopsidedTimer; interval: oneSide.lopsidedNow ? 1000 : 2000; onTriggered: oneSide.lopsided = oneSide.lopsidedNow }
                    visible: addDialog.audioKind && addDialog.kind === "mic"
                    opacity: !addDialog.mono && lopsided ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Pal.fast } }
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: "Only one side has sound. Turn on Mono so viewers hear it in both ears."
                    color: "#fbbf24"
                }
                BrowserOptions {
                    visible: addDialog.kind === "browser"
                    st: addDialog.browserSt
                    onChanged: (key, value) => addDialog.setBrowser(key, value)
                }
                ColumnLayout {
                    visible: addDialog.kind === "window"
                    Layout.fillWidth: true
                    spacing: 6
                    RowLayout {
                        Layout.fillWidth: true
                        Caption { text: "Choose the window to capture"; Layout.fillWidth: true }
                        FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "Refresh window list"; onClicked: addDialog.refreshDevices() }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        radius: Pal.radius
                        color: Pal.s3
                        border.color: Pal.border
                        Text {
                            visible: addDialog.devices.length === 0
                            anchors.centerIn: parent
                            width: parent.width - 32
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: permissions.missing.some(m => m.id === "screen") ? "Allow Screen Recording to list your windows." : "No windows to capture."
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                        }
                        ListView {
                            id: windowList
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            model: addDialog.devices
                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool chosen: addDialog.device === modelData.value
                                width: windowList.width
                                height: 30
                                radius: Pal.radius - 2
                                color: chosen ? Pal.selected : winMouse.containsMouse ? Pal.hover : "transparent"
                                Text {
                                    anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 10 }
                                    text: modelData.name
                                    elide: Text.ElideRight
                                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: chosen ? 550 : 400
                                }
                                MouseArea {
                                    id: winMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    Accessible.role: Accessible.Button
                                    Accessible.name: String(modelData.name)
                                    onClicked: { addDialog.device = modelData.value; addDialog.deviceName = modelData.name }
                                    onDoubleClicked: { addDialog.device = modelData.value; addDialog.deviceName = modelData.name; addDialog.create() }
                                }
                            }
                        }
                    }
                }
            }
        }
        FInput { id: nameField; Layout.fillWidth: true; placeholderText: "Name (optional)"; onAccepted: addDialog.create() }
        FInput {
            id: extraField
            Layout.fillWidth: true
            visible: ["text", "color", "image", "media"].indexOf(addDialog.kind) >= 0
            placeholderText: addDialog.kind === "text" ? "Text" : addDialog.kind === "color" ? "#1d1b2e" : "File path"
            onAccepted: addDialog.create()
        }
        FCheckbox { id: placeBox; checked: true; text: "Also place on " + app.editSceneName; visible: ["mic", "desktop_audio"].indexOf(addDialog.kind) < 0 }
        FCheckbox { id: tuneBox; checked: true; text: "Tune it after adding (30 seconds)"; visible: addDialog.kind === "mic" }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Item { Layout.fillWidth: true }
            Caption { visible: addDialog.inUseBy !== "" && addDialog.audioKind; text: "Already in your show" }
            FButton { text: "Cancel"; variant: "ghost"; onClicked: addDialog.close() }
            FButton {
                text: !addDialog.inUseBy ? "Add source" : addDialog.audioKind ? "Add source" : "Place on " + app.editSceneName
                variant: "primary"
                enabled: addDialog.ready && !(addDialog.inUseBy && (addDialog.audioKind || !placeBox.checked))
                onClicked: addDialog.create()
            }
        }
    }
}
