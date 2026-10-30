import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Full-page settings: sidebar sections on the left, grouped cards on the right.
// Every change saves immediately (the stream key has an explicit save, since it's a secret).
Rectangle {
    id: page
    color: Pal.s1
    visible: opacity > 0
    opacity: open ? 1 : 0
    property bool open: false
    signal updateRequested() // the update dialog lives in the main window
    signal twitchConnectRequested() // so does the Twitch sign-in window
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
        { title: "Updates", icon: "refresh-cw", desc: "New versions of LOBS and its OBS engine." },
        { title: "Plugins", icon: "puzzle", desc: "OBS plugins that add sources and filters." },
        { title: "Remote control", icon: "radio", desc: "Let Stream Deck and other OBS apps control LOBS." },
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
                    Layout.topMargin: chrome.leadingInset > 0 ? 30 : 0 // below macOS's window buttons (hidden in full screen)
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
                                description: "Auto picks the closest server."
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
                            // Twitch: connect the account and the key fills itself in, as in OBS.
                            SettingRow {
                                visible: String(page.s.service).startsWith("Twitch")
                                divider: false
                                title: twitch.connected ? "Connected as " + twitch.displayName : "Twitch account"
                                description: twitch.connected ? "Stream key from Twitch." : "Fills in your stream key."
                                Row {
                                    property bool fillSlot: false // right-aligned, like the other rows' controls
                                    spacing: 6
                                    height: Pal.ctl
                                    FButton { visible: !twitch.connected; text: "Connect with Twitch"; variant: "primary"; onClicked: page.twitchConnectRequested() }
                                    FButton { visible: twitch.connected; iconName: "refresh-cw"; variant: "ghost"; tip: "Get the stream key again"; enabled: !twitch.busy; onClicked: twitch.refreshStreamKey() }
                                    FButton { visible: twitch.connected; text: "Disconnect"; variant: "secondary"; onClicked: twitch.disconnectAccount() }
                                }
                            }
                            SettingRow {
                                id: keyRow
                                // A saved key shows as saved; Edit opens the box (to replace or remove it).
                                // Twitch's account provides it: no second row saying so.
                                property bool editing: false
                                readonly property bool fromTwitch: twitch.connected && String(page.s.service).startsWith("Twitch")
                                visible: !fromTwitch
                                divider: String(page.s.service).startsWith("Twitch")
                                readonly property bool showInput: !page.s.hasKey || editing
                                title: page.s.hasKey ? "Key saved" : "No key yet"
                                description: page.s.hasKey ? "Stored encrypted." : "Find it in your platform's dashboard."
                                Row {
                                    spacing: 6
                                    height: Pal.ctl
                                    FIcon { visible: !keyRow.showInput; name: "check"; size: 16; color: "#22c55e"; anchors.verticalCenter: parent.verticalCenter }
                                    FButton { visible: !keyRow.showInput; iconName: "pencil"; text: "Edit"; variant: "secondary"; onClicked: { keyRow.editing = true; keyIn.forceActiveFocus() } }
                                    FInput {
                                        id: keyIn
                                        visible: keyRow.showInput
                                        width: 220
                                        echoMode: TextInput.Password
                                        placeholderText: page.s.hasKey ? "New stream key" : "Paste stream key"
                                        onAccepted: if (text.length) saveKey.clicked()
                                        Keys.onEscapePressed: cancelKey.clicked()
                                    }
                                    FButton { id: saveKey; visible: keyRow.showInput; text: "Save"; variant: "primary"; enabled: keyIn.text.length > 0; onClicked: { app.saveSettings({ key: keyIn.text }); keyIn.text = ""; keyRow.editing = false } }
                                    FButton { id: cancelKey; visible: keyRow.editing; text: "Cancel"; variant: "ghost"; onClicked: { keyIn.text = ""; keyRow.editing = false } }
                                    FButton { visible: keyRow.editing && page.s.hasKey; iconName: "trash-2"; variant: "ghost"; tip: "Remove saved key"; onClicked: { app.saveSettings({ clearKey: true }); keyIn.text = ""; keyRow.editing = false } }
                                }
                            }
                        }
                        // A rehearsal on Twitch: the real encoder, scene and connection, but the channel
                        // never goes live. Twitch Inspector shows how it arrives.
                        GroupTitle { visible: String(page.s.service).startsWith("Twitch"); text: "Twitch" }
                        Card {
                            visible: String(page.s.service).startsWith("Twitch")
                            // Twitch's Enhanced Broadcasting, as in OBS: Twitch picks the qualities and the GPU
                            // encodes them all, so viewers and VODs always get a choice.
                            SettingRow {
                                divider: false
                                title: "Enhanced Broadcasting"
                                description: page.s.enhanced !== false ? "Sends several qualities so viewers can pick one. Uses more GPU and upload, and shares your PC's specs with Twitch."
                                                                      : "Sends one quality at the bitrate below."
                                FSwitch { property bool fillSlot: false; checkable: false; checked: page.s.enhanced !== false; onClicked: page.save("enhanced", !(page.s.enhanced !== false)) }
                            }
                            SettingRow {
                                title: app.testStreaming ? "Test stream running" : "Test without going live"
                                description: "Viewers can't see it. Check it in Twitch Inspector."
                                Row {
                                    spacing: 6
                                    height: Pal.ctl
                                    FButton {
                                        visible: !app.streaming || app.testStreaming
                                        text: app.testStreaming ? "Stop" : "Start test stream"
                                        variant: "secondary"
                                        enabled: page.s.hasKey
                                        onClicked: app.testStreaming ? app.toggleStreaming() : app.startTestStream()
                                    }
                                    FButton { iconName: "external-link"; text: "Inspector"; variant: "ghost"; onClicked: Qt.openUrlExternally("https://inspector.twitch.tv/") }
                                }
                            }
                        }
                        GroupTitle { text: "Quality" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Video bitrate"
                                description: String(page.s.service).startsWith("Twitch") && page.s.enhanced !== false ? "Twitch sets it with Enhanced Broadcasting." : "6000 kbps suits 1080p60 and is Twitch's limit."
                                FNumField { enabled: !(String(page.s.service).startsWith("Twitch") && page.s.enhanced !== false); label: ""; suffix: "kbps"; implicitWidth: 120; value: page.s.bitrate || 6000; onCommit: (v) => page.save("bitrate", Math.max(500, Math.round(v))) }
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
                                description: chrome.keys("Locks on-air layers while you stream or record. Hold Alt or click Unlock to edit.")
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
                                    options: [{ name: "Same as stream", value: "stream" }, { name: "High (medium files)", value: "high" }, { name: "Indistinguishable (large files)", value: "indistinguishable" }]
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
                                description: "Turn it on in the top bar. Saved replays go to your recordings folder."
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
                                    description: "Plays in every scene."
                                    FSelect {
                                        options: page.open ? app.devices(modelData.id) : []
                                        currentValue: page.open ? (app.sourceSetting(modelData.id, "device") || "") : ""
                                        placeholder: "Default device"
                                        onPicked: (v) => app.setSourceSetting(modelData.id, "device", v)
                                    }
                                }
                            }
                        }
                        Text { Layout.topMargin: 10; text: "Levels, monitoring and tracks are in the Audio mixer."; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                    }

                    // ===== Hotkeys =====
                    ColumnLayout {
                        visible: page.section === 4
                        Layout.fillWidth: true
                        spacing: 0
                        // macOS hands no keys to any app while Secure Keyboard Entry is on.
                        Rectangle {
                            visible: permissions.hotkeysBlocked
                            Layout.fillWidth: true
                            Layout.topMargin: 12
                            implicitHeight: blockedText.implicitHeight + 24
                            radius: Pal.radius
                            color: Pal.s3
                            border.color: "#4dfbbf24"
                            RowLayout {
                                anchors { fill: parent; margins: 12 }
                                spacing: 10
                                FIcon { name: "triangle-alert"; size: 15; color: "#fbbf24"; Layout.alignment: Qt.AlignTop }
                                Text {
                                    id: blockedText
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
                                    text: "<b>Global hotkeys are blocked.</b> Secure Keyboard Entry is on. Close password fields and password managers, or turn it off in Terminal's menu. Logging out clears it."
                                    textFormat: Text.StyledText
                                }
                            }
                        }
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
                                description: appUpdater.state === "checking" ? "Checking for updates…"
                                    : appUpdater.state === "available" ? "LOBS " + appUpdater.latest + " is available"
                                    : appUpdater.state === "downloading" ? "Downloading LOBS " + appUpdater.latest + "… " + Math.round(appUpdater.progress * 100) + "%"
                                    : appUpdater.state === "ready" ? "LOBS " + appUpdater.latest + " is ready to install"
                                    : appUpdater.state === "installing" ? "Installing…"
                                    : appUpdater.state === "upToDate" ? "Up to date" + (appUpdater.lastChecked ? " · checked " + appUpdater.lastChecked : "")
                                    : appUpdater.lastChecked ? "Last checked " + appUpdater.lastChecked : "Not checked yet"
                                FButton {
                                    property bool fillSlot: false
                                    text: appUpdater.state === "available" || appUpdater.state === "downloading" || appUpdater.state === "ready" ? "Update…" : "Check now"
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
                                description: "Ships with LOBS. Each release uses the engine it was tested with."
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
                            text: "Checks GitHub daily. Updates install only if the checksum and signature match. If a new engine fails to start twice, LOBS goes back to the previous one."
                        }
                    }

                    // ===== Plugins =====
                    ColumnLayout {
                        id: plug
                        visible: page.section === 6
                        Layout.fillWidth: true
                        spacing: 0
                        property var list: []
                        property bool restart: false
                        property string error: ""
                        function refresh() { list = app.plugins(); restart = app.pluginsNeedRestart() }
                        function add(url) { error = app.addPlugin(url.toString()); refresh() }
                        onVisibleChanged: if (visible) { error = ""; refresh() }
                        Connections { target: app; function onPluginsChanged() { plug.refresh() } }
                        FileDialog { id: pluginZip; title: "Add a plugin from its .zip"; nameFilters: ["Plugin archives (*.zip)", "All files (*)"]; onAccepted: plug.add(selectedFile) }
                        FolderDialog { id: pluginDir; title: "Add a plugin from its folder"; onAccepted: plug.add(selectedFolder) }
                        Rectangle {
                            visible: plug.restart
                            Layout.fillWidth: true; Layout.topMargin: 8
                            implicitHeight: bannerRow.implicitHeight + 20
                            radius: 12
                            color: Pal.s3
                            border.width: 1; border.color: Pal.borderStrong
                            RowLayout {
                                id: bannerRow
                                anchors.fill: parent; anchors.margins: 10; anchors.leftMargin: 16
                                FIcon { name: "refresh-cw"; color: Pal.fg }
                                Text { text: "Restart LOBS to apply plugin changes."; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                                FButton { text: "Restart LOBS"; variant: "primary"; visible: Qt.platform.os === "windows"; onClicked: engineUpdater.restartLobs() }
                            }
                        }
                        GroupTitle { text: "Installed" }
                        Card {
                            SettingRow {
                                visible: plug.list.length === 0
                                divider: false
                                title: "No plugins"
                                description: "Add an OBS plugin for 64-bit Windows from its .zip or folder."
                                Item {}
                            }
                            Repeater {
                                model: plug.list
                                delegate: SettingRow {
                                    required property var modelData
                                    required property int index
                                    readonly property var pl: modelData
                                    divider: index > 0
                                    title: pl.title + (pl.version ? "  " + pl.version : "")
                                    description: {
                                        const parts = []
                                        if (pl.status === "loaded") {
                                            const adds = []
                                            if (pl.sources.length) adds.push((pl.sources.length === 1 ? "source: " : "sources: ") + pl.sources.join(", "))
                                            if (pl.filters.length) adds.push((pl.filters.length === 1 ? "filter: " : "filters: ") + pl.filters.join(", "))
                                            parts.push("Loaded" + (adds.length ? ". Adds " + adds.join("; ") : ". Adds nothing LOBS can show") + ".")
                                            if (pl.transitions.length) parts.push("Its transitions (" + pl.transitions.join(", ") + ") aren't in LOBS yet.")
                                        } else if (pl.status === "failed") {
                                            parts.push("Didn't load. " + pl.reason)
                                        } else {
                                            parts.push(pl.reason)
                                        }
                                        parts.push(pl.folder)
                                        return parts.join("\n")
                                    }
                                    Row {
                                        property bool fillSlot: false
                                        spacing: 6
                                        FButton {
                                            visible: pl.status !== "removing"
                                            text: pl.status === "disabled" || pl.status === "crashed" ? "Turn on" : "Turn off"
                                            variant: "ghost"
                                            onClicked: app.setPluginEnabled(pl.name, pl.status === "disabled" || pl.status === "crashed")
                                        }
                                        FButton {
                                            visible: pl.status !== "removing"
                                            iconName: "trash-2"; variant: "ghost"; tip: "Remove " + pl.title
                                            onClicked: plug.error = app.removePlugin(pl.name)
                                        }
                                    }
                                }
                            }
                        }
                        Text {
                            visible: plug.error.length > 0
                            Layout.fillWidth: true; Layout.topMargin: 8
                            wrapMode: Text.WordWrap
                            color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.body
                            text: plug.error
                        }
                        GroupTitle { text: "Add" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Add a plugin"
                                description: "From the plugin's .zip or its unpacked folder."
                                Row {
                                    property bool fillSlot: false
                                    spacing: 6
                                    FButton { text: ".zip…"; variant: "secondary"; onClicked: pluginZip.open() }
                                    FButton { text: "Folder…"; variant: "secondary"; onClicked: pluginDir.open() }
                                }
                            }
                            SettingRow {
                                title: "Plugins folder"
                                description: app.pluginsFolder()
                                FButton { property bool fillSlot: false; text: "Open"; variant: "secondary"; onClicked: app.openPluginsFolder() }
                            }
                        }
                        Text {
                            Layout.fillWidth: true; Layout.topMargin: 12
                            wrapMode: Text.WordWrap
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                            text: "Plugins built for OBS 32 that add sources or filters work. Ones that add docks, menus or their own windows to OBS don't load. Their sources show under Plugins in Add Source, and their filters at the end of each Add filter list. A plugin runs inside LOBS, so only add plugins you trust."
                        }
                    }

                    // ===== Remote control: obs-websocket =====
                    // Apps that control OBS connect to LOBS the same way: same protocol, same port.
                    ColumnLayout {
                        visible: page.section === 7
                        Layout.fillWidth: true
                        spacing: 0
                        GroupTitle { text: "Server" }
                        Card {
                            SettingRow {
                                divider: false
                                title: "Allow remote control"
                                description: !websocket.enabled ? "For Stream Deck, Touch Portal, VNyan and other OBS apps."
                                           : websocket.error ? websocket.error
                                           : websocket.clients ? websocket.clients + (websocket.clients === 1 ? " app connected" : " apps connected")
                                           : "Waiting for an app"
                                FSwitch { property bool fillSlot: false; checkable: false; checked: websocket.enabled; onClicked: websocket.setEnabled(!websocket.enabled) }
                            }
                            SettingRow {
                                visible: websocket.enabled
                                title: "Address"
                                description: "Enter it in the app. OBS apps use port 4455."
                                Row {
                                    spacing: 6
                                    height: Pal.ctl
                                    Text { text: websocket.address; color: Pal.fg; font.family: Pal.mono; font.pixelSize: Pal.body; anchors.verticalCenter: parent.verticalCenter }
                                    FButton { iconName: "copy"; variant: "ghost"; tip: "Copy"; onClicked: { app.copyText(websocket.address); app.notify("Address copied") } }
                                }
                            }
                            SettingRow {
                                visible: websocket.enabled
                                title: "Port"
                                FNumField { property bool fillSlot: false; label: ""; implicitWidth: 120; from: 1024; to: 65535; value: websocket.port; onCommit: (v) => websocket.setPort(Math.round(v)) }
                            }
                            SettingRow {
                                visible: websocket.enabled
                                title: "Require a password"
                                description: websocket.authRequired ? "" : "Any app on your network can control LOBS."
                                FSwitch { property bool fillSlot: false; checkable: false; checked: websocket.authRequired; onClicked: websocket.setAuthRequired(!websocket.authRequired) }
                            }
                            SettingRow {
                                visible: websocket.enabled && websocket.authRequired
                                title: "Password"
                                description: "Enter it in the app."
                                Row {
                                    spacing: 6
                                    height: Pal.ctl
                                    FInput { id: wsPass; width: 180; readOnly: true; text: websocket.password; echoMode: wsShow.revealed ? TextInput.Normal : TextInput.Password }
                                    FButton { id: wsShow; property bool revealed: false; iconName: revealed ? "eye-off" : "eye"; variant: "ghost"; tip: revealed ? "Hide" : "Show"; onClicked: revealed = !revealed }
                                    FButton { iconName: "copy"; variant: "ghost"; tip: "Copy"; onClicked: { app.copyText(websocket.password); app.notify("Password copied") } }
                                    FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "New password"; onClicked: websocket.newPassword() }
                                }
                            }
                        }
                        Text {
                            Layout.fillWidth: true; Layout.topMargin: 10; Layout.leftMargin: 4
                            wrapMode: Text.WordWrap
                            text: "Not yet: filters, transitions and source settings."
                            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                        }
                    }

                    // ===== About =====
                    ColumnLayout {
                        visible: page.section === 8
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
