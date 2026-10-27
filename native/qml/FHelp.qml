import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// (?) button that opens a short help card on hover or click: a title and a few key points
// (what the panel does, the shortcuts that matter). Keeps panels free of filler text.
FButton {
    id: h
    property string title
    property var points: [] // strings; "Key: text" renders the key in semibold
    iconName: "circle-help"
    variant: "ghost"
    onHoveredChanged: if (hovered) openTimer.restart(); else openTimer.stop()
    onClicked: card.opened ? card.close() : card.open()
    Timer { id: openTimer; interval: 350; onTriggered: card.open() }

    Popup {
        popupType: Popup.Item // in the scene, not an OS window (see FMenu)
        id: card
        y: h.height + 4
        x: Math.min(0, (h.Window.window ? h.Window.window.width : 9999) - h.mapToItem(null, 0, 0).x - width - 8)
        width: 300
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        // The engine preview is a native window above all QML; step it aside while open.
        property bool coversPreview: false
        // True if this popup's rect overlaps the canvas preview (a native window above all QML).
        function overlapsPreview(popup) {
            if (app.previewInScene) return false
            const w = popup.parent ? popup.parent.Window.window : null
            const p = w ? w.previewItem : null
            if (!p || !p.visible || !popup.parent) return false
            const a = p.mapToGlobal(0, 0), b = popup.parent.mapToGlobal(popup.x, popup.y)
            return b.x < a.x + p.width && b.x + popup.width > a.x && b.y < a.y + p.height && b.y + popup.height > a.y
        }

        onAboutToShow: { coversPreview = overlapsPreview(card); if (coversPreview) app.popupOpened() }
        onAboutToHide: if (coversPreview) { coversPreview = false; app.popupClosed() }
        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Pal.moderate }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Pal.moderate; easing.type: Easing.OutCubic }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Pal.fast } }
        background: Rectangle { radius: Pal.radius + 4; color: Pal.s4; border.width: 1; border.color: Pal.border }
        // Close when the pointer leaves both the button and the card.
        Timer { id: closeTimer; interval: 250; onTriggered: if (!h.hovered && !cardHover.hovered) card.close() }
        Connections { target: h; function onHoveredChanged() { if (!h.hovered) closeTimer.restart() } }
        contentItem: ColumnLayout {
            spacing: 8
            HoverHandler { id: cardHover; onHoveredChanged: if (!hovered) closeTimer.restart() }
            Text { text: h.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Repeater {
                model: h.points
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { width: 4; height: 4; radius: 2; color: Pal.muted; Layout.alignment: Qt.AlignTop; Layout.topMargin: 7 }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        textFormat: Text.StyledText
                        color: Pal.muted
                        font.family: Pal.font
                        font.pixelSize: Pal.body
                        lineHeight: 1.15
                        readonly property int colon: modelData.indexOf(": ")
                        text: colon > 0 ? "<font color='" + Pal.fg + "'><b>" + modelData.slice(0, colon) + "</b></font> " + modelData.slice(colon + 2) : modelData
                    }
                }
            }
        }
    }
}
