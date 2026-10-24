import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Color field: a swatch with its hex. Opens LOBS's own picker: a saturation/brightness square,
// a hue strip, a hex box, presets and recent colors. The popup stays inside the window and opens
// on whichever side of the field has room.
FButton {
    id: cf
    Accessible.role: Accessible.Button
    Accessible.name: "Color " + color
    property string color: "#ffffff"   // #rrggbb
    signal picked(string color)
    readonly property bool valid: /^#[0-9a-fA-F]{6}$/.test(color)
    variant: "tertiary"
    pressDip: false
    leftPadding: 6; rightPadding: 10
    implicitWidth: 120
    contentItem: Row {
        spacing: 8
        Rectangle {
            width: 18; height: 18; radius: 4
            anchors.verticalCenter: parent.verticalCenter
            color: cf.valid ? cf.color : "transparent"
            border.width: 1; border.color: Pal.borderStrong
        }
        Text { text: cf.valid ? cf.color.toUpperCase() : "Color"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; anchors.verticalCenter: parent.verticalCenter }
    }
    onClicked: picker().opened ? picker().close() : picker().open()
    // The picker is built the first time it opens, not with the field: Inspector filter cards
    // make and drop many fields, and Qt kept ~28 KB of each unopened picker (LOBS-039).
    function picker() { pickerLoader.active = true; return pickerLoader.item.popup }
    Loader { id: pickerLoader; anchors.fill: parent; active: false; sourceComponent: pickerComponent }

    // Swatches: the show's saved colors (its palette), stored in the show file and shared by every
    // color field. + saves the current color; the x on hover (or right-click) removes one. Undoable.
    function saveSwatches(list) { app.setSwatches(list) }
    property bool waitingForScreen: false
    Connections {
        target: app
        function onScreenColorPicked(hex) { if (!cf.waitingForScreen) return; cf.waitingForScreen = false; cf.choose(hex); cf.picker().open() }
        function onScreenColorCanceled() { if (cf.waitingForScreen) { cf.waitingForScreen = false; cf.picker().open() } }
    }
    function choose(c) { cf.picked(c.toLowerCase()); remember(c.toLowerCase()) }
    function remember(c) {
        const r = (app.uiValue("colors/recent", "") || "").split(",").filter(x => /^#[0-9a-f]{6}$/.test(x) && x !== c)
        r.unshift(c)
        app.setUiValue("colors/recent", r.slice(0, 6).join(","))
        if (pickerLoader.item) pickerLoader.item.popup.recent = r.slice(0, 6)
    }

    Component {
        id: pickerComponent
        // Fills the field, so the popup's parent (and its press-outside-parent close) is the field.
        Item {
            property alias popup: pop
            Popup {
                id: pop
                // Below the field when it fits, else above. margins keep it inside the window either way.
                readonly property real roomBelow: cf.Window.window ? cf.Window.window.height - cf.mapToItem(null, 0, cf.height).y : 1000
                y: roomBelow > implicitHeight + 12 ? cf.height + 4 : -implicitHeight - 4
                margins: 8
                width: 244
                padding: 12
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                property var recent: []
                readonly property var swatches: app.swatches
                property bool coversPreview: false
                // Working color while the popup is open, as hue/saturation/value (0..1).
                property real h: 0
                property real s: 0
                property real v: 1
                readonly property string current: hsvHex(h, s, v)
                function hsvHex(h, s, v) {
                    const c = Qt.hsva(h, s, v, 1)
                    const hx = (x) => ("0" + Math.round(x * 255).toString(16)).slice(-2)
                    return "#" + hx(c.r) + hx(c.g) + hx(c.b)
                }
                function load(hex) {
                    const c = Qt.color(/^#[0-9a-fA-F]{6}$/.test(hex) ? hex : "#ffffff")
                    if (c.hsvSaturation > 0 && c.hsvHue >= 0) h = c.hsvHue
                    s = c.hsvSaturation; v = c.hsvValue
                }
                // Applied when you let go, so one drag is one undo step.
                function commit() { cf.picked(current); hex.text = current.toUpperCase() }
                function overlapsPreview() {
                    const w = cf.Window.window, p = w ? w.previewItem : null
                    if (!p || !p.visible) return false
                    const a = p.mapToGlobal(0, 0), b = cf.mapToGlobal(x, y)
                    return b.x < a.x + p.width && b.x + width > a.x && b.y < a.y + p.height && b.y + height > a.y
                }
                onAboutToShow: {
                    load(cf.color)
                    hex.text = cf.valid ? cf.color.toUpperCase() : ""
                    recent = (app.uiValue("colors/recent", "") || "").split(",").filter(x => /^#[0-9a-f]{6}$/.test(x))
                    coversPreview = overlapsPreview(); if (coversPreview) app.popupOpened()
                }
                onAboutToHide: {
                    if (cf.valid && cf.color.toLowerCase() === current) cf.remember(current)
                    if (coversPreview) { coversPreview = false; app.popupClosed() }
                }
                enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Pal.fast } }
                exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Pal.fast } }
                background: Rectangle { radius: Pal.radius + 4; color: Pal.s4; border.width: 1; border.color: Pal.border }

                component Swatch: Rectangle {
                    id: sw
                    required property string modelData
                    property bool removable: false
                    readonly property bool on: modelData === cf.color.toLowerCase()
                    width: 30; height: 22; radius: 5
                    color: modelData
                    border.width: on ? 2 : 1
                    border.color: on ? Pal.fg : Qt.rgba(1, 1, 1, 0.14)
                    scale: sm.pressed ? 0.94 : sm.containsMouse ? 1.06 : 1
                    Behavior on scale { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
                    MouseArea {
                        id: sm; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: (m) => {
                            if (m.button === Qt.RightButton) { if (sw.removable) cf.saveSwatches(pop.swatches.filter(x => x !== sw.modelData)); return }
                            pop.load(sw.modelData); cf.choose(sw.modelData); pop.close()
                        }
                    }
                    // Remove (saved swatches only), on hover.
                    Rectangle {
                        visible: sw.removable && (sm.containsMouse || xm.containsMouse)
                        width: 14; height: 14; radius: 7
                        x: parent.width - 9; y: -5
                        color: Pal.s6; border.width: 1; border.color: Pal.borderStrong
                        FIcon { anchors.centerIn: parent; name: "x"; size: 9; color: Pal.fg }
                        MouseArea { id: xm; anchors.fill: parent; anchors.margins: -3; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: cf.saveSwatches(pop.swatches.filter(x => x !== sw.modelData)) }
                    }
                }

                contentItem: ColumnLayout {
                    spacing: 10
                    // Saturation (x) and brightness (y) for the current hue.
                    Rectangle {
                        id: sv
                        Layout.fillWidth: true
                        implicitHeight: 140
                        radius: Pal.radius
                        color: Qt.hsva(pop.h, 1, 1, 1)
                        clip: true
                        Rectangle {
                            anchors.fill: parent; radius: parent.radius
                            gradient: Gradient { orientation: Gradient.Horizontal; GradientStop { position: 0; color: "#ffffff" } GradientStop { position: 1; color: "#00ffffff" } }
                        }
                        Rectangle {
                            anchors.fill: parent; radius: parent.radius
                            gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#000000" } }
                        }
                        Rectangle { anchors.fill: parent; radius: parent.radius; color: "transparent"; border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.1) }
                        Rectangle {
                            width: 14; height: 14; radius: 7
                            x: pop.s * sv.width - 7; y: (1 - pop.v) * sv.height - 7
                            color: pop.current
                            border.width: 2; border.color: "#ffffff"
                            Rectangle { anchors.fill: parent; anchors.margins: -1; radius: 8; color: "transparent"; border.width: 1; border.color: Qt.rgba(0, 0, 0, 0.35) }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.CrossCursor
                            function at(m) { pop.s = Math.max(0, Math.min(1, m.x / width)); pop.v = Math.max(0, Math.min(1, 1 - m.y / height)); hex.text = pop.current.toUpperCase() }
                            onReleased: pop.commit()
                            onPressed: (m) => at(m)
                            onPositionChanged: (m) => { if (pressed) at(m) }
                        }
                    }
                    // Hue.
                    Item {
                        id: hue
                        Layout.fillWidth: true
                        implicitHeight: 14
                        Rectangle {
                            anchors.fill: parent; radius: 7
                            gradient: Gradient {
                                orientation: Gradient.Horizontal
                                GradientStop { position: 0.0; color: "#ff0000" }
                                GradientStop { position: 0.167; color: "#ffff00" }
                                GradientStop { position: 0.333; color: "#00ff00" }
                                GradientStop { position: 0.5; color: "#00ffff" }
                                GradientStop { position: 0.667; color: "#0000ff" }
                                GradientStop { position: 0.833; color: "#ff00ff" }
                                GradientStop { position: 1.0; color: "#ff0000" }
                            }
                        }
                        Rectangle {
                            width: 16; height: 16; radius: 8
                            x: pop.h * (hue.width - 14) - 1; y: -1
                            color: Qt.hsva(pop.h, 1, 1, 1)
                            border.width: 2; border.color: "#ffffff"
                        }
                        MouseArea {
                            anchors.fill: parent; anchors.margins: -4
                            cursorShape: Qt.PointingHandCursor
                            function at(m) { pop.h = Math.max(0, Math.min(0.999, (m.x - 4) / hue.width)); if (pop.s === 0) pop.s = 1; if (pop.v === 0) pop.v = 1; hex.text = pop.current.toUpperCase() }
                            onReleased: pop.commit()
                            onPressed: (m) => at(m)
                            onPositionChanged: (m) => { if (pressed) at(m) }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Rectangle { implicitWidth: 28; implicitHeight: 28; radius: Pal.radius; color: pop.current; border.width: 1; border.color: Qt.rgba(1, 1, 1, 0.14) }
                        FButton {
                            iconName: "pipette"; variant: "ghost"
                            tip: "Pick from screen (Esc cancels)"
                            onClicked: { cf.waitingForScreen = true; pop.close(); Qt.callLater(app.pickScreenColor) }
                        }
                        FInput {
                            id: hex
                            Layout.fillWidth: true
                            placeholderText: "#RRGGBB"
                            font.family: Pal.mono
                            onAccepted: {
                                const t = text.trim(); const val = t.startsWith("#") ? t : "#" + t
                                if (/^#[0-9a-fA-F]{6}$/.test(val)) { pop.load(val); cf.choose(val); pop.close() }
                            }
                        }
                    }
                    Text { text: "Swatches"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                    Grid {
                        columns: 6; spacing: 6
                        Repeater { model: pop.swatches; delegate: Swatch { removable: true } }
                        // Save the current color as a swatch.
                        Rectangle {
                            readonly property bool saved: pop.swatches.indexOf(pop.current) >= 0
                            width: 30; height: 22; radius: 5
                            color: addMouse.containsMouse && !saved ? Pal.s6 : "transparent"
                            border.width: 1; border.color: Pal.borderStrong
                            opacity: saved ? 0.4 : 1
                            FIcon { anchors.centerIn: parent; name: "plus"; size: 12; color: Pal.fg }
                            MouseArea {
                                id: addMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: parent.saved ? Qt.ArrowCursor : Qt.PointingHandCursor
                                onClicked: if (!parent.saved) cf.saveSwatches(pop.swatches.concat([pop.current]))
                            }
                            ToolTip.visible: addMouse.containsMouse
                            ToolTip.delay: 400
                            ToolTip.text: saved ? "Already a swatch" : "Save " + pop.current.toUpperCase() + " as a swatch. Right-click a swatch to remove it."
                        }
                    }
                    ColumnLayout {
                        visible: pop.recent.length > 0
                        spacing: 6
                        Text { text: "Recent"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
                        Row { spacing: 6; Repeater { model: pop.recent; delegate: Swatch {} } }
                    }
                }
            }
        }
    }
}
