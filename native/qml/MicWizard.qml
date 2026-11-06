import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Tune my mic. LOBS listens in short steps and sets up the mic's effects from what it
// heard. Each step starts by itself: quiet after a short countdown, talking when it hears your
// voice, loud when it hears a jump. The first reading and a second one (with the new effects)
// are kept as clips, so you can hear before and after. The mic's effects are off while it
// measures (the raw signal); the result is an ordinary effect chain, one undo step.
FDialog {
    id: wiz
    title: step === 6 ? "Your mic is set up" : "Tune your mic"
    width: 520
    // Only Cancel, Keep it or Undo close it: a stray click outside (or Esc) would throw away a run.
    closePolicy: Popup.NoAutoClose
    property string micId: ""
    property bool fromAdd: false // opened right after adding the mic: its close button reads "Skip"
    readonly property var mic: app.mixer.find(m => m.id === micId) || ({ name: "Mic" })
    // About 12 seconds read aloud: long enough to measure, the same both times.
    readonly property string line: "Hey chat, welcome in! Grab a snack and get comfy. Today we're trying something brand new, and I want to hear what you think. If you're new here, say hi in chat, and let's have some fun."

    // Steps: 0 start, 1 quiet, 2 talk, 3 loud, 4 game audio, 5 read it again, 6 results.
    property int step: 0
    // The strip: step number, name and icon. (4 was game audio, now gone; ducking is a mic option.)
    readonly property var strip: [{ n: 1, name: "Room noise", icon: "volume-x" }, { n: 2, name: "Talking", icon: "mic" }, { n: 3, name: "Get loud!", icon: "volume-2" },
        { n: 5, name: "Compare", icon: "headphones" }]
    // phase: "wait" (counting down, or waiting to hear you), "listen", "done"
    property string phase: "wait"
    property real elapsed: 0
    property real countdown: 0
    readonly property var lengths: [0, 4, 12, 4, 5, 12, 0]
    property var samples: []
    property var measured: ({})
    property string beforeClip: ""
    property string afterClip: ""
    property string playing: "" // "before" | "after" | ""
    property bool again: false // recording After again from the results
    property int before: -1 // the history step before tuning: Undo and Try again go back to it, tweaks and all

    PresetNameDialog { id: presetName } // Save as preset, on the results

    // From Setup, the mic was never chosen: the first screen picks it and checks both sides.
    property bool pickMic: false
    property var micDevices: []
    function openFor(id, pick) {
        micId = id
        pickMic = !!pick
        if (pickMic) micDevices = app.devices(id)
        measured = ({}); beforeClip = ""; afterClip = ""; playing = ""
        step = 0
        app.setFxBypassed(micId, true)
        open()
    }
    onClosed: { phase = "done"; app.stopClip(); app.playClip(""); app.setFxBypassed(micId, false) }

    function level(id) { const l = app.levels[id] || [-60, -60]; return Math.max(l[0], l[1]) }
    function pct(arr, p) { if (!arr.length) return -60; const s = arr.slice().sort((a, b) => a - b); return s[Math.min(s.length - 1, Math.floor(p * s.length))] }
    readonly property real floorDb: measured.floor ?? -60
    readonly property real speechDb: measured.speech ?? -20

    // Each step waits first: a countdown for quiet and game audio, your voice for talking, a jump
    // for loud. Then it listens for its length.
    function begin(s) {
        step = s; samples = []; elapsed = 0
        phase = "wait"
        countdown = s >= 1 && s <= 5 ? 3 : 0 // time to read the next step (and stop yelling)
        if (s === 6) finishAll()
    }
    function listen() {
        phase = "listen"; elapsed = 0; samples = []
        if (step === 2) app.startClip(micId) // the raw reading: "before"
        if (step === 5) app.startClip(micId) // the same line with the new effects: "after"
    }
    function finishStep() {
        const m = Object.assign({}, measured)
        if (step === 1) m.floor = pct(samples, 0.5)
        if (step === 2) { const voiced = samples.filter(v => v > floorDb + 10); m.speech = pct(voiced.length > 10 ? voiced : samples, 0.9); beforeClip = app.stopClip("mic-before") }
        if (step === 3) m.loud = pct(samples, 1)
        if (step === 5) afterClip = app.stopClip("mic-after")
        measured = m
        if (step === 3) return tune()
        if (step === 5) again = false
        begin(step + 1)
    }
    // After the measuring steps: apply the effects, turn them on, and read the line again.
    function tune() { before = app.historyIndex; app.setFxBypassed(micId, false); app.applyMicTuning(micId, measured); begin(5) }
    function revert() { if (before >= 0 && app.historyIndex > before) app.jumpHistory(before); before = -1 }
    function finishAll() { phase = "done" }

    Timer {
        interval: 50; repeat: true; running: wiz.visible && wiz.step >= 1 && wiz.step <= 5 && wiz.phase !== "done"
        onTriggered: {
            const lv = wiz.level(wiz.micId)
            if (wiz.phase === "wait") {
                if (wiz.countdown > 0) { wiz.countdown -= 0.05; if (wiz.countdown <= 0) wiz.listen(); return }
                // Talking: starts when your voice is clearly above the room. Loud: when it jumps.
                if ((wiz.step === 2 || wiz.step === 5) && lv > wiz.floorDb + 12) wiz.listen()
                else if (wiz.step === 3 && lv > wiz.speechDb + 4) wiz.listen()
                return
            }
            wiz.samples.push(lv)
            wiz.elapsed += 0.05
            if (wiz.elapsed >= wiz.lengths[wiz.step]) wiz.finishStep()
        }
    }

    // Plain words for what it heard, and what LOBS did about it.
    function roomText(f) { return f < -60 ? "Quiet" : f < -50 ? "A little noisy" : "Noisy" }
    function voiceText(s) { return s < -30 ? "Soft · raised" : s < -14 ? "Good" : "Loud · lowered" }

    // A step on the strip: its icon in a circle (a check once it's done) and its name.
    component StepDot: ColumnLayout {
        required property var modelData
        readonly property bool current: wiz.step === modelData.n || (modelData.n === 5 && wiz.step === 6)
        readonly property bool past: wiz.step > modelData.n && !(modelData.n === 5 && wiz.step === 6)
        readonly property color accent: Pal.link
        Layout.preferredWidth: dots.width / wiz.strip.length // exactly equal shares, so the line meets every circle
        spacing: 4
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 32; implicitHeight: 32; radius: 16
            color: Pal.s5 // opaque, over the joining line
            border.width: 1; border.color: parent.current ? Pal.link : Pal.border
            Rectangle { anchors.fill: parent; radius: 16; visible: parent.parent.current; color: Qt.rgba(parent.parent.accent.r, parent.parent.accent.g, parent.parent.accent.b, 0.22) }
            FIcon { anchors.centerIn: parent; name: parent.parent.past ? "check" : parent.parent.modelData.icon; size: 15; color: parent.parent.current ? Pal.fg : Pal.muted }
        }
        Text { Layout.alignment: Qt.AlignHCenter; text: parent.modelData.name; color: parent.current || wiz.step === 0 ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    }

    ColumnLayout {
        width: parent.width
        spacing: 16

        // The steps, while it runs (the results screen doesn't need them).
        Item {
            visible: wiz.step < 6
            Layout.fillWidth: true
            implicitHeight: dots.implicitHeight
            // The line joining the steps, from the first circle's center to the last.
            Rectangle { x: dots.width / wiz.strip.length / 2; width: dots.width * (1 - 1 / wiz.strip.length); y: 16; height: 1; color: Pal.border }
            RowLayout {
                id: dots
                width: parent.width
                spacing: 0
                Repeater { model: wiz.strip; delegate: StepDot {} }
            }
        }

        // Start: one line, then go.
        Text {
            visible: wiz.step === 0
            Layout.fillWidth: true; wrapMode: Text.WordWrap
            text: "Automatically set up your mic. Listens to background noise, your talking and shouting, and applies filters at recommended levels."
            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
        }

        // From Setup: which mic, and does it come through on both sides?
        ColumnLayout {
            visible: wiz.step === 0 && wiz.pickMic
            Layout.fillWidth: true
            spacing: 8
            Text { text: "Your mic"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
            FSelect {
                Layout.fillWidth: true
                placeholder: "Choose your mic…"
                options: wiz.micDevices
                currentValue: wiz.micId && wiz.mic ? (app.sourceSetting(wiz.micId, "device") || "") : "" // wiz.mic: re-read when the show changes
                onDownChanged: if (down) wiz.micDevices = app.devices(wiz.micId, true)
                onPicked: (v) => app.setSourceSetting(wiz.micId, "device", v)
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Repeater {
                        model: ["L", "R"]
                        delegate: RowLayout {
                            required property string modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 6
                            Text { text: modelData; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 10 }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 8; radius: 4
                                color: Pal.s1; border.color: Pal.border
                                Rectangle {
                                    readonly property real db: (app.levels[wiz.micId] || [-60, -60])[index]
                                    readonly property real f: Math.max(0, Math.min(1, (db + 60) / 60))
                                    width: (parent.width - 2) * f; height: parent.height - 2; x: 1; y: 1; radius: 3
                                    color: db > -9 ? "#e5484d" : db > -20 ? "#f5a524" : "#30a46c"
                                }
                            }
                        }
                    }
                }
                Text { text: wiz.level(wiz.micId) > -59 ? Math.round(wiz.level(wiz.micId)) + " dB" : "Say something"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 84 }
                FCheckbox { text: "Mono"; checked: !!wiz.mic.mono; onClicked: app.setAudio(wiz.micId, "mono", checked, true) }
            }
            Text {
                // One side only, for a second: suggest Mono (keeps its space, so nothing jumps).
                id: oneSide
                readonly property var lv: app.levels[wiz.micId] || [-60, -60]
                readonly property bool lopsidedNow: Math.max(lv[0], lv[1]) > -45 && Math.abs(lv[0] - lv[1]) > 15
                property bool lopsided: false
                onLopsidedNowChanged: sideTimer.restart()
                Timer { id: sideTimer; interval: oneSide.lopsidedNow ? 1000 : 2000; onTriggered: oneSide.lopsided = oneSide.lopsidedNow }
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                opacity: !wiz.mic.mono && lopsided ? 1 : 0
                text: "Only one side has sound. Turn on Mono so viewers hear it in both ears."
                color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.caption
            }
        }

        // A step in progress.
        ColumnLayout {
            visible: wiz.step >= 1 && wiz.step <= 5
            Layout.fillWidth: true
            spacing: 10
            // Between steps: a banner over the next step's title, so you stop and read before it starts.
            Rectangle {
                visible: wiz.phase === "wait" && wiz.countdown > 0
                Layout.fillWidth: true
                implicitHeight: readyText.implicitHeight + 16
                radius: Pal.radius
                color: Qt.rgba(0.96, 0.65, 0.14, 0.16)
                border.width: 1; border.color: Qt.rgba(0.96, 0.65, 0.14, 0.55)
                Text {
                    id: readyText
                    anchors.centerIn: parent
                    text: "Get ready… " + Math.ceil(wiz.countdown)
                    color: "#f5a524"; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550
                }
            }
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550
                text: ["", "Stay quiet", "Read this out loud", "Now get loud", "", "Read it once more"][wiz.step]
            }
            Text {
                visible: wiz.step === 1 || wiz.step === 3
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                text: wiz.step === 1 ? "LOBS is hearing your room." : wiz.step === 3 ? "Yell, laugh or cheer, like your best moment on stream." : ""
            }
            // The line to read, set apart from the instructions.
            Rectangle {
                visible: wiz.step === 2 || wiz.step === 5
                Layout.fillWidth: true
                implicitHeight: quote.implicitHeight + 28
                radius: Pal.radius
                color: Pal.s3
                Text {
                    id: quote
                    anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16; anchors.rightMargin: 14
                    wrapMode: Text.WordWrap; lineHeight: 1.15
                    text: wiz.line
                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title
                }
            }
            MeterFader {
                Layout.fillWidth: true
                implicitHeight: 18
                value: 1
                levels: app.levels[wiz.micId] || [-60, -60]
                enabled: false
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Rectangle {
                    visible: wiz.phase === "listen"
                    Layout.fillWidth: true; implicitHeight: 4; radius: 2; color: Pal.s1
                    Rectangle { width: parent.width * (wiz.phase === "listen" ? Math.min(1, wiz.elapsed / Math.max(1, wiz.lengths[wiz.step])) : 0); height: parent.height; radius: 2; color: Pal.link }
                }
                Text {
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: wiz.phase === "listen" ? "Listening… " + Math.max(0, Math.ceil(wiz.lengths[wiz.step] - wiz.elapsed)) + " s"
                        : wiz.countdown > 0 ? ""
                        : wiz.step === 3 ? "Waiting for something loud…" : "Start reading whenever you're ready"
                }
            }
        }

        // Results: hear it, then the effects themselves, ready to adjust.
        ColumnLayout {
            visible: wiz.step === 6
            Layout.fillWidth: true
            spacing: 12
            Text {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: "Room " + wiz.roomText(wiz.floorDb).toLowerCase() + " · Voice " + wiz.voiceText(wiz.speechDb).toLowerCase() + " · Loud moments limited" + ""
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            }
            // Before / after, the same reading both times. Change an effect below, then record After again.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: [{ id: "before", text: "Before" }, { id: "after", text: "After" }]
                    delegate: FButton {
                        required property var modelData
                        readonly property bool on: wiz.playing === modelData.id
                        Layout.fillWidth: true; Layout.preferredWidth: 1
                        iconName: on ? "square" : "play"; text: modelData.text
                        variant: "secondary"; selected: on
                        enabled: (modelData.id === "before" ? wiz.beforeClip : wiz.afterClip) !== ""
                        onClicked: {
                            app.playClip(on ? "" : (modelData.id === "before" ? wiz.beforeClip : wiz.afterClip))
                            wiz.playing = on ? "" : modelData.id
                            stopTimer.restart()
                        }
                    }
                }
                Timer { id: stopTimer; interval: (wiz.lengths[2] + 0.5) * 1000; onTriggered: wiz.playing = "" } // a clip's length
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
            // Edit filters: closed by default. The effects LOBS set up (the Inspector's rows), and
            // recording After again once they're changed.
            property bool editing: false
            MouseArea {
                Layout.fillWidth: true
                implicitHeight: editRow.implicitHeight + 4
                cursorShape: Qt.PointingHandCursor
                onClicked: parent.editing = !parent.editing
                RowLayout {
                    id: editRow
                    anchors.fill: parent
                    spacing: 6
                    FIcon { name: "sliders-horizontal"; size: 14; color: Pal.muted }
                    Text { text: "Edit filters"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true }
                    Text { visible: !editRow.parent.parent.editing; text: wiz.mic.fx ? wiz.mic.fx.length + " effects" : ""; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                    FIcon { name: editRow.parent.parent.editing ? "chevron-up" : "chevron-down"; size: 14; color: Pal.muted }
                }
            }
            FButton {
                visible: parent.editing
                Layout.fillWidth: true
                iconName: "record"; text: "Record After again"; variant: "secondary"
                tip: "Read the line again with the effects as they are now"
                onClicked: { app.playClip(""); wiz.playing = ""; wiz.again = true; wiz.begin(5) }
            }
            FScrollView {
                id: fxScroll
                visible: parent.editing
                Layout.fillWidth: true
                // Grows with the list; scrolls only in a short window (with its bar showing).
                Layout.preferredHeight: Math.min(fxList.implicitHeight, Math.max(200, (Overlay.overlay ? Overlay.overlay.height : 800) - 340))
                clip: true
                contentWidth: availableWidth
                FxChain { id: fxList; width: fxScroll.availableWidth; s: wiz.mic; review: true }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            FButton { visible: wiz.step === 6; text: "Try again"; variant: "ghost"; onClicked: { app.playClip(""); wiz.revert(); wiz.measured = ({}); wiz.beforeClip = ""; wiz.afterClip = ""; app.setFxBypassed(wiz.micId, true); wiz.begin(1) } }
            Item { Layout.fillWidth: true }
            FButton { visible: wiz.step === 6; text: "Cancel"; variant: "ghost"; tip: "Put the old effects back"; onClicked: { app.playClip(""); wiz.revert(); wiz.close() } }
            FButton { visible: wiz.step < 6; text: wiz.fromAdd ? "Skip" : "Cancel"; variant: "ghost"; onClicked: { if (wiz.step === 5 && !wiz.again) wiz.revert(); wiz.close() } } // re-recording After keeps the effects
            FButton { visible: wiz.step === 6; text: "Save as preset"; variant: "ghost"; tip: "Use this sound on another mic, or after a reinstall"; onClicked: presetName.ask(wiz.micId, wiz.mic.name) }
            FButton { visible: wiz.step === 6; text: "Keep it"; variant: "primary"; onClicked: wiz.close() }
            FButton { visible: wiz.step === 0; text: "Start"; variant: "primary"; onClicked: wiz.begin(1) }
        }
    }
}
