import QtQuick
import QtQuick.Layouts

// An audio source's effects, one row each (from AudioSettings, and the mic wizard's results).
// Dragging an effect by its grip: it follows the cursor and the others slide aside to show
// where it lands (as tabs do when they're dragged).
ColumnLayout {
    property var s // an app.mixer entry
    property bool review: false // the mic wizard: settings only (no grip, menu or on/off)
    id: fxChain
    Layout.fillWidth: true
    spacing: 0
    property int dragIndex: -1
    property real dragDy: 0
    property int dropIndex: -1
    function rowAt(i) { return fxRep.itemAt(i) }
    function updateDrop() {
        const me = rowAt(dragIndex)
        if (!me) return
        const mid = me.y + dragDy + me.height / 2
        let to = dragIndex
        for (let i = 0; i < fxRep.count; i++) {
            const r = rowAt(i)
            if (!r || i === dragIndex) continue
            const c = r.y + r.height / 2
            if (i < dragIndex && mid < c) to = Math.min(to, i)
            if (i > dragIndex && mid > c) to = Math.max(to, i)
        }
        dropIndex = to
    }
    function finish() {
        const from = dragIndex, to = dropIndex
        dragIndex = -1; dropIndex = -1; dragDy = 0
        for (let k = from; to >= 0 && k !== to; k += (to > k ? 1 : -1))
            app.moveAudioFilter(fxChain.s.id, k, to > k ? 1 : -1)
    }
    Repeater {
        id: fxRep
        model: fxChain.s.fx.length // by position, so sliders survive edits
        delegate: ColumnLayout {
            id: fxRow
            required property int index
            readonly property var fx: fxChain.s.fx[fxRow.index] || ({ label: "", enabled: false, params: [], known: true })
            readonly property var params: fx.params || []
            readonly property bool inline: params.length === 1 && !fx.obs && fx.known !== false // its one slider on the row
            readonly property bool expandable: (params.length > 0 || !!fx.obs || fx.known === false)
            property bool open: false
            function fmt(p) { return p.kind === "percent" ? Math.round(p.value * 100) + "%" : Number(Number(p.value).toFixed(p.decimals)) + p.suffix } // -50, not -50.0
            readonly property bool dragged: fxChain.dragIndex === index
            // While another row is dragged past this one, it slides a row's height out of the way.
            readonly property real shift: {
                const d = fxChain.dragIndex, t = fxChain.dropIndex, me = fxChain.rowAt(d)
                if (d < 0 || t < 0 || dragged || !me) return 0
                if (d < t && index > d && index <= t) return -me.height
                if (d > t && index < d && index >= t) return me.height
                return 0
            }
            Layout.fillWidth: true
            spacing: 8
            z: dragged ? 2 : 0
            opacity: fx.enabled ? 1 : 0.55
            transform: Translate {
                y: fxRow.dragged ? fxChain.dragDy : fxRow.shift
                Behavior on y { enabled: !fxRow.dragged; NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border; visible: fxRow.index > 0 && !fxRow.dragged }
            Rectangle { // the row being dragged lifts off the list
                Layout.fillWidth: true
                implicitHeight: head.implicitHeight
                radius: Pal.radius
                color: fxRow.dragged ? Pal.s3 : "transparent"
                border.width: fxRow.dragged ? 1 : 0; border.color: Pal.borderStrong
                // Closed, one line: grip, name, what it's set to, open, menu, switch.
                // Open, a one-setting effect gets its control on a second line.
                MouseArea {
                    id: head
                    anchors.fill: parent
                    implicitHeight: headCol.implicitHeight + 8
                    hoverEnabled: true
                    cursorShape: fxRow.expandable ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: if (fxRow.expandable) fxRow.open = !fxRow.open
                    ColumnLayout {
                        id: headCol
                        anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 2; anchors.rightMargin: 2
                        spacing: 2
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            FIcon {
                                name: "grip-vertical"; size: 14
                                color: grip.containsMouse || grip.pressed ? Pal.fg : Pal.muted
                                visible: fxChain.s.fx.length > 1 && !fxChain.review
                                MouseArea {
                                    id: grip
                                    anchors.fill: parent; anchors.margins: -6
                                    hoverEnabled: true
                                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    preventStealing: true
                                    property real startY: 0
                                    onPressed: (m) => { startY = mapToItem(fxChain, m.x, m.y).y; fxChain.dragIndex = fxRow.index; fxChain.dropIndex = fxRow.index }
                                    onPositionChanged: (m) => { if (pressed) { fxChain.dragDy = mapToItem(fxChain, m.x, m.y).y - startY; fxChain.updateDrop() } }
                                    onReleased: fxChain.finish()
                                    onCanceled: { fxChain.dragIndex = -1; fxChain.dropIndex = -1; fxChain.dragDy = 0 }
                                }
                            }
                            Text {
                                text: fxRow.fx.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
                                elide: Text.ElideRight
                                Layout.minimumWidth: Math.min(implicitWidth, 60) // the name keeps its width; the summary gives way first
                            }
                            Text {
                                Layout.fillWidth: true
                                text: !fxRow.expandable || fxRow.open ? ""
                                    : fxRow.fx.type === "duck" ? ((app.mixer.find(m => m.id === fxRow.fx.sidechain) || {}).name ? "Under " + app.mixer.find(m => m.id === fxRow.fx.sidechain).name : "No voice chosen")
                                    : fxRow.params.slice(0, 2).map(p => fxRow.fmt(p)).join(" · ")
                                elide: Text.ElideRight
                                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                            }
                            FButton {
                                visible: fxRow.expandable
                                iconName: fxRow.open ? "chevron-up" : "chevron-down"; variant: "ghost"
                                tip: fxRow.open ? "Close" : "Open"
                                onClicked: fxRow.open = !fxRow.open
                            }
                            FButton {
                                visible: !fxChain.review
                                id: fxMore
                                iconName: "ellipsis"; variant: "ghost"; tip: "More"
                                onClicked: rowMenu.popup(fxMore, 0, fxMore.height + 4)
                                FMenu {
                                    id: rowMenu
                                    FMenuItem { text: "Move up"; enabled: fxRow.index > 0; onTriggered: app.moveAudioFilter(fxChain.s.id, fxRow.index, -1) }
                                    FMenuItem { text: "Move down"; enabled: fxRow.index < fxChain.s.fx.length - 1; onTriggered: app.moveAudioFilter(fxChain.s.id, fxRow.index, 1) }
                                    FMenuItem { text: "Remove"; onTriggered: app.removeAudioFilter(fxChain.s.id, fxRow.index) }
                                }
                            }
                            FSwitch { visible: !fxChain.review; checkable: false; checked: fxRow.fx.enabled; onClicked: app.setAudioFilterEnabled(fxChain.s.id, fxRow.index, !fxRow.fx.enabled) }
                        }
                        RowLayout {
                            visible: fxRow.open && fxRow.inline
                            Layout.fillWidth: true
                            Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0 // under the name, past the grip
                            spacing: 8
                            // One setting: its control, unlabeled (the effect's name says it).
                            FxControl {
                                visible: fxRow.inline
                                Layout.fillWidth: true
                                p: fxRow.params[0] || ({ key: "", label: "", suffix: "", min: 0, max: 1, decimals: 0, def: 0, kind: "level", presets: [], value: 0 })
                                sourceId: fxChain.s.id; fxIndex: fxRow.index
                                showLabel: false
                            }
                        }
                    }
                }
            }
            // Ducking: which voice it ducks under.
            FSelect {
                visible: fxRow.open && fxRow.fx.type === "duck"
                Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0
                placeholder: "Choose the voice to duck under"
                options: app.mixer.filter(m => m.id !== fxChain.s.id).map(m => ({ name: "Under " + m.name, value: m.id }))
                currentValue: fxRow.fx.sidechain || ""
                onPicked: (v) => app.setAudioFilterText(fxChain.s.id, fxRow.index, "sidechain_source", v)
            }
            // Open: its settings, each labeled (there are several), labels outside the controls.
            // A noise gate's thresholds share one bar over the live level; an EQ gets faders.
            readonly property bool gate: fx.type === "noise_gate" && params.length >= 2
            readonly property bool eq: fx.type === "eq"
            FxGateBar {
                visible: fxRow.open && fxRow.gate
                Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0
                close: fxRow.params[0] || ({ key: "", min: -96, max: 0, value: -32 })
                open: fxRow.params[1] || ({ key: "", min: -96, max: 0, value: -26 })
                sourceId: fxChain.s.id; fxIndex: fxRow.index
            }
            FxEqFaders {
                visible: fxRow.open && fxRow.eq
                Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0
                bands: fxRow.eq ? fxRow.params : []
                sourceId: fxChain.s.id; fxIndex: fxRow.index
            }
            Repeater {
                model: fxRow.open && !fxRow.inline && !fxRow.eq ? fxRow.params.length : 0
                delegate: FxControl {
                    required property int index
                    // The gate's thresholds are on its bar; times share one row below.
                    visible: !(fxRow.gate && index < 2) && (fxRow.params[index] || {}).kind !== "time"
                    Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0
                    p: fxRow.params[index] || ({ key: "", label: "", suffix: "", min: 0, max: 1, decimals: 0, def: 0, kind: "level", presets: [], value: 0 })
                    sourceId: fxChain.s.id; fxIndex: fxRow.index
                }
            }
            // Times (attack, hold, release): set once, so small fields side by side, label inside.
            RowLayout {
                visible: fxRow.open && !fxRow.inline && fxRow.params.some(p => p.kind === "time")
                Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0
                spacing: 6
                Repeater {
                    model: fxRow.open ? fxRow.params.map((p, i) => ({ p: p, i: i })).filter(x => x.p.kind === "time") : []
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.fillWidth: true; Layout.preferredWidth: 1 // equal columns
                        spacing: 4
                        Text { text: modelData.p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                        FNumField {
                        Layout.fillWidth: true; Layout.minimumWidth: 52
                        suffix: "ms"; step: 1; live: true
                        from: modelData.p.min; to: modelData.p.max
                        value: Math.round(modelData.p.value)
                        onCommit: (v) => app.setAudioFilterValue(fxChain.s.id, fxRow.index, modelData.p.key, v, true)
                        onScrubbed: (v, final) => app.setAudioFilterValue(fxChain.s.id, fxRow.index, modelData.p.key, v, final)
                        }
                    }
                }
            }
            ObsProperties {
                visible: fxRow.open && !!fxRow.fx.obs
                active: visible
                Layout.leftMargin: 22
                scope: "audio"; targetId: fxChain.s.id; index: fxRow.index
            }
            Text {
                visible: fxRow.open && fxRow.fx.known === false
                Layout.fillWidth: true; Layout.leftMargin: fxChain.s.fx.length > 1 && !fxChain.review ? 20 : 0; wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                text: "Imported from OBS. No controls in LOBS yet."
            }
            Item { implicitHeight: 2 }
        }
    }
}
