import QtQuick
import QtQuick.Layouts
import "AudioMath.js" as AudioMath

// An audio source's settings: volume, monitoring, balance, sync, mono, tracks, warnings and its
// effect chain. In the Inspector when an audio source is selected (in Sources or the mixer).
ColumnLayout {
    id: au
    required property var s // an app.mixer entry
    spacing: 10
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Audio"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.fillWidth: true }
        FBadge { text: au.s.global ? "Global" : "Scene" }
        FButton {
            iconName: au.s.pinned ? "pin-off" : "pin"; variant: "ghost"
            selected: !!au.s.pinned
            tip: au.s.pinned ? "Unpin" : "Pin to the front of the mixer"
            onClicked: app.setMixerPinned(au.s.id, !au.s.pinned)
        }
    }
    FFillSlider {
        Layout.fillWidth: true
        label: "Volume"
        value: AudioMath.posFromLin(au.s.volume)
        format: (p) => AudioMath.toDb(AudioMath.linFromPos(p)) + " dB"
        onMoved: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), false)
        onReleased: (v) => app.setAudio(au.s.id, "volume", AudioMath.linFromPos(v), true)
    }
    FSelect {
        Layout.fillWidth: true
        options: [{ name: "Monitor off", value: "off" }, { name: "Monitor and output", value: "monitor_and_output" }, { name: "Monitor only (not in output)", value: "monitor_only" }]
        currentValue: au.s.monitor
        onPicked: (v) => app.setAudio(au.s.id, "monitor", v, true)
    }
    FFillSlider {
        Layout.fillWidth: true
        label: "Balance"
        from: 0; to: 1
        value: au.s.balance === undefined ? 0.5 : au.s.balance
        format: (v) => AudioMath.balanceText(v)
        // A small detent at center, so the middle is easy to hit.
        function snap(v) { return Math.abs(v - 0.5) < 0.03 ? 0.5 : v }
        onMoved: (v) => app.setAudio(au.s.id, "balance", snap(v), false)
        onReleased: (v) => app.setAudio(au.s.id, "balance", snap(v), true)
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        FNumField {
            Layout.fillWidth: true
            label: "Sync offset (ms)"
            value: au.s.syncMs || 0
            onCommit: (v) => app.setAudio(au.s.id, "sync", Math.round(v), true)
        }
        FSwitch {
            text: "Mono"
            checkable: false
            checked: !!au.s.mono
            onClicked: app.setAudio(au.s.id, "mono", !au.s.mono, true)
        }
    }
    Text {
        visible: (au.s.syncMs || 0) !== 0 || !!au.s.mono
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        text: [(au.s.syncMs || 0) > 0 ? "Audio is delayed " + au.s.syncMs + " ms to match slower video." : (au.s.syncMs || 0) < 0 ? "Audio plays " + (-au.s.syncMs) + " ms early." : "",
               au.s.mono ? "Mono: the same mix on both sides. Fixes a mic heard on one side only." : ""].filter(t => t).join(" ")
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 2
        Text { text: "Tracks"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body; Layout.fillWidth: true }
        Repeater {
            model: au.s.tracks
            delegate: FButton {
                required property var modelData
                required property int index
                text: String(index + 1)
                width: 26
                variant: modelData ? "secondary" : "ghost"
                selected: modelData
                tip: "Track " + (index + 1) + (index === 0 ? " (stream)" : "")
                onClicked: app.setAudio(au.s.id, "track", index, true)
            }
        }
    }
    Repeater {
        model: au.s.warnings
        delegate: RowLayout { required property var modelData; Layout.fillWidth: true; spacing: 6
            FIcon { name: "triangle-alert"; color: "#fbbf24"; size: 12; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
            Text { text: modelData; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
    }
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    // Effects: the source's audio filter chain, top to bottom = processing order.
    RowLayout {
        Layout.fillWidth: true
        spacing: 4
        Text { text: "Effects" + (au.s.fx.length ? " · " + au.s.fx.length : ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true }
        FButton {
            id: addFx
            iconName: "plus"; text: "Effect"; variant: "secondary"
            tip: "Add an audio effect"
            onClicked: fxMenu.popup(addFx, 0, addFx.height + 4)
            FMenu {
                id: fxMenu
                Instantiator {
                    active: fxMenu.populated // built while the menu is open
                    model: app.audioFxTypes()
                    delegate: FMenuItem { required property var modelData; text: modelData.label; onTriggered: app.addAudioFilter(au.s.id, modelData.type) }
                    onObjectAdded: (i, o) => fxMenu.insertItem(i, o)
                    onObjectRemoved: (i, o) => fxMenu.removeItem(o)
                }
            }
        }
    }
    Repeater {
        model: au.s.fx.length // by position, so sliders survive edits
        delegate: Rectangle {
            id: fxCard
            required property int index
            readonly property var fx: au.s.fx[index] || ({ label: "", enabled: false, params: [], known: true })
            Layout.fillWidth: true
            implicitHeight: fxCol.implicitHeight + 16
            radius: Pal.radius
            color: Pal.s3
            border.width: 1; border.color: Pal.border
            opacity: fx.enabled ? 1 : 0.6
            ColumnLayout {
                id: fxCol
                anchors.fill: parent; anchors.margins: 8
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    FSwitch { checkable: false; checked: fxCard.fx.enabled; onClicked: app.setAudioFilterEnabled(au.s.id, fxCard.index, !fxCard.fx.enabled) }
                    Text { text: fxCard.fx.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true; elide: Text.ElideRight; Layout.leftMargin: 4 }
                    FButton { iconName: "chevron-up"; variant: "ghost"; enabled: fxCard.index > 0; tip: "Earlier in the chain"; onClicked: app.moveAudioFilter(au.s.id, fxCard.index, -1) }
                    FButton { iconName: "chevron-down"; variant: "ghost"; enabled: fxCard.index < au.s.fx.length - 1; tip: "Later in the chain"; onClicked: app.moveAudioFilter(au.s.id, fxCard.index, 1) }
                    FButton { iconName: "x"; variant: "ghost"; tip: "Remove"; onClicked: app.removeAudioFilter(au.s.id, fxCard.index) }
                }
                Repeater {
                    model: fxCard.fx.params.length
                    delegate: FFillSlider {
                        required property int index
                        readonly property var p: fxCard.fx.params[index] || ({ label: "", min: 0, max: 1, value: 0, decimals: 0, suffix: "", key: "" })
                        Layout.fillWidth: true
                        label: p.label
                        from: p.min; to: p.max; decimals: p.decimals
                        value: p.value
                        format: (v) => v.toFixed(p.decimals) + p.suffix
                        onMoved: (v) => app.setAudioFilterValue(au.s.id, fxCard.index, p.key, v, false)
                        onReleased: (v) => app.setAudioFilterValue(au.s.id, fxCard.index, p.key, v, true)
                    }
                }
                ObsProperties {
                    visible: !!fxCard.fx.obs
                    active: visible
                    scope: "audio"; targetId: au.s.id; index: fxCard.index
                }
                Text {
                    visible: !fxCard.fx.known
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: "Imported from OBS. No controls in LOBS yet."
                }
            }
        }
    }
    Text {
        visible: au.s.fx.length === 0
        Layout.fillWidth: true; wrapMode: Text.WordWrap
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        text: "Applies to this source in every scene."
    }
}
