import QtQuick
import QtQuick.Layouts
import "AudioMath.js" as AudioMath

// A source's input settings (device, window, page, file, text, color). Used by the source
// inspector and by the Source section of the layer inspector, so both edit the same things.
ColumnLayout {
    id: sf
    property var d: ({})            // source info: id, kind, settings, hasDevice
    property bool active: true      // false while hidden: skips listing devices
    readonly property var st: d.settings || ({})
    spacing: 8
    Layout.fillWidth: true

    FSelect {
        visible: sf.d.kind === "game"
        Layout.fillWidth: true
        options: [{ name: "Any fullscreen game", value: "any_fullscreen" }, { name: "A specific window", value: "window" }, { name: "Foreground window (hotkey)", value: "hotkey" }]
        currentValue: sf.st.mode || "any_fullscreen"
        onPicked: (v) => app.setSourceSetting(sf.d.id, "mode", v)
    }
    FSelect {
        visible: sf.d.hasDevice === true
        Layout.fillWidth: true
        placeholder: sf.d.kind === "window" || sf.d.kind === "game" ? "Choose window…" : "Choose device…"
        // Listing devices asks Windows for every camera or window (~100 ms), so it's done once per
        // source and again when the menu is opened, not on every edit (like dragging the layer).
        readonly property string listFor: sf.active && sf.d.hasDevice ? sf.d.id : ""
        property var deviceList: []
        function refresh() { deviceList = listFor ? app.devices(listFor) : [] }
        onListForChanged: refresh()
        Component.onCompleted: refresh()
        onDownChanged: if (down) deviceList = listFor ? app.devices(listFor, true) : [] // opening it: ask Windows again
        options: deviceList
        currentValue: sf.st.device
        onPicked: (v) => app.setSourceSetting(sf.d.id, "device", v)
    }
    // A page's sound, right here: volume and mute without a trip to the mixer (once it's on a
    // scene, which is when it's in the mixer).
    RowLayout {
        id: pageAudio
        readonly property var mx: sf.d.kind === "browser" ? (app.mixer.find(r => r.id === sf.d.id) || null) : null
        readonly property bool speakers: sf.st.controlAudio === false // plays straight out, past the mixer
        visible: !!mx
        Layout.fillWidth: true
        spacing: 6
        FButton {
            iconName: pageAudio.mx && pageAudio.mx.muted ? "volume-x" : "volume-2"
            variant: "ghost"
            tip: pageAudio.mx && pageAudio.mx.muted ? "Unmute" : "Mute"
            onClicked: app.setAudio(sf.d.id, "muted", !pageAudio.mx.muted, true)
        }
        Text {
            visible: pageAudio.speakers
            Layout.fillWidth: true
            text: "Plays on your speakers, not in the stream"
            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            elide: Text.ElideRight
        }
        MeterFader {
            visible: !pageAudio.speakers
            Layout.fillWidth: true
            implicitHeight: 14
            value: pageAudio.mx ? AudioMath.posFromLin(pageAudio.mx.volume) : 0
            levels: (pageAudio.mx && app.levels[sf.d.id]) || [-60, -60]
            muted: !!(pageAudio.mx && pageAudio.mx.muted)
            onMoved: (v) => app.setAudio(sf.d.id, "volume", AudioMath.linFromPos(v), false)
            onReleased: (v) => app.setAudio(sf.d.id, "volume", AudioMath.linFromPos(v), true)
        }
        Text {
            visible: !pageAudio.speakers
            text: pageAudio.mx ? AudioMath.toDb(pageAudio.mx.volume) + " dB" : ""
            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
            Layout.preferredWidth: 50; horizontalAlignment: Text.AlignRight
        }
        FButton {
            id: routeButton
            iconName: "ellipsis"; variant: "ghost"; tip: "Where the page's sound goes"
            onClicked: routeMenu.popup(routeButton, 0, routeButton.height + 4)
            FMenu {
                id: routeMenu
                FMenuItem { text: "In the mixer (stream and recording)"; selected: !pageAudio.speakers; onTriggered: app.setSourceSetting(sf.d.id, "controlAudio", true) }
                FMenuItem { text: "Your speakers only"; selected: pageAudio.speakers; onTriggered: app.setSourceSetting(sf.d.id, "controlAudio", false) }
            }
        }
    }
    BrowserOptions {
        visible: sf.d.kind === "browser"
        st: sf.st
        showRefresh: true
        onChanged: (key, value) => app.setSourceSetting(sf.d.id, key, value)
        onRefreshRequested: app.refreshBrowser(sf.d.id)
    }
    // Text: what it says, then font, weight and color.
    FInput {
        visible: sf.d.kind === "text"
        Layout.fillWidth: true
        placeholderText: "Text"
        text: sf.st.text || ""
        onEditingFinished: if (text !== (sf.st.text || "")) app.setSourceSetting(sf.d.id, "text", text)
    }
    RowLayout {
        visible: sf.d.kind === "text"
        Layout.fillWidth: true
        spacing: 4
        FFontPicker {
            Layout.fillWidth: true
            currentFont: sf.st.font || app.defaultTextFont
            onPicked: (f) => app.setSourceSetting(sf.d.id, "font", f)
        }
        FButton {
            text: "B"; font.bold: true
            variant: "ghost"; selected: sf.st.bold !== false // bold unless turned off
            tip: "Bold"
            implicitWidth: Pal.ctl
            onClicked: app.setSourceSetting(sf.d.id, "bold", sf.st.bold === false)
        }
        FButton {
            text: "I"; font.italic: true
            variant: "ghost"; selected: sf.st.italic === true
            tip: "Italic"
            implicitWidth: Pal.ctl
            onClicked: app.setSourceSetting(sf.d.id, "italic", sf.st.italic !== true)
        }
    }
    RowLayout {
        visible: sf.d.kind === "text"
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Color"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 40 }
        FColorField {
            Layout.fillWidth: true
            color: sf.st.textColor || "#ffffff"
            onPicked: (c) => app.setSourceSetting(sf.d.id, "textColor", c)
        }
    }
    // Color layer: its color.
    FColorField {
        visible: sf.d.kind === "color"
        Layout.fillWidth: true
        color: sf.st.color || "#1d1b2e"
        onPicked: (c) => app.setSourceSetting(sf.d.id, "color", c)
    }
    // Shape: fill, outline, corners.
    RowLayout {
        visible: sf.d.kind === "shape" && sf.st.shape !== "arrow" && sf.st.shape !== "path"
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Fill"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
        FColorField { Layout.fillWidth: true; color: sf.st.fill || ""; onPicked: (c) => app.setSourceSetting(sf.d.id, "fill", c) }
        FButton { text: "None"; variant: "ghost"; selected: !sf.st.fill; tip: "No fill (outline only)"; onClicked: app.setSourceSetting(sf.d.id, "fill", "") }
    }
    RowLayout {
        visible: sf.d.kind === "shape"
        Layout.fillWidth: true
        spacing: 6
        Text { text: sf.st.shape === "arrow" || sf.st.shape === "path" ? "Color" : "Stroke"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
        FColorField { Layout.fillWidth: true; color: sf.st.stroke || ""; onPicked: (c) => app.setSourceSetting(sf.d.id, "stroke", c) }
        FButton {
            visible: sf.st.shape !== "arrow" && sf.st.shape !== "path"
            text: "None"; variant: "ghost"; selected: !sf.st.stroke; tip: "No outline"
            onClicked: app.setSourceSetting(sf.d.id, "stroke", "")
        }
    }
    FFillSlider {
        visible: sf.d.kind === "shape"
        Layout.fillWidth: true
        label: sf.st.shape === "arrow" || sf.st.shape === "path" ? "Thickness" : "Stroke width"
        from: 0; to: 48; decimals: 0
        value: sf.st.strokeWidth === undefined ? 6 : sf.st.strokeWidth
        format: (v) => Math.round(v) + " px"
        onReleased: (v) => app.setSourceSetting(sf.d.id, "strokeWidth", Math.round(v))
    }
    // (Rounded corners: the Corners section, which works on every layer.)
    // Image or video file.
    FInput {
        visible: sf.d.kind === "image" || sf.d.kind === "media"
        Layout.fillWidth: true
        placeholderText: "C:\\path\\file"
        text: sf.st.file || ""
        onEditingFinished: if (text !== (sf.st.file || "")) app.setSourceSetting(sf.d.id, "file", text)
    }
    // A plugin's source: its own settings, from OBS.
    ObsProperties {
        visible: String(sf.d.kind || "").startsWith("obs:")
        active: visible && sf.active
        scope: "source"; targetId: visible ? sf.d.id : ""
    }
    // Playback for a video or audio file: play/pause, from the start, stop, seek.
    MediaControls {
        visible: sf.d.kind === "media" && sf.active
        Layout.fillWidth: true
        sourceId: visible ? sf.d.id : ""
    }
}
