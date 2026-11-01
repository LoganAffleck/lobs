import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "AudioMath.js" as AudioMath

// An audio source's settings: volume, where it goes (stream, recording), monitoring, then
// balance, sync, mono and extra tracks under Advanced, warnings and its effect chain. In the Inspector when an audio source is selected (in Sources or the mixer).
ColumnLayout {
    id: au
    required property var s // an app.mixer entry
    spacing: 10
    readonly property bool monitorOnly: au.s.monitor === "monitor_only"
    readonly property bool onStream: !!(au.s.tracks && au.s.tracks[0])
    readonly property bool inRecording: au.s.inRecording !== false
    // A tile that's on or off: see-through blue when on, plain and muted when off.
    component OutTile: Rectangle {
        id: tile
        property string iconName
        property string text
        property bool on
        signal toggled()
        readonly property bool hoverOn: tileMouse.containsMouse
        readonly property color accent: Pal.link // a color, so its channels can be read (Pal.link is a string)
        Layout.fillWidth: true
        implicitHeight: Pal.ctl + 4
        radius: Pal.radius
        enabled: !au.monitorOnly
        opacity: enabled ? 1 : 0.5
        color: on ? Qt.rgba(accent.r, accent.g, accent.b, tileMouse.containsMouse ? 0.30 : 0.20) : tileMouse.containsMouse ? Pal.hover : "transparent"
        border.width: 1
        border.color: on ? accent : Pal.border
        Behavior on color { ColorAnimation { duration: Pal.fast } }
        Row {
            anchors.centerIn: parent
            spacing: 8
            FIcon { visible: tile.iconName !== ""; anchors.verticalCenter: parent.verticalCenter; name: tile.iconName; size: 14; color: tile.on ? Pal.fg : Pal.muted }
            Text { anchors.verticalCenter: parent.verticalCenter; text: tile.text; color: tile.on ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
        }
        MouseArea { id: tileMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: tile.toggled() }
        Accessible.role: Accessible.CheckBox
        Accessible.name: text
        Accessible.checked: on
    }
    // Mute, the live meter (its handle is the volume, as in the mixer) and the level.
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        FButton {
            iconName: au.s.muted ? "volume-x" : "volume-2"; variant: "ghost"
            selected: !!au.s.muted
            tip: au.s.muted ? "Unmute" : "Mute"
            onClicked: app.setAudio(au.s.id, "muted", !au.s.muted, true)
        }
        MeterFader {
            id: vol
            Layout.fillWidth: true
            implicitHeight: 18
            value: AudioMath.posFromLin(au.s.volume)
            levels: app.levels[au.s.id] || [-60, -60]
            muted: !!au.s.muted
            onMoved: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), false)
            onReleased: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), true)
        }
        Text {
            text: au.s.muted ? "Muted" : AudioMath.toDb(AudioMath.linFromPos(vol.shown)) + " dB"
            color: au.s.muted ? Pal.danger : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            Layout.preferredWidth: 52; horizontalAlignment: Text.AlignRight
        }
    }
    // Where it's heard: the stream (track 1) and the recording (its own mix, track 6).
    Text { text: "Active in"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: -6
        spacing: 6
        OutTile {
            iconName: "record"; text: "Recording"
            on: au.inRecording && !au.monitorOnly
            onToggled: app.setAudio(au.s.id, "inRecording", !au.inRecording, true)
        }
        OutTile {
            iconName: "radio-tower"; text: "Stream"
            on: au.onStream && !au.monitorOnly
            onToggled: app.setAudio(au.s.id, "track", 0, true)
        }
    }
    Text {
        visible: au.monitorOnly
        Layout.topMargin: -6
        text: "Monitor only: you hear it, the stream and recordings don't."
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
    }
    // Monitoring, in OBS's words: what you hear in your headphones.
    Text { text: "Monitor"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    FSelect {
        Layout.fillWidth: true
        Layout.topMargin: -6
        options: [{ name: "Off", value: "off" }, { name: "Monitor and output", value: "monitor_and_output" }, { name: "Monitor only", value: "monitor_only" }]
        currentValue: au.s.monitor
        onPicked: (v) => app.setAudio(au.s.id, "monitor", v, true)
    }
    // What the monitor choice means (all the warnings are about it), right under it.
    Repeater {
        model: au.s.warnings
        delegate: RowLayout { required property var modelData; Layout.fillWidth: true; Layout.topMargin: -6; spacing: 6
            FIcon { name: "triangle-alert"; color: "#fbbf24"; size: 12; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
            Text { text: modelData; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
    }
    // Set once, if ever: closed, it names what isn't at its default.
    readonly property var advancedChanged: [
        Math.abs((au.s.balance === undefined ? 0.5 : au.s.balance) - 0.5) > 0.001 ? "Balance" : "",
        (au.s.syncMs || 0) !== 0 ? "Sync" : "",
        au.s.mono ? "Mono" : "",
        (au.s.tracks || []).slice(1).some(t => t) ? "Tracks" : "",
        au.s.mutedHere ? "Muted in this scene" : "",
        au.s.ducksOthers ? "Ducking" : "",
        au.s.pinned ? "Pinned" : ""].filter(t => t)
    property bool advancedOpen: false
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    MouseArea {
        Layout.fillWidth: true
        implicitHeight: advRow.implicitHeight + 4
        cursorShape: Qt.PointingHandCursor
        onClicked: au.advancedOpen = !au.advancedOpen
        RowLayout {
            id: advRow
            anchors.fill: parent
            spacing: 6
            FIcon { name: "sliders-horizontal"; size: 14; color: Pal.muted }
            Text { text: "Advanced"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true }
            Text { visible: !au.advancedOpen; text: au.advancedChanged.join(" · "); color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
            // Same height and edge as Effects' button below, so the two sections read alike.
            FButton { iconName: au.advancedOpen ? "chevron-up" : "chevron-down"; variant: "ghost"; tip: au.advancedOpen ? "Hide" : "Show"; onClicked: au.advancedOpen = !au.advancedOpen }
        }
    }
    // Labels sit outside the controls, as in the Inspector's Position and Size rows.
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Balance"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 72 }
        FFillSlider {
            Layout.fillWidth: true
            from: 0; to: 1
            fillFrom: 0.5 // from the middle out: which side it leans to
            value: au.s.balance === undefined ? 0.5 : au.s.balance
            format: (v) => AudioMath.balanceText(v)
            // Snaps to center near the middle, so it's easy to put back.
            function snap(v) { return Math.abs(v - 0.5) < 0.06 ? 0.5 : v }
            onMoved: (v) => app.setAudio(au.s.id, "balance", snap(v), false)
            onReleased: (v) => app.setAudio(au.s.id, "balance", snap(v), true)
        }
    }
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Sync offset"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 72 }
        FNumField {
            Layout.fillWidth: true
            label: "ms"
            value: au.s.syncMs || 0
            onCommit: (v) => app.setAudio(au.s.id, "sync", Math.round(v), true)
        }
    }
    // An on/off setting: its icon and name, the switch at the end (the row toggles it too).
    component OptionRow: MouseArea {
        id: opt
        property string iconName
        property string text
        property bool checked
        property string tip
        signal toggled()
        visible: au.advancedOpen
        Layout.fillWidth: true
        implicitHeight: Pal.ctl
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: toggled()
        RowLayout {
            anchors.fill: parent
            spacing: 8
            FIcon { name: opt.iconName; size: 14; color: opt.checked ? Pal.fg : Pal.muted }
            Text { text: opt.text; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight; Layout.fillWidth: true }
            FSwitch { checkable: false; checked: opt.checked; onClicked: opt.toggled() }
        }
        ToolTip.visible: containsMouse && tip !== ""
        ToolTip.delay: 500
        ToolTip.text: tip
    }
    OptionRow {
        iconName: "audio-lines"; text: "Mono"
        checked: !!au.s.mono
        tip: "The same mix on both sides. Fixes a mic heard on one side only."
        onToggled: app.setAudio(au.s.id, "mono", !au.s.mono, true)
    }
    OptionRow {
        visible: au.advancedOpen && au.s.kind === "mic"
        iconName: "arrow-down"; text: "Duck all other audio"
        checked: !!au.s.ducksOthers
        tip: "Other audio sources get quieter while speaking"
        onToggled: app.setDuckUnder(au.s.id, !au.s.ducksOthers)
    }
    // Mics and desktop audio play in every scene; this keeps one quiet while a scene is on air.
    OptionRow {
        visible: au.advancedOpen && !!au.s.global
        iconName: "volume-x"; text: "Always muted in “" + app.editSceneName + "”"
        checked: !!au.s.mutedHere
        tip: "Muted whenever this scene is on air. Other scenes still hear it."
        onToggled: app.setMutedInScene(au.s.id, !au.s.mutedHere)
    }
    OptionRow {
        iconName: "pin"; text: "Pinned to the front of the mixer"
        checked: !!au.s.pinned
        onToggled: app.setMixerPinned(au.s.id, !au.s.pinned)
    }
    Text { visible: au.advancedOpen; text: "Extra tracks"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    RowLayout {
        visible: au.advancedOpen
        Layout.fillWidth: true
        Layout.topMargin: -6
        spacing: 6
        Repeater {
            model: (au.s.tracks || []).length > 1 ? au.s.tracks.length - 1 : 0 // 2–5: 1 is the stream (above)
            delegate: OutTile {
                required property int index
                text: String(index + 2)
                on: !!au.s.tracks[index + 1]
                onToggled: app.setAudio(au.s.id, "track", index + 1, true)
                ToolTip.visible: hoverOn
                ToolTip.delay: 500
                ToolTip.text: "Track " + (index + 2) + ", for multitrack recordings and plugins"
            }
        }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    // Effects: the source's audio filter chain, top to bottom = processing order. Flat rows, like
    // Advanced: the name and menu on one line, then its slider (or a summary) and switch. One with
    // several settings opens (and closes) with a click.
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        FIcon { name: "wand-sparkles"; size: 14; color: Pal.muted }
        Text { text: "Effects" + (au.s.fx.length ? " · " + au.s.fx.length : ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
        // Gives way first when the panel is narrow.
        Text { text: au.s.fx.length > 1 ? "Runs top to bottom" : ""; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.leftMargin: 4; Layout.fillWidth: true; Layout.minimumWidth: 0; elide: Text.ElideRight }
        // The whole chain: copy and paste it, or keep it in a .lobsfx file to share (P2-8).
        FButton {
            id: chainMore
            iconName: "ellipsis"; variant: "ghost"; tip: "Copy, paste, save or load these effects"
            onClicked: chainMenu.popup(chainMore, 0, chainMore.height + 4)
            FMenu {
                id: chainMenu
                property bool canPaste: false
                onAboutToShow: canPaste = app.canPasteAudioFilters()
                FMenuItem { text: "Copy effects"; enabled: au.s.fx.length > 0; onTriggered: app.copyAudioFilters(au.s.id) }
                FMenuItem { text: "Paste effects"; enabled: chainMenu.canPaste; onTriggered: app.pasteAudioFilters([au.s.id], true) }
                FMenuItem { text: "Add pasted effects to the end"; enabled: chainMenu.canPaste && au.s.fx.length > 0; onTriggered: app.pasteAudioFilters([au.s.id], false) }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                FMenuItem { text: "Save effects as…"; enabled: au.s.fx.length > 0; onTriggered: fxSaveDialog.open() }
                FMenuItem { text: "Load effects…"; onTriggered: fxLoadDialog.open() }
            }
        }
        FButton {
            id: addFx
            iconName: "plus"; text: "Effect"; variant: "secondary"
            tip: "Add an audio effect, or start from a preset"
            onClicked: fxMenu.popup(addFx, 0, addFx.height + 4)
            FMenu {
                id: fxMenu
                // Presets replace the chain (one undo step brings it back).
                FMenu {
                    id: presetMenu
                    title: "Start from a preset"
                    Instantiator {
                        active: fxMenu.populated
                        model: app.audioPresets()
                        delegate: FMenuItem { required property var modelData; text: modelData.name; onTriggered: app.applyAudioPreset(au.s.id, modelData.id) }
                        onObjectAdded: (i, o) => presetMenu.insertItem(i, o)
                        onObjectRemoved: (i, o) => presetMenu.removeItem(o)
                    }
                }
                MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Pal.border } }
                Instantiator {
                    active: fxMenu.populated // built while the menu is open
                    model: app.audioFxTypes()
                    delegate: FMenuItem { required property var modelData; text: modelData.label; onTriggered: app.addAudioFilter(au.s.id, modelData.type) }
                    onObjectAdded: (i, o) => fxMenu.insertItem(i + 2, o) // after the presets and the line
                    onObjectRemoved: (i, o) => fxMenu.removeItem(o)
                }
            }
        }
    }
    // Mics: let LOBS listen and set the effects up (P2-9).
    FButton {
        visible: au.s.kind === "mic"
        Layout.fillWidth: true
        iconName: "wand-sparkles"; text: au.s.fx.length ? "Tune my mic again" : "Tune my mic"
        variant: "secondary"
        tip: "Set up your mic in 30 seconds"
        onClicked: micWizard.openFor(au.s.id)
    }
    MicWizard { id: micWizard }
    FxChain { s: au.s; Layout.fillWidth: true }
    FileDialog {
        id: fxSaveDialog
        fileMode: FileDialog.SaveFile; nameFilters: ["LOBS effects (*.lobsfx)"]; defaultSuffix: "lobsfx"
        currentFile: "file:///" + (au.s.name || "Effects") + ".lobsfx"
        onAccepted: app.saveAudioFilters(au.s.id, selectedFile.toString().replace(/^file:\/{3}/, ""))
    }
    FileDialog {
        id: fxLoadDialog
        fileMode: FileDialog.OpenFile; nameFilters: ["LOBS effects (*.lobsfx)", "All files (*)"]
        onAccepted: app.loadAudioFilters([au.s.id], selectedFile.toString().replace(/^file:\/{3}/, ""), true)
    }
}
