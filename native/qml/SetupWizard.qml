import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Setup assistant: first run, or Settings â†’ Run setup again. Walks through what you'll do,
// your platform and stream key, the closest server, a bandwidth test, video and bitrate,
// recording, and Twitch panels. Nothing is saved until Finish.
FDialog {
    id: wiz
    title: ""
    width: 720
    closePolicy: Popup.NoAutoClose
    padding: 20
    topPadding: 12
    signal addPanels(bool chat, bool info)

    // ---- choices ----
    property bool streaming: true
    property string service: "Twitch"
    property string customUrl: ""
    property string keyText: ""
    property string channel: ""
    property var pings: []
    property bool pinging: false
    property string server: "auto"
    property bool serverPicked: false // you chose one on the server step
    property var test: ({})          // live progress
    property var testResult: null    // done
    property bool testing: false
    property int manualMbps: 10
    property var rec: ({})
    property int outW: 1920
    property int outH: 1080
    property int fps: 60
    property int bitrate: 6000
    property string recFolder: ""
    property string recFormat: "mp4"
    property string recQuality: "high"
    property bool chatPanel: true
    property bool infoPanel: true

    readonly property bool twitch: streaming && service === "Twitch"
    readonly property bool hasKey: keyText.trim() !== "" || !!app.settings.hasKey
    // Steps shown for this path. On macOS, capture permissions come right after the welcome.
    readonly property bool asksAccess: permissions.all.length > 0
    readonly property var steps: {
        const list = streaming
            ? (twitch ? ["start", "account", "server", "test", "video", "record", "panels", "done"]
                      : ["start", "account", "test", "video", "record", "done"])
            : ["start", "video", "record", "done"]
        if (asksAccess)
            list.splice(1, 0, "access") // QML's JavaScript has no flatMap
        return list
    }
    property int index: 0
    readonly property string step: steps[Math.min(index, steps.length - 1)]
    readonly property var stepTitles: ({ start: "Welcome to LOBS", access: "Camera, mic and screen", account: "Your channel", server: "Closest server", test: "Test your connection",
                                         video: "Video and bitrate", record: "Recording", panels: "Twitch panels", done: "You're set" })
    readonly property int measured: testResult && testResult.ok ? testResult.kbps : manualMbps * 1000

    function recompute() {
        rec = app.recommendSettings(measured, service)
        if (streaming) { outW = rec.outWidth; outH = rec.outHeight; fps = rec.fps; bitrate = rec.bitrate }
    }
    function next() {
        if (step === "account" && keyText.trim()) app.saveSettings({ service: service, customUrl: customUrl, key: keyText.trim() })
        if (step === "test" || (step === "start" && !streaming)) recompute()
        if (index < steps.length - 1) index++
        if (step === "server" && pings.length === 0) runPings()
    }
    function back() { if (index > 0) index-- }
    function runPings() { pinging = true; pings = []; serverPicked = false; app.pingServers(service) }
    function runTest() {
        const target = pings.length && pings[0].ms >= 0 ? pings[0] : null
        const url = server !== "auto" ? server : (target ? target.value : "")
        if (!url) return
        testResult = null; test = {}; testing = app.startBandwidthTest(url, 8000)
    }
    function finish() {
        const s = { outWidth: outW, outHeight: outH, fps: fps, recordFolder: recFolder, recordFormat: recFormat, recordQuality: recQuality }
        if (streaming) {
            s.service = service; s.customUrl = customUrl; s.bitrate = bitrate
            s.server = server
            if (keyText.trim()) s.key = keyText.trim()
        }
        app.saveSettings(s)
        if (twitch && channel.trim()) app.setUiValue("twitch/channel", channel.trim().toLowerCase())
        app.setUiFlag("setup/done", true)
        if (twitch) addPanels(chatPanel, infoPanel)
        close()
    }
    onOpened: {
        index = 0; testResult = null; keyText = ""; serverPicked = false; pings = []
        const st = app.settings
        service = st.service || "Twitch"; customUrl = st.customUrl || ""; server = st.server || "auto"
        recFolder = st.recordFolder || ""; recFormat = st.recordFormat || "mp4"; recQuality = st.recordQuality || "high"
        outW = st.outWidth || 1920; outH = st.outHeight || 1080; fps = st.fps || 60; bitrate = st.bitrate || 6000
        channel = app.uiValue("twitch/channel", "")
    }
    onClosed: if (testing) { app.stopBandwidthTest(); testing = false }
    Connections {
        target: app
        function onServerPings(list) {
            wiz.pinging = false; wiz.pings = list
            if (!wiz.serverPicked && list.length && list[0].ms >= 0) wiz.server = list[0].value
        }
        function onBandwidthProgress(p) { wiz.test = p }
        function onBandwidthDone(r) { wiz.testing = false; wiz.testResult = r; if (r.ok) wiz.recompute() }
    }

    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    component Label: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 110 }
    // A big choice tile.
    component Choice: Rectangle {
        id: ch
        property string icon
        property string label
        property string detail
        property bool on: false
        signal picked()
        Layout.fillWidth: true
        implicitHeight: 74
        radius: Pal.radius + 2
        color: on ? Pal.s5 : chm.containsMouse ? Pal.s4 : Pal.s3
        border.width: on ? 2 : 1; border.color: on ? Pal.fg : Pal.border
        Behavior on color { ColorAnimation { duration: Pal.fast } }
        RowLayout {
            anchors { fill: parent; margins: 14 }
            spacing: 12
            FIcon { name: ch.icon; size: 20; color: Pal.fg }
            ColumnLayout {
                spacing: 2; Layout.fillWidth: true
                Text { text: ch.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body + 1; font.weight: 550 }
                Text { text: ch.detail; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            }
        }
        MouseArea { id: chm; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: ch.picked() }
    }

    header: ColumnLayout {
        spacing: 10
        // Progress: one bar per step.
        RowLayout {
            Layout.fillWidth: true; Layout.leftMargin: 20; Layout.rightMargin: 20; Layout.topMargin: 18
            spacing: 4
            Repeater {
                model: wiz.steps.length
                delegate: Rectangle {
                    required property int index
                    Layout.fillWidth: true; implicitHeight: 3; radius: 2
                    color: index <= wiz.index ? Pal.fg : Pal.s6
                    Behavior on color { ColorAnimation { duration: Pal.moderate } }
                }
            }
        }
        Text {
            Layout.leftMargin: 20
            text: wiz.stepTitles[wiz.step]
            color: Pal.fg; font.family: Pal.font; font.pixelSize: 20; font.weight: 600
        }
    }

    contentItem: Item {
        // As tall as the step showing (steps size to their content).
        implicitHeight: Math.max(220, ...Array.from(children).filter(c => c.visible).map(c => c.implicitHeight))
        // ---- start ----
        ColumnLayout {
            visible: wiz.step === "start"
            width: parent.width; spacing: 10
            Body { text: "A few questions and a quick test to set up streaming and recording. You can change it all later in Settings." }
            Item { implicitHeight: 4 }
            Choice { icon: "radio-tower"; label: "Stream and record"; detail: "Twitch, YouTube or another service."; on: wiz.streaming; onPicked: wiz.streaming = true }
            Choice { icon: "film"; label: "Just record"; detail: "No streaming."; on: !wiz.streaming; onPicked: wiz.streaming = false }
            ColumnLayout {
                visible: wiz.streaming
                Layout.fillWidth: true; spacing: 6; Layout.topMargin: 6
                Caption { text: "Where do you stream?" }
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: [{ v: "Twitch", n: "Twitch" }, { v: "YouTube - RTMPS", n: "YouTube" }, { v: "Custom", n: "Other (RTMP)" }]
                        delegate: FButton {
                            required property var modelData
                            text: modelData.n; variant: "tertiary"; selected: wiz.service === modelData.v
                            onClicked: { wiz.service = modelData.v; wiz.pings = []; wiz.server = "auto" }
                        }
                    }
                }
            }
        }
        // ---- access (macOS) ----
        ColumnLayout {
            visible: wiz.step === "access"
            width: parent.width; spacing: 10
            Body { text: "macOS asks before LOBS can use these. Allow the ones you'll use; you can change them any time in System Settings." }
            Item { implicitHeight: 2 }
            Repeater {
                model: permissions.all
                delegate: Rectangle {
                    id: acc
                    required property var modelData
                    readonly property bool granted: modelData.state === "granted"
                    Layout.fillWidth: true
                    implicitHeight: accRow.implicitHeight + 24
                    radius: Pal.radius + 2
                    color: Pal.s3
                    border.width: 1; border.color: Pal.border
                    RowLayout {
                        id: accRow
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 14 }
                        spacing: 12
                        FIcon { name: acc.modelData.id === "microphone" ? "mic" : acc.modelData.id === "camera" ? "video" : "monitor"; size: 20; color: Pal.fg }
                        ColumnLayout {
                            spacing: 2; Layout.fillWidth: true
                            Text { text: acc.modelData.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body + 1; font.weight: 550 }
                            Caption {
                                text: acc.modelData.purpose + (acc.modelData.state === "denied"
                                    ? " Off for LOBS: turn it on in System Settings" + (acc.modelData.relaunch ? ", then reopen LOBS." : ".") : "")
                            }
                        }
                        RowLayout {
                            visible: acc.granted
                            spacing: 4
                            FIcon { name: "check"; size: 14; color: Pal.muted }
                            Text { text: "Allowed"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
                        }
                        FButton {
                            visible: !acc.granted
                            text: acc.modelData.state === "notAsked" ? "Allow…" : "Open System Settings"
                            variant: acc.modelData.state === "notAsked" ? "primary" : "secondary"
                            onClicked: permissions.request(acc.modelData.id)
                        }
                        FButton {
                            visible: acc.modelData.state === "denied" && acc.modelData.relaunch
                            text: "Reopen LOBS"
                            variant: "secondary"
                            onClicked: permissions.relaunch()
                        }
                    }
                }
            }
        }
        // ---- account ----
        ColumnLayout {
            visible: wiz.step === "account"
            width: parent.width; spacing: 16
            ColumnLayout {
                visible: wiz.twitch
                Layout.fillWidth: true; spacing: 6
                Text { text: "Twitch username"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
                FInput {
                    id: chanIn
                    Layout.fillWidth: true
                    leftPadding: prefix.implicitWidth + 12
                    placeholderText: "yourname"
                    text: wiz.channel
                    // A pasted link works too: keep just the name.
                    onTextChanged: { const c = text.replace(/^.*twitch\.tv\//i, "").replace(/[\/?#].*$/, ""); if (c !== text) text = c; wiz.channel = c }
                    Text { id: prefix; x: 10; anchors.verticalCenter: parent.verticalCenter; text: "twitch.tv/"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
                }
                Caption { text: "As in twitch.tv/yourname." }
            }
            ColumnLayout {
                visible: wiz.service === "Custom"
                Layout.fillWidth: true; spacing: 6
                Text { text: "Server URL"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
                FInput { Layout.fillWidth: true; placeholderText: "rtmp://…"; text: wiz.customUrl; onTextChanged: wiz.customUrl = text }
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 6
                Text { text: "Stream key"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    FInput { Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: app.settings.hasKey ? "Saved. Paste a new one to replace it" : "Paste your stream key"; text: wiz.keyText; onTextChanged: wiz.keyText = text }
                    FButton {
                        visible: app.streamKeyUrl(wiz.service) !== ""
                        iconName: "external-link"; text: "Get my key"; variant: "secondary"
                        tip: "Opens your dashboard in your browser"
                        onClicked: Qt.openUrlExternally(app.streamKeyUrl(wiz.service))
                    }
                }
                Caption { text: "Find it in your " + (wiz.service === "Custom" ? "server settings" : wiz.service.replace(" - RTMPS", "") + " dashboard") + ". Stored encrypted." }
            }
        }
        // ---- server ----
        ColumnLayout {
            visible: wiz.step === "server"
            width: parent.width; spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Body { text: "Sorted by response time." }
                FButton { iconName: "refresh-cw"; text: wiz.pinging ? "Checking…" : "Check again"; variant: "secondary"; enabled: !wiz.pinging; onClicked: wiz.runPings() }
            }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: 340
                clip: true; spacing: 2
                model: wiz.pings
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: ListView.view.width; height: 34; radius: Pal.radius
                    color: wiz.server === modelData.value ? Pal.selected : sm.containsMouse ? Pal.hover : "transparent"
                    RowLayout {
                        anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                        Text { text: modelData.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; Layout.fillWidth: true; elide: Text.ElideRight }
                        FBadge { visible: index === 0 && modelData.ms >= 0; text: "Fastest"; tone: "neutral" }
                        Text { text: modelData.ms >= 0 ? modelData.ms + " ms" : "no answer"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                    }
                    MouseArea { id: sm; anchors.fill: parent; hoverEnabled: true; onClicked: { wiz.server = modelData.value; wiz.serverPicked = true } }
                }
                Text { visible: wiz.pinging; anchors.centerIn: parent; text: "Checking servers…"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
            }
        }
        // ---- test ----
        ColumnLayout {
            visible: wiz.step === "test"
            width: parent.width; spacing: 12
            Body {
                text: wiz.twitch
                      ? "A 10 second test stream. Nothing goes live."
                      : "LOBS can't test " + wiz.service.replace(" - RTMPS", "") + " without going live, so tell it your upload speed. A speed test site shows it as \"Upload\"."
            }
            // Twitch: live gauge.
            ColumnLayout {
                visible: wiz.twitch
                Layout.fillWidth: true; spacing: 8
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 120; radius: Pal.radius + 2
                    color: Pal.s3; border.width: 1; border.color: Pal.border
                    ColumnLayout {
                        anchors.centerIn: parent; spacing: 4
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: wiz.testResult ? (wiz.testResult.ok ? (wiz.testResult.kbps / 1000).toFixed(1) + " Mbps" : "Test failed")
                                 : wiz.testing ? ((wiz.test.kbps || 0) / 1000).toFixed(1) + " Mbps" : "—"
                            color: wiz.testResult && !wiz.testResult.ok ? Pal.danger : Pal.fg; font.family: Pal.font; font.pixelSize: 30; font.weight: 600
                        }
                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: wiz.testResult ? (wiz.testResult.ok ? "upload measured · " + wiz.testResult.dropped.toFixed(1) + "% frames dropped" : wiz.testResult.error)
                                 : wiz.testing ? (wiz.test.connected ? "measuring… " + Math.round(wiz.test.elapsed || 0) + " / 10 s" : "connecting…") : "Not tested yet"
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                        }
                    }
                    Rectangle {
                        visible: wiz.testing
                        anchors { left: parent.left; bottom: parent.bottom; margins: 1 }
                        height: 3; radius: 2; color: Pal.fg
                        width: (parent.width - 2) * Math.min(1, (wiz.test.elapsed || 0) / 10)
                        Behavior on width { NumberAnimation { duration: 450 } }
                    }
                }
                RowLayout {
                    FButton {
                        text: wiz.testing ? "Stop" : wiz.testResult ? "Test again" : "Start test"
                        variant: wiz.testing ? "secondary" : "primary"
                        enabled: wiz.testing || wiz.hasKey
                        onClicked: wiz.testing ? (app.stopBandwidthTest(), wiz.testing = false) : wiz.runTest()
                    }
                    Caption { visible: !wiz.hasKey; text: "Add your stream key first (previous step)." }
                }
            }
            // Others: manual upload speed.
            FFillSlider {
                visible: !wiz.twitch
                Layout.fillWidth: true
                label: "Upload speed"
                from: 1; to: 100; decimals: 0
                value: wiz.manualMbps
                format: (v) => Math.round(v) + " Mbps"
                onReleased: (v) => wiz.manualMbps = Math.round(v)
            }
            Caption { text: wiz.twitch ? "If you skip it, LOBS assumes " + wiz.manualMbps + " Mbps." : "" }
        }
        // ---- video ----
        ColumnLayout {
            visible: wiz.step === "video"
            width: parent.width; spacing: 12
            Body {
                text: wiz.streaming
                      ? "Recommended for " + (wiz.measured / 1000).toFixed(1) + " Mbps upload" + (wiz.rec.encoderName ? ", encoding with " + wiz.rec.encoderName : "") + "."
                      : "How recordings are sized and how smooth they are."
            }
            Caption {
                visible: wiz.streaming && wiz.rec.slow === true
                color: Pal.danger
                text: "Your upload is too slow for a stable stream. A wired connection helps, or record instead."
            }
            Caption {
                visible: wiz.streaming && wiz.rec.hardware === false
                text: "No hardware encoder found. 720p is lighter on your CPU."
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Resolution" }
                FSelect {
                    Layout.fillWidth: true
                    options: [{ name: "1920 × 1080", value: "1920x1080" }, { name: "1600 × 900", value: "1600x900" }, { name: "1280 × 720", value: "1280x720" }, { name: "852 × 480", value: "852x480" }]
                    currentValue: wiz.outW + "x" + wiz.outH
                    onPicked: (v) => { const p = v.split("x"); wiz.outW = +p[0]; wiz.outH = +p[1] }
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Frame rate" }
                FSelect {
                    Layout.fillWidth: true
                    options: [{ name: "60 fps (games, motion)", value: 60 }, { name: "30 fps (talking, slides)", value: 30 }]
                    currentValue: wiz.fps
                    onPicked: (v) => wiz.fps = v
                }
            }
            FFillSlider {
                visible: wiz.streaming
                Layout.fillWidth: true
                label: "Stream bitrate"
                from: 1500; to: wiz.service === "Twitch" ? 6000 : 12000; decimals: 0
                value: wiz.bitrate
                format: (v) => (v / 1000).toFixed(1) + " Mbps"
                onReleased: (v) => wiz.bitrate = Math.round(v / 100) * 100
            }
            Caption {
                visible: wiz.streaming
                text: wiz.bitrate > wiz.measured * 0.8 ? "Close to your measured upload." : ""
                color: wiz.bitrate > wiz.measured * 0.8 ? Pal.danger : Pal.muted
            }
        }
        // ---- record ----
        ColumnLayout {
            visible: wiz.step === "record"
            width: parent.width; spacing: 12
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Save to" }
                FInput { Layout.fillWidth: true; text: wiz.recFolder; onTextChanged: wiz.recFolder = text }
                FButton { text: "Browse…"; variant: "secondary"; onClicked: folderPick.open() }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Quality" }
                FSelect {
                    Layout.fillWidth: true
                    options: [{ name: "High (moderate files)", value: "high" }, { name: "Indistinguishable (very large files)", value: "indistinguishable" }, { name: "Same as stream (small files)", value: "stream" }]
                    currentValue: wiz.recQuality
                    onPicked: (v) => wiz.recQuality = v
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Format" }
                FSelect {
                    Layout.fillWidth: true
                    options: [{ name: "MP4 (plays everywhere)", value: "mp4" }, { name: "MKV (survives crashes)", value: "mkv" }, { name: "MOV", value: "mov" }]
                    currentValue: wiz.recFormat
                    onPicked: (v) => wiz.recFormat = v
                }
            }
            FolderDialog { id: folderPick; onAccepted: wiz.recFolder = decodeURIComponent(selectedFolder.toString().replace(/^file:\/{3}/, "")) }
        }
        // ---- panels ----
        ColumnLayout {
            visible: wiz.step === "panels"
            width: parent.width; spacing: 10
            Body { text: "Twitch panels next to your scenes." }
            Choice { icon: "message-square"; label: "Chat"; detail: "Read and reply to chat."; on: wiz.chatPanel; onPicked: wiz.chatPanel = !wiz.chatPanel }
            Choice { icon: "info"; label: "Stream info"; detail: "Title, category and tags."; on: wiz.infoPanel; onPicked: wiz.infoPanel = !wiz.infoPanel }
            Caption { visible: !wiz.channel.trim(); text: "Needs your channel name from the Your channel step." }
        }
        // ---- done ----
        ColumnLayout {
            visible: wiz.step === "done"
            width: parent.width; spacing: 8
            Body { text: "Finish to save." }
            GridLayout {
                columns: 2; columnSpacing: 16; rowSpacing: 6; Layout.topMargin: 6
                Label { text: "Platform"; visible: wiz.streaming }
                Body { visible: wiz.streaming; text: wiz.service.replace(" - RTMPS", "") + (wiz.hasKey ? " · key saved" : " · no stream key yet") }
                Label { text: "Server"; visible: wiz.twitch }
                Body { visible: wiz.twitch; text: (wiz.pings.find(p => p.value === wiz.server) || { name: "Automatic" }).name }
                Label { text: "Video" }
                Body { text: wiz.outW + " × " + wiz.outH + " at " + wiz.fps + " fps" }
                Label { text: "Bitrate"; visible: wiz.streaming }
                Body { visible: wiz.streaming; text: (wiz.bitrate / 1000).toFixed(1) + " Mbps" }
                Label { text: "Recording" }
                Body { text: wiz.recFormat.toUpperCase() + ", " + wiz.recQuality + " quality" }
                Label { text: "Panels"; visible: wiz.twitch }
                Body { visible: wiz.twitch; text: [wiz.chatPanel ? "Chat" : "", wiz.infoPanel ? "Stream info" : ""].filter(x => x).join(", ") || "None" }
            }
        }
    }

    footer: RowLayout {
        spacing: 6
        FButton { text: "Skip setup"; variant: "ghost"; visible: wiz.index === 0; onClicked: { app.setUiFlag("setup/done", true); wiz.close() }
            Layout.leftMargin: 20; Layout.bottomMargin: 18 }
        Item { Layout.fillWidth: true }
        FButton { text: "Back"; variant: "ghost"; visible: wiz.index > 0; enabled: !wiz.testing; onClicked: wiz.back(); Layout.bottomMargin: 18 }
        FButton {
            text: wiz.step === "done" ? "Finish" : wiz.step === "test" && wiz.twitch && !wiz.testResult ? "Skip test" : "Continue"
            variant: "primary"
            enabled: !wiz.testing && !(wiz.step === "account" && wiz.service === "Custom" && !wiz.customUrl.trim())
            onClicked: wiz.step === "done" ? wiz.finish() : wiz.next()
            Layout.rightMargin: 20; Layout.bottomMargin: 18
        }
    }
}
