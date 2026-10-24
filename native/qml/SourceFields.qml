import QtQuick
import QtQuick.Layouts

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
    RowLayout {
        visible: sf.d.kind === "browser"
        Layout.fillWidth: true
        spacing: 6
        FInput {
            Layout.fillWidth: true
            placeholderText: "https://"
            text: sf.st.url || ""
            onEditingFinished: if (text !== (sf.st.url || "")) app.setSourceSetting(sf.d.id, "url", text)
        }
        FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "Reload the page"; onClicked: app.refreshBrowser(sf.d.id) }
    }
    RowLayout {
        visible: sf.d.kind === "browser"
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Page size"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
        Item { Layout.fillWidth: true }
        FInput {
            Layout.preferredWidth: 72
            placeholderText: "Canvas"
            text: sf.st.width ? String(sf.st.width) : ""
            onEditingFinished: app.setSourceSetting(sf.d.id, "width", parseInt(text) || undefined)
        }
        Text { text: "×"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
        FInput {
            Layout.preferredWidth: 72
            placeholderText: "Canvas"
            text: sf.st.height ? String(sf.st.height) : ""
            onEditingFinished: app.setSourceSetting(sf.d.id, "height", parseInt(text) || undefined)
        }
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
    // Playback for a video or audio file: play/pause, from the start, stop, seek.
    MediaControls {
        visible: sf.d.kind === "media" && sf.active
        Layout.fillWidth: true
        sourceId: visible ? sf.d.id : ""
    }
}
