import QtQuick
import QtQuick.Layouts

// One audio-effect setting, with the control that suits its kind (from app.mixer's fx params):
//   level / percent: a bar to drag and the value to type
//   gain:            a bar that fills out from 0 dB, snaps to it, double-click resets
//   ratio:           preset chips and the value to type
//   time:            a number field (type, scroll, drag sideways, or − / +)
// A label sits above it when the effect has more than one setting. Everything in the row can
// shrink, so a narrow panel never pushes the Inspector wider.
ColumnLayout {
    id: fc
    required property var p          // {key, label, suffix, min, max, decimals, def, kind, presets, value}
    required property string sourceId
    required property int fxIndex
    property bool showLabel: true
    spacing: 4

    readonly property bool pct: p.kind === "percent"
    function set(v, final) { app.setAudioFilterValue(fc.sourceId, fc.fxIndex, fc.p.key, Math.max(fc.p.min, Math.min(fc.p.max, v)), final) }
    function shown(v) { return pct ? Math.round(v * 100) : Number(v.toFixed(p.decimals)) }

    Text {
        visible: fc.showLabel
        text: fc.p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        Layout.fillWidth: true; elide: Text.ElideRight
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        // Level, percent and gain: the bar.
        FFillSlider {
            id: bar
            visible: fc.p.kind === "level" || fc.p.kind === "percent" || fc.p.kind === "gain"
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            from: fc.p.min; to: fc.p.max; decimals: fc.p.decimals
            value: fc.p.value
            fillFrom: fc.p.kind === "gain" ? (0 - fc.p.min) / (fc.p.max - fc.p.min) : 0
            format: () => "" // the number is in the box beside it
            // Gains snap to 0 dB near the middle.
            function snap(v) { return fc.p.kind === "gain" && Math.abs(v) < (fc.p.max - fc.p.min) * 0.025 ? 0 : v }
            onMoved: (v) => fc.set(snap(v), false)
            onReleased: (v) => fc.set(snap(v), true)
            onReset: fc.set(fc.p.kind === "gain" ? 0 : fc.p.def, true)
        }

        // Ratio: the common values as chips.
        Repeater {
            model: fc.p.kind === "ratio" ? fc.p.presets : []
            delegate: FButton {
                required property var modelData
                readonly property bool on: Math.abs(fc.p.value - modelData) < 0.05
                text: modelData + ":1"
                Layout.fillWidth: true; Layout.minimumWidth: 30 // the chips share the row
                variant: on ? "secondary" : "ghost"
                selected: on
                onClicked: fc.set(modelData, true)
            }
        }

        // Times: − field +.
        FButton {
            visible: fc.p.kind === "time"
            iconName: "minus"; variant: "ghost"; tip: "Shorter"
            onClicked: fc.set(fc.p.value - (fc.p.value <= 20 ? 1 : fc.p.value <= 200 ? 5 : 10), true)
        }

        // The value, to type (every kind has one).
        FNumField {
            Layout.fillWidth: fc.p.kind === "time"
            Layout.preferredWidth: fc.p.kind === "time" ? -1 : 76
            Layout.minimumWidth: 56
            suffix: fc.pct ? "%" : fc.p.suffix.trim()
            from: fc.pct ? 0 : fc.p.min; to: fc.pct ? 100 : fc.p.max
            step: fc.p.kind === "time" ? 1 : Math.pow(10, -fc.p.decimals)
            live: true
            value: fc.shown(fc.p.value)
            onCommit: (v) => fc.set(fc.pct ? v / 100 : v, true)
            onScrubbed: (v, final) => fc.set(fc.pct ? v / 100 : v, final)
        }

        FButton {
            visible: fc.p.kind === "time"
            iconName: "plus"; variant: "ghost"; tip: "Longer"
            onClicked: fc.set(fc.p.value + (fc.p.value < 20 ? 1 : fc.p.value < 200 ? 5 : 10), true)
        }
    }
}
