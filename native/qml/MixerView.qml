import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Audio mixer. Packs into whatever space the panel has, like OBS:
//  - wide and short (a bottom dock): vertical channel strips side by side,
//  - narrow or tall: one compact row per source.
// Each source shows its name, a stereo meter with the fader on it, mute and monitor. Monitoring
// mode, tracks, effects and warnings open from "..." (small badges show effects and warnings).
Item {
    id: mv
    function toDb(lin) { return lin <= 0.0001 ? "-inf" : (20 * Math.log10(lin)).toFixed(1) }
    // Fader shaped like OBS's: position 0..1 maps to -60..0 dB.
    function posFromLin(lin) { return lin <= 0.001 ? 0 : Math.max(0, Math.min(1, (20 * Math.log10(lin) + 60) / 60)) }
    function linFromPos(p) { return p <= 0 ? 0 : Math.pow(10, (p * 60 - 60) / 20) }
    readonly property int count: app.mixer.length
    // Layout: Auto picks strips for a wide, short panel, unless the strips would scroll
    // sideways while rows would fit; Rows / Strips force one (remembered).
    property string layoutMode: app.uiValue("mixer/layout", "auto")
    readonly property real bodyH: height - toolbar.height
    readonly property int shownCount: { let n = 0; for (let i = 0; i < count; ++i) if (matches(src(i))) ++n; return n }
    readonly property bool autoStrips: {
        const wideShort = width > bodyH * 1.3 && bodyH >= 170
        const stripsFit = width >= shownCount * 88 + 12
        const rowsFit = bodyH >= shownCount * 56 + 8
        return wideShort && (stripsFit || !rowsFit)
    }
    readonly property bool strips: layoutMode === "strips" || (layoutMode === "auto" && autoStrips)
    property bool searching: false
    function matches(s) { return !searchField.query || String(s.name || "").toLowerCase().includes(searchField.query) }
    function cycleLayout() {
        layoutMode = layoutMode === "auto" ? "rows" : layoutMode === "rows" ? "strips" : "auto"
        app.setUiValue("mixer/layout", layoutMode)
    }
    function src(i) { return app.mixer[i] || ({ id: "", name: "", icon: "", color: "#555", volume: 0, muted: false, monitor: "off", tracks: [], global: true, warnings: [], fx: [], mono: false, balance: 0.5, syncMs: 0 }) }
    function balanceText(b) { const p = Math.round((b - 0.5) * 200); return p === 0 ? "Center" : p < 0 ? "L " + (-p) + "%" : "R " + p + "%" }
    function monitorIcon(m) { return m === "off" ? "headphone-off" : "headphones" }
    function monitorTip(m) { return m === "off" ? "Not monitored. Click to hear it in your headphones (and keep it in the output)." : m === "monitor_only" ? "Monitor only: you hear it, viewers don't. Click to turn monitoring off." : "Monitored: you hear it, and it's in the output. Click to turn monitoring off." }
    function toggleMonitor(s) { app.setAudio(s.id, "monitor", s.monitor === "off" ? "monitor_and_output" : "off", true) }

    component IconToggle: AbstractButton {
        id: it
        property string iconName
        property color tint: Pal.muted
        property string tip
        implicitWidth: 24; implicitHeight: 24
        hoverEnabled: true
        background: Rectangle { radius: Pal.radius - 2; color: it.down ? Pal.s6 : it.hovered ? Pal.s5 : "transparent" }
        contentItem: Item { FIcon { anchors.centerIn: parent; name: it.iconName; size: 14; color: it.hovered && it.tint === Pal.muted ? Pal.fg : it.tint } }
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: tip
    }
    component Badges: Row {
        property var s
        spacing: 3
        Rectangle {
            visible: s.fx.length > 0
            width: fxt.implicitWidth + 8; height: 14; radius: 7; color: Pal.s5
            anchors.verticalCenter: parent.verticalCenter
            Text { id: fxt; anchors.centerIn: parent; text: "FX " + s.fx.length; color: Pal.muted; font.family: Pal.font; font.pixelSize: 9; font.weight: 600 }
        }
        Repeater {
            model: [s.mono ? "Mono" : "", Math.abs((s.balance === undefined ? 0.5 : s.balance) - 0.5) > 0.005 ? mv.balanceText(s.balance) : "", s.syncMs ? (s.syncMs > 0 ? "+" : "") + s.syncMs + " ms" : ""].filter(t => t)
            delegate: Rectangle {
                required property string modelData
                width: bt.implicitWidth + 8; height: 14; radius: 7; color: Pal.s5
                anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                Text { id: bt; anchors.centerIn: parent; text: modelData; color: Pal.muted; font.family: Pal.font; font.pixelSize: 9; font.weight: 600 }
            }
        }
        FIcon { visible: s.warnings.length > 0; name: "triangle-alert"; size: 12; color: "#fbbf24"; anchors.verticalCenter: parent.verticalCenter }
    }

    // ---------- toolbar: search, layout ----------
    Item {
        id: toolbar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 34
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 6
            spacing: 2
            FSearchField {
                id: searchField
                visible: mv.searching
                Layout.fillWidth: true
                implicitHeight: 26
                placeholderText: "Search audio"
                onDismissed: { mv.searching = false; text = "" }
            }
            Text {
                visible: !mv.searching
                Layout.fillWidth: true
                text: mv.count === 1 ? "1 input" : mv.count + " inputs"
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            }
            FButton {
                iconName: "search"; variant: "ghost"; selected: mv.searching
                tip: "Search"
                onClicked: { mv.searching = !mv.searching; if (mv.searching) searchField.forceActiveFocus(); else searchField.text = "" }
            }
            FButton {
                iconName: mv.layoutMode === "rows" ? "rows-3" : mv.layoutMode === "strips" ? "columns-3" : "wand-sparkles"
                variant: "ghost"; selected: mv.layoutMode !== "auto"
                tip: (mv.layoutMode === "auto" ? "Layout: Auto (" + (mv.strips ? "strips" : "rows") + ")" : mv.layoutMode === "rows" ? "Layout: Rows" : "Layout: Strips") + ". Click to change."
                onClicked: mv.cycleLayout()
            }
        }
    }

    // ---------- rows (narrow / tall) ----------
    FScrollView {
        visible: !mv.strips
        anchors { left: parent.left; right: parent.right; top: toolbar.bottom; bottom: parent.bottom }
        clip: true
        contentWidth: availableWidth
        Column {
            width: parent.width
            topPadding: 4; bottomPadding: 4
            Repeater {
                model: mv.count // by position, so faders aren't recreated mid-drag
                delegate: Item {
                    id: row
                    required property int index
                    readonly property var s: mv.src(index)
                    visible: mv.matches(s) // filtered in place: delegates stay put, so drags aren't interrupted
                    width: parent.width
                    height: 56
                    Rectangle { anchors.fill: parent; anchors.leftMargin: 4; anchors.rightMargin: 4; radius: Pal.radius - 2; color: rowHover.hovered ? Pal.hover : "transparent" }
                    HoverHandler { id: rowHover }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12; anchors.rightMargin: 8; anchors.topMargin: 5; anchors.bottomMargin: 8
                        spacing: 3
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Rectangle { width: 6; height: 6; radius: 3; color: row.s.color }
                            FIcon { name: row.s.icon; size: 13; color: row.s.muted ? Pal.muted : Pal.fg }
                            Text { text: row.s.name; color: row.s.muted ? Pal.muted : Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true }
                            Badges { s: row.s }
                            Text { text: mv.toDb(row.s.volume) + " dB"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 46; horizontalAlignment: Text.AlignRight }
                            IconToggle { iconName: row.s.muted ? "volume-x" : "volume-2"; tint: row.s.muted ? Pal.danger : Pal.muted; tip: row.s.muted ? "Muted. Click to unmute." : "Mute"; onClicked: app.setAudio(row.s.id, "muted", !row.s.muted, true) }
                            IconToggle { iconName: mv.monitorIcon(row.s.monitor); tint: row.s.monitor === "off" ? Pal.muted : Pal.link; tip: mv.monitorTip(row.s.monitor); onClicked: mv.toggleMonitor(row.s) }
                            IconToggle { id: rowMore; iconName: "ellipsis"; tip: "Monitoring, tracks and effects"; onClicked: details.openFor(row.index, rowMore) }
                        }
                        MeterFader {
                            Layout.fillWidth: true
                            implicitHeight: 14
                            value: mv.posFromLin(row.s.volume)
                            levels: app.levels[row.s.id] || [-60, -60]
                            muted: row.s.muted
                            onMoved: (v) => app.setAudio(row.s.id, "volume", mv.linFromPos(v), false)
                            onReleased: (v) => app.setAudio(row.s.id, "volume", mv.linFromPos(v), true)
                        }
                    }
                    Rectangle { visible: row.index < mv.count - 1; anchors.bottom: parent.bottom; x: 12; width: parent.width - 24; height: 1; color: Pal.border }
                }
            }
        }
    }

    // ---------- strips (wide / short) ----------
    ScrollView {
        visible: mv.strips
        anchors { left: parent.left; right: parent.right; top: toolbar.bottom; bottom: parent.bottom }
        clip: true
        Accessible.role: Accessible.Pane
        Accessible.name: "Audio mixer"
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        contentHeight: availableHeight
        Row {
            height: parent.height
            leftPadding: 6; topPadding: 6; bottomPadding: 6
            spacing: 4
            Repeater {
                model: mv.count
                delegate: Rectangle {
                    id: strip
                    required property int index
                    readonly property var s: mv.src(index)
                    visible: mv.matches(s)
                    width: 84
                    height: parent.height - 12
                    radius: Pal.radius
                    color: Pal.s3
                    border.width: 1; border.color: Pal.border
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 4
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Rectangle { width: 6; height: 6; radius: 3; color: strip.s.color }
                            Text { text: strip.s.name; color: strip.s.muted ? Pal.muted : Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption; font.weight: 550; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: mv.toDb(strip.s.volume) + " dB"; color: Pal.muted; font.family: Pal.font; font.pixelSize: 10; Layout.fillWidth: true }
                            Badges { s: strip.s }
                        }
                        MeterFader {
                            vertical: true
                            Layout.alignment: Qt.AlignHCenter
                            Layout.fillHeight: true
                            implicitWidth: 22
                            value: mv.posFromLin(strip.s.volume)
                            levels: app.levels[strip.s.id] || [-60, -60]
                            muted: strip.s.muted
                            onMoved: (v) => app.setAudio(strip.s.id, "volume", mv.linFromPos(v), false)
                            onReleased: (v) => app.setAudio(strip.s.id, "volume", mv.linFromPos(v), true)
                        }
                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 0
                            IconToggle { iconName: strip.s.muted ? "volume-x" : "volume-2"; tint: strip.s.muted ? Pal.danger : Pal.muted; tip: strip.s.muted ? "Muted. Click to unmute." : "Mute"; onClicked: app.setAudio(strip.s.id, "muted", !strip.s.muted, true) }
                            IconToggle { iconName: mv.monitorIcon(strip.s.monitor); tint: strip.s.monitor === "off" ? Pal.muted : Pal.link; tip: mv.monitorTip(strip.s.monitor); onClicked: mv.toggleMonitor(strip.s) }
                            IconToggle { id: stripMore; iconName: "ellipsis"; tip: "Monitoring, tracks and effects"; onClicked: details.openFor(strip.index, stripMore) }
                        }
                    }
                }
            }
        }
    }

    Text {
        visible: mv.shownCount === 0
        anchors.centerIn: parent
        width: parent.width - 48
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
        color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
        text: mv.count && !mv.shownCount ? "No audio inputs match \u201c" + searchField.text + "\u201d." : "No audio sources. Add a Mic or Desktop audio from Sources."
    }

    // ---------- details: monitoring, tracks, effects, warnings ----------
    Popup {
        id: details
        property int idx: -1
        readonly property var s: mv.src(idx)
        property bool fxOpen: true
        // "..." toggles: pressing it while open closes the popover (press outside), and the
        // release that follows must not open it again.
        property double closedAt: 0
        property int closedIdx: -1
        onClosed: { closedAt = Date.now(); closedIdx = idx }
        function openFor(i, anchorItem) {
            if (i === closedIdx && Date.now() - closedAt < 400) return
            if (opened && i === idx) return close()
            idx = i
            const p = anchorItem.mapToItem(mv, 0, anchorItem.height + 4)
            x = Math.max(4, Math.min(mv.width - width - 4, p.x - width + anchorItem.width))
            y = p.y
            open()
        }
        width: 300
        height: Math.min(contentCol.implicitHeight + 28, mv.Window.window ? mv.Window.window.height * 0.7 : 600)
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property bool coversPreview: false
        // Only step the native preview aside if this popup actually overlaps it (no flash otherwise).
        function overlapsPreview() {
            const w = parent ? parent.Window.window : null
            const p = w ? w.previewItem : null
            if (!p || !p.visible || !parent) return false
            const a = p.mapToGlobal(0, 0), b = parent.mapToGlobal(x, y)
            return b.x < a.x + p.width && b.x + width > a.x && b.y < a.y + p.height && b.y + height > a.y
        }
        onAboutToShow: { coversPreview = overlapsPreview(); if (coversPreview) app.popupOpened() }
        onAboutToHide: if (coversPreview) { coversPreview = false; app.popupClosed() }
        background: Rectangle { radius: Pal.radius + 4; color: Pal.s4; border.width: 1; border.color: Pal.border }
        contentItem: Flickable {
            clip: true
            contentHeight: contentCol.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ColumnLayout {
                id: contentCol
                width: parent.width
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    FIcon { name: details.s.icon; color: Pal.fg }
                    Text { text: details.s.name; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.fillWidth: true; elide: Text.ElideRight }
                    FBadge { text: details.s.global ? "Global" : "Scene" }
                }
                FFillSlider {
                    Layout.fillWidth: true
                    label: "Volume"
                    value: mv.posFromLin(details.s.volume)
                    format: (p) => mv.toDb(mv.linFromPos(p)) + " dB"
                    onMoved: (v) => app.setAudio(details.s.id, "volume", mv.linFromPos(v), false)
                    onReleased: (v) => app.setAudio(details.s.id, "volume", mv.linFromPos(v), true)
                }
                FSelect {
                    Layout.fillWidth: true
                    options: [{ name: "Monitor off", value: "off" }, { name: "Monitor and output", value: "monitor_and_output" }, { name: "Monitor only (not in output)", value: "monitor_only" }]
                    currentValue: details.s.monitor
                    onPicked: (v) => app.setAudio(details.s.id, "monitor", v, true)
                }
                FFillSlider {
                    Layout.fillWidth: true
                    label: "Balance"
                    from: 0; to: 1
                    value: details.s.balance === undefined ? 0.5 : details.s.balance
                    format: (v) => mv.balanceText(v)
                    // A small detent at center, so the middle is easy to hit.
                    function snap(v) { return Math.abs(v - 0.5) < 0.03 ? 0.5 : v }
                    onMoved: (v) => app.setAudio(details.s.id, "balance", snap(v), false)
                    onReleased: (v) => app.setAudio(details.s.id, "balance", snap(v), true)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    FNumField {
                        Layout.fillWidth: true
                        label: "Sync offset (ms)"
                        value: details.s.syncMs || 0
                        onCommit: (v) => app.setAudio(details.s.id, "sync", Math.round(v), true)
                    }
                    FSwitch {
                        text: "Mono"
                        checkable: false
                        checked: !!details.s.mono
                        onClicked: app.setAudio(details.s.id, "mono", !details.s.mono, true)
                    }
                }
                Text {
                    visible: (details.s.syncMs || 0) !== 0 || !!details.s.mono
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: [(details.s.syncMs || 0) > 0 ? "Audio is delayed " + details.s.syncMs + " ms to line up with slower video." : (details.s.syncMs || 0) < 0 ? "Audio plays " + (-details.s.syncMs) + " ms early." : "",
                           details.s.mono ? "Mono: both channels carry the same mix (useful for a mic that only comes in on one side)." : ""].filter(t => t).join(" ")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: "Tracks"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body; Layout.fillWidth: true }
                    Repeater {
                        model: details.s.tracks
                        delegate: FButton {
                            required property var modelData
                            required property int index
                            text: String(index + 1)
                            width: 26
                            variant: modelData ? "secondary" : "ghost"
                            selected: modelData
                            tip: "Track " + (index + 1) + (index === 0 ? " (stream)" : "")
                            onClicked: app.setAudio(details.s.id, "track", index, true)
                        }
                    }
                }
                Repeater {
                    model: details.s.warnings
                    delegate: RowLayout { required property var modelData; Layout.fillWidth: true; spacing: 6
                        FIcon { name: "triangle-alert"; color: "#fbbf24"; size: 12; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
                        Text { text: modelData; color: "#fbbf24"; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true } }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
                // Effects: the source's audio filter chain, top to bottom = processing order.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: "Effects" + (details.s.fx.length ? " · " + details.s.fx.length : ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true }
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
                                delegate: FMenuItem { required property var modelData; text: modelData.label; onTriggered: app.addAudioFilter(details.s.id, modelData.type) }
                                onObjectAdded: (i, o) => fxMenu.insertItem(i, o)
                                onObjectRemoved: (i, o) => fxMenu.removeItem(o)
                            }
                        }
                    }
                }
                Repeater {
                    model: details.s.fx.length // by position, so sliders survive edits
                    delegate: Rectangle {
                        id: fxCard
                        required property int index
                        readonly property var fx: details.s.fx[index] || ({ label: "", enabled: false, params: [], known: true })
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
                                FSwitch { checkable: false; checked: fxCard.fx.enabled; onClicked: app.setAudioFilterEnabled(details.s.id, fxCard.index, !fxCard.fx.enabled) }
                                Text { text: fxCard.fx.label; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; Layout.fillWidth: true; elide: Text.ElideRight; Layout.leftMargin: 4 }
                                FButton { iconName: "chevron-up"; variant: "ghost"; enabled: fxCard.index > 0; tip: "Earlier in the chain"; onClicked: app.moveAudioFilter(details.s.id, fxCard.index, -1) }
                                FButton { iconName: "chevron-down"; variant: "ghost"; enabled: fxCard.index < details.s.fx.length - 1; tip: "Later in the chain"; onClicked: app.moveAudioFilter(details.s.id, fxCard.index, 1) }
                                FButton { iconName: "x"; variant: "ghost"; tip: "Remove"; onClicked: app.removeAudioFilter(details.s.id, fxCard.index) }
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
                                    onMoved: (v) => app.setAudioFilterValue(details.s.id, fxCard.index, p.key, v, false)
                                    onReleased: (v) => app.setAudioFilterValue(details.s.id, fxCard.index, p.key, v, true)
                                }
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
                    visible: details.s.fx.length === 0
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                    text: "Applies to this source in every scene."
                }
            }
        }
    }
}
