import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Full-page settings: sidebar sections on the left, grouped cards on the right.
// Every change saves immediately (the stream key has an explicit save, since it's a secret).
Rectangle {
    id: page
    color: Pal.s1
    visible: opacity > 0
    opacity: open ? 1 : 0
    property bool open: false
    signal updateRequested() // the update dialog lives in the main window
    property int section: 0
    property var s: app.settings
    Behavior on opacity { NumberAnimation { duration: page.open ? Pal.slow : Pal.moderate; easing.type: Easing.OutCubic } }
    function show(i) { if (i !== undefined) section = i; open = true; forceActiveFocus() }
    function save(k, v) { const m = {}; m[k] = v; app.saveSettings(m) }
    Keys.onEscapePressed: open = false
    MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: (w) => w.accepted = false } // block clicks to the editor

    readonly property var sections: [
        { title: "Stream", icon: "radio-tower", desc: "Where LOBS sends your live video." },
        { title: "Recording", icon: "circle", desc: "Where recordings go and how they're encoded." },
        { title: "Video", icon: "monitor", desc: "Output size, frame rate and encoder." },
        { title: "Audio", icon: "audio-lines", desc: "Devices for your global mic and desktop audio." },
        { title: "Hotkeys", icon: "keyboard", desc: "Work even while a game has focus." },
        { title: "Updates", icon: "refresh-cw", desc: "New versions of LOBS, with the OBS engine they were tested with." },
        { title: "About", icon: "info", desc: "Versions, licenses and where LOBS keeps its files." }
    ]

    // ---- building blocks ----
    component Card: Rectangle {
        default property alias rows: col.data
        Layout.fillWidth: true
        implicitHeight: col.implicitHeight
        radius: 12
        color: Pal.s2
        border.width: 1
        border.color: Pal.border
        ColumnLayout { id: col; anchors.left: parent.left; anchors.right: parent.right; spacing: 0 }
    }
    component SettingRow: Item {
        id: sr
        property string title
        property string description
        property bool divider: true
        default property alias control: slot.data
        Layout.fillWidth: true
        implicitHeight: Math.max(56, txt.implicitHeight + 24)
        Rectangle { visible: sr.divider; anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right; anchors.leftMargin: 16; anchors.rightMargin: 16; height: 1; color: Pal.border }
        ColumnLayout {
            id: txt
            anchors.left: parent.left; anchors.leftMargin: 16
            anchors.right: slot.left; anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text { text: sr.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Text { visible: sr.description !== ""; text: sr.description; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        }
        Item {
            id: slot
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 240
            height: Pal.ctl
            // Fields fill the slot; segmented tabs keep their natural width, right-aligned.
            function fit() {
                for (let i = 0; i < children.length; i++) {
                    const c = children[i]
                    if (c.fillSlot === false) c.x = Qt.binding(() => slot.width - c.width)
                    else c.width = width
                    c.y = (height - c.height) / 2
                }
            }
            onWidthChanged: fit()
            onChildrenChanged: fit()
        }
    }
    component GroupTitle: Text {
        Layout.topMargin: 24
        Layout.bottomMargin: 8
        Layout.leftMargin: 4
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 600
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- sidebar ----
        Rectangle {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            color: Pal.s2
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    FButton { iconName: "x"; variant: "ghost"; onClicked: page.open = false; tip: "Close settings  Esc" }
                    Text { text: "Settings"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550; Layout.leftMargin: 4 }
                }
                FluidList {
                    id: nav
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: page.sections
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: Pal.ctl + 4
                        Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; anchors.topMargin: 2; anchors.bottomMargin: 2; radius: Pal.radius - 2; color: Pal.selected; visible: page.section === index }
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 14
                            spacing: 10
                            FIcon { name: modelData.icon; color: page.section === index ? Pal.fg : Pal.muted; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: modelData.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: page.section === index ? 550 : 400; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea { anchors.fill: parent; onClicked: page.section = index }
                    }
                }
            }
        }
        Rectangle { Layout.fillHeight: true; implicitWidth: 1; color: Pal.border }

        // ---- content ----
        FScrollView {
            id: sv
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            Item {
                width: sv.availableWidth
                implicitHeight: content.implicitHeight + 64
                ColumnLayout {
                    id: content
                    width: Math.min(680, parent.width - 64)
                    x: Math.max(32, (parent.width - width) / 2)
                    y: 36
                    spacing: 0
                    Text { text: page.sections[page.section].title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.display; font.weight: 700 }
                    Text { text: page.sections[page.section].desc; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body; Layout.topMargin: 6; Layout.bottomMargin: 8; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                    Rectangle {
                        visible: app.live && (page.section === 0 || page.section === 2)
                        Layout.fillWidth: true; Layout.topMargin: 16
                        implicitHeight: 40; radius: Pal.radius; color: "#2a1212"
                        Row { anchors.verticalCenter: parent.verticalCenter; x: 12; spacing: 8
                            FIcon { name: "triangle-alert"; color: "#fca5a5"; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "You're live. Stream and video changes apply next time you go live."; color: "#fecaca"; font.family: Pal.font; font.pixelSize: Pal.body; anchors.verticalCenter: parent.verticalCenter } }
                    }

                    // ===== Stream =====
                    ColumnLayout {
                        visible: page.section === 0
                        Layout.fillWidth: true
                        spacing: 0
                        Card {
                            SettingRow {
                                divider: false
                                title: "Setup assistant"
                                description: "Picks a server, bitrate and video settings."
                                FButton { property bool fillSlot: false; iconName: "wand-sparkles"; text: "Run setup"; variant: "secondary"; onClicked: { page.open = false; page.Window.window.openSetup() } }
                            }
                        }
                        GroupTitle { text: "Destination" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Service"
                                description: "Pick Custom for any RTMP or SRT server."
                                FSelect {
                                    options: app.services.map(n => ({ name: n, value: n })).concat([{ name: "Custom server", value: "Custom" }])
                                    currentValue: page.s.service
                                    onPicked: (v) => app.saveSettings({ service: v, server: "auto" })
                                }
                            }
                            SettingRow {
                                visible: page.s.service !== "Custom"
                                title: "Server"
                                description: "Auto picks the closest ingest."
                                FSelect {
                                    options: page.open && page.s.service !== "Custom" ? app.servers(page.s.service) : []
                                    currentValue: page.s.server
                                    placeholder: "Auto"
                                    onPicked: (v) => page.save("server", v)
                                }
                            }
                            SettingRow {
                                visible: page.s.service === "Custom"
                                title: "Server URL"
                                description: "rtmp://, rtmps:// or srt://"
                                FInput { text: page.s.customUrl || ""; placeholderText: "rtmp://live.example.com/app"; onEditingFinished: if (text !== page.s.customUrl) page.save("customUrl", text) }
                            }
                        }
                        GroupTitle { text: "Stream key" }
                        Card {
                            SettingRow {
                                divider: false
                                title: page.s.hasKey ? "Key saved" : "No key yet"
                                description: page.s.hasKey ? "Stored encrypted." : "Find it in your platform's dashboard."
                                Row {
                                    spacing: 6
                                    height: Pal.ctl
                                    FInput { id: keyIn; width: parent.width - (saveKey.visible ? saveKey.width + 6 : 0) - (removeKey.visible ? removeKey.width + 6 : 0); echoMode: TextInput.Password; placeholderText: page.s.hasKey ? "Replace key…" : "Paste stream key" }
                                    FButton { id: saveKey; text: "Save"; variant: "primary"; visible: keyIn.text.length > 0; onClicked: { app.saveSettings({ key: keyIn.text }); keyIn.text = "" } }
                                    FButton { id: removeKey; visible: page.s.hasKey && keyIn.text.length === 0; iconName: "trash-2"; variant: "ghost"; tip: "Remove saved key"; onClicked: app.saveSettings({ clearKey: true }) }
                                }
                            }
                        }
                        GroupTitle { text: "Quality" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Video bitrate"
                                description: "6000 kbps suits 1080p60 on Twitch. Services may cap it."
                                FNumField { label: ""; suffix: "kbps"; implicitWidth: 120; value: page.s.bitrate || 6000; onCommit: (v) => page.save("bitrate", Math.max(500, Math.round(v))) }
                            }
                            SettingRow {
                                title: "Audio bitrate"
                                FSelect {
                                    options: [96, 128, 160, 192, 256, 320].map(b => ({ name: b + " kbps", value: b }))
                                    currentValue: page.s.audioBitrate
                                    onPicked: (v) => page.save("audioBitrate", v)
                                }
                            }
                        }
                        GroupTitle { text: "While live" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Live lock"
                                description: "While streaming or recording, layers on air don't change by accident. Hold Alt, or click Unlock in the banner, to edit."
                                FSwitch { property bool fillSlot: false; checkable: false; checked: page.s.liveLock !== false; onClicked: page.save("liveLock", !(page.s.liveLock !== false)) }
                            }
                        }
                    }

                    // ===== Recording =====
                    ColumnLayout {
                        visible: page.section === 1
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "Files" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Folder"
                                description: "New recordings are named by date and time."
                                FInput { text: page.s.recordFolder || ""; onEditingFinished: if (text !== page.s.recordFolder) page.save("recordFolder", text) }
                            }
                            SettingRow {
                                title: "Format"
                                description: "MKV survives a crash; MP4 plays everywhere."
                                FTabs { model: ["MP4", "MKV", "MOV"]; currentIndex: ["mp4", "mkv", "mov"].indexOf(page.s.recordFormat); onActivated: (i) => page.save("recordFormat", ["mp4", "mkv", "mov"][i]) }
                            }
                        }
                        GroupTitle { text: "Quality" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Recording quality"
                                description: "Same as stream reuses the stream encoder."
                                FSelect {
                                    options: [{ name: "Same as stream", value: "stream" }, { name: "High, medium files", value: "high" }, { name: "Indistinguishable, large files", value: "indistinguishable" }]
                                    currentValue: page.s.recordQuality
                                    onPicked: (v) => page.save("recordQuality", v)
                                }
                            }
                        }
                        GroupTitle { text: "Replay buffer" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Keep the last"
                                description: "Turn it on from the top bar. Save replay writes to your recordings folder."
                                FTabs {
                                    property bool fillSlot: false
                                    model: ["15 s", "30 s", "1 min", "2 min"]
                                    currentIndex: [15, 30, 60, 120].indexOf(page.s.replaySeconds || 30)
                                    onActivated: (i) => page.save("replaySeconds", [15, 30, 60, 120][i])
                                }
                            }
                        }
                    }

                    // ===== Video =====
                    ColumnLayout {
                        visible: page.section === 2
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "Output" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Resolution"
                                description: "Your canvas stays 1920 × 1080 and is scaled to this."
                                FSelect {
                                    options: [[1920, 1080], [1600, 900], [1280, 720], [960, 540]].map(r => ({ name: r[0] + " × " + r[1], value: r[0] + "x" + r[1] }))
                                    currentValue: page.s.outWidth + "x" + page.s.outHeight
                                    onPicked: (v) => { const p = v.split("x"); app.saveSettings({ outWidth: Number(p[0]), outHeight: Number(p[1]) }) }
                                }
                            }
                            SettingRow {
                                title: "Frame rate"
                                FTabs { model: ["30 fps", "60 fps"]; currentIndex: page.s.fps === 30 ? 0 : 1; onActivated: (i) => page.save("fps", i === 0 ? 30 : 60) }
                            }
                        }
                        GroupTitle { text: "Encoder" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Video encoder"
                                description: "Hardware encoders use the GPU."
                                FSelect {
                                    readonly property string best: app.encoders.length ? app.encoders[0].name : "x264"
                                    options: [{ name: "Automatic · " + best, value: "auto" }].concat(app.encoders)
                                    currentValue: page.s.encoder
                                    onPicked: (v) => page.save("encoder", v)
                                }
                            }
                            SettingRow {
                                title: "Preset"
                                description: "Quality looks best; Speed uses the least GPU."
                                FTabs { model: ["Quality", "Balanced", "Speed"]; currentIndex: ["quality", "balanced", "speed"].indexOf(page.s.preset); onActivated: (i) => page.save("preset", ["quality", "balanced", "speed"][i]) }
                            }
                        }
                    }

                    // ===== Audio =====
                    ColumnLayout {
                        visible: page.section === 3
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "Global sources" }
                        Card {
                            Repeater {
                                model: app.mixer.filter(m => m.global)
                                delegate: SettingRow {
                                    required property var modelData
                                    required property int index
                                    divider: index > 0
                                    title: modelData.name
                                    description: "Always mixed in, in every scene."
                                    FSelect {
                                        options: page.open ? app.devices(modelData.id) : []
                                        currentValue: page.open ? (app.sourceSetting(modelData.id, "device") || "") : ""
                                        placeholder: "Default device"
                                        onPicked: (v) => app.setSourceSetting(modelData.id, "device", v)
                                    }
                                }
                            }
                        }
                        Text { Layout.topMargin: 10; text: "Levels, monitoring and tracks are in the Audio tab."; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                    }

                    // ===== Hotkeys =====
                    ColumnLayout {
                        visible: page.section === 4
                        Layout.fillWidth: true
                        spacing: 0
                        Repeater {
                            // One card per group (Output, Scenes, Audio).
                            model: [...new Set(app.hotkeyActions.map(a => a.group))]
                            delegate: ColumnLayout {
                                id: grp
                                required property string modelData
                                Layout.fillWidth: true
                                spacing: 0
                                GroupTitle { text: grp.modelData }
                                Card {
                                    Repeater {
                                        model: app.hotkeyActions.filter(a => a.group === grp.modelData)
                                        delegate: SettingRow {
                                            required property var modelData
                                            required property int index
                                            divider: index > 0
                                            title: modelData.label
                                            Rectangle {
                                                id: keyField
                                                property bool fillSlot: false
                                                readonly property string combo: (page.s.hotkeys || {})[modelData.id] || ""
                                                property bool capturing: false
                                                width: 160; height: Pal.ctl
                                                radius: Pal.radius
                                                color: capturing ? Pal.s5 : Pal.s1
                                                border.width: 1
                                                border.color: capturing ? Pal.fg : keyMouse.containsMouse ? Pal.borderStrong : Pal.border
                                                Text {
                                                    anchors.centerIn: parent
                                                    text: keyField.capturing ? "Press keys" : keyField.combo || "Not set"
                                                    color: keyField.combo || keyField.capturing ? Pal.fg : Pal.muted
                                                    font.family: Pal.font; font.pixelSize: Pal.body; font.weight: keyField.combo ? 550 : 400
                                                }
                                                MouseArea { id: keyMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: { keyField.capturing = true; keyField.forceActiveFocus() } }
                                                ToolTip.visible: keyMouse.containsMouse && !capturing
                                                ToolTip.delay: 600
                                                ToolTip.text: "Click, then press keys. Backspace clears, Esc cancels."
                                                onActiveFocusChanged: if (!activeFocus) capturing = false
                                                Keys.onPressed: (e) => {
                                                    if (!capturing) return
                                                    e.accepted = true
                                                    if (e.key === Qt.Key_Escape) { capturing = false; return }
                                                    if ((e.key === Qt.Key_Backspace || e.key === Qt.Key_Delete) && e.modifiers === Qt.NoModifier) {
                                                        app.setHotkey(modelData.id, ""); capturing = false; return
                                                    }
                                                    const name = app.keyName(e.key, e.modifiers)
                                                    if (name) { app.setHotkey(modelData.id, name); capturing = false }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ===== Updates =====
                    ColumnLayout {
                        visible: page.section === 5
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "LOBS" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "LOBS " + appUpdater.current
                                description: appUpdater.state === "checking" ? "Checking for updates..."
                                    : appUpdater.state === "available" ? "LOBS " + appUpdater.latest + " is available"
                                    : appUpdater.state === "downloading" ? "Downloading LOBS " + appUpdater.latest + "... " + Math.round(appUpdater.progress * 100) + "%"
                                    : appUpdater.state === "ready" ? "LOBS " + appUpdater.latest + " is ready to install"
                                    : appUpdater.state === "installing" ? "Installing..."
                                    : appUpdater.state === "upToDate" ? "Up to date" + (appUpdater.lastChecked ? " · checked " + appUpdater.lastChecked : "")
                                    : appUpdater.lastChecked ? "Last checked " + appUpdater.lastChecked : "Not checked yet"
                                FButton {
                                    property bool fillSlot: false
                                    text: appUpdater.state === "available" || appUpdater.state === "downloading" || appUpdater.state === "ready" ? "Update..." : "Check now"
                                    variant: appUpdater.state === "available" || appUpdater.state === "ready" ? "primary" : "secondary"
                                    enabled: ["checking", "installing"].indexOf(appUpdater.state) < 0
                                    onClicked: appUpdater.state === "available" || appUpdater.state === "downloading" || appUpdater.state === "ready" ? page.updateRequested() : appUpdater.check()
                                }
                            }
                            Text {
                                visible: appUpdater.message.length > 0
                                Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.bottomMargin: 12
                                wrapMode: Text.WordWrap
                                color: appUpdater.state === "error" ? "#fbbf24" : Pal.muted
                                font.family: Pal.font; font.pixelSize: Pal.body
                                text: appUpdater.message
                            }
                        }
                        GroupTitle { text: "OBS engine" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "OBS " + engineUpdater.installed
                                description: "Comes with LOBS and updates with it, so every release runs the engine it was tested with."
                                Item {}
                            }
                            Text {
                                visible: engineUpdater.message.length > 0
                                Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.bottomMargin: 12
                                wrapMode: Text.WordWrap
                                color: "#fbbf24"
                                font.family: Pal.font; font.pixelSize: Pal.body
                                text: engineUpdater.message
                            }
                            SettingRow {
                                visible: engineUpdater.previous.length > 0
                                title: "Previous engine"
                                description: "OBS " + engineUpdater.previous + " is kept as a fallback."
                                FButton { property bool fillSlot: false; text: engineUpdater.state === "ready" ? "Restart LOBS" : "Go back to " + engineUpdater.previous; variant: "secondary"; onClicked: engineUpdater.state === "ready" ? engineUpdater.restartLobs() : engineUpdater.rollback() }
                            }
                        }
                        Text {
                            Layout.fillWidth: true; Layout.topMargin: 12
                            wrapMode: Text.WordWrap
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                            text: "Checks LOBS's GitHub releases daily. Updates install only after the download matches its SHA-256 and is signed by LOBS's publisher. If LOBS fails to start twice on a new engine, it goes back to the previous one."
                        }
                    }

                    // ===== About =====
                    ColumnLayout {
                        visible: page.section === 6
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "LOBS" }
                        Card {
                            SettingRow { divider: false; title: "Version"; description: appUpdater.current + " · Qt 6.11, C++20"; Item {} }
                            SettingRow { title: "Engine"; description: "OBS " + engineUpdater.installed + " (official release build)"; Item {} }
                            SettingRow { title: "Encoder in use"; description: (app.encoders.find(e => e.value === page.s.resolvedEncoder) || { name: page.s.resolvedEncoder }).name; Item {} }
                        }
                        GroupTitle { text: "Files" }
                        Card {
                            SettingRow { divider: false; title: "Shows and settings"; description: app.dataFolder; Item {} }
                            SettingRow { title: "Recordings"; description: page.s.recordFolder || ""; Item {} }
                        }
                        GroupTitle { text: "Open source" }
                        Card {
                            SettingRow { divider: false; title: "OBS Studio (libobs)"; description: "GPL-2.0. LOBS is GPL-2.0-or-later."; Item {} }
                            SettingRow { title: "Inter typeface"; description: "SIL Open Font License 1.1, by Rasmus Andersson"; Item {} }
                            SettingRow { title: "Lucide icons"; description: "ISC License"; Item {} }
                            SettingRow { title: "Fluid Functionalism"; description: "Design system the UI follows, ported to QML"; Item {} }
                        }
                    }
                }
            }
        }
    }
}
