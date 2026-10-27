import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Font picker: a field showing the font in its own face; opens a searchable list of the
// installed families, each previewed in itself. Up/Down move, Enter picks, Esc closes.
FButton {
    id: fp
    property string currentFont: ""
    signal picked(string family)
    variant: "tertiary"
    pressDip: false
    leftPadding: 10; rightPadding: 8
    text: currentFont || "Choose font…"
    contentItem: Item {
        implicitWidth: 140
        clip: true
        Text {
            readonly property real fit: Math.min(1, 18 / Math.max(1, contentHeight)) // see the list below
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
            width: (parent.width - chev.width - 6) / fit
            scale: fit; transformOrigin: Item.Left
            text: fp.text; elide: Text.ElideRight
            color: fp.currentFont ? Pal.fg : Pal.muted
            font.family: fp.currentFont || Pal.font; font.pixelSize: Pal.body + 1
        }
        Text { id: chev; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: "▾"; color: Pal.muted; font.pixelSize: 10 }
    }
    onClicked: pop.opened ? pop.close() : pop.open()
    // Picking remembers the font: it tops the list next time, and new text uses it.
    function pick(family) {
        const r = (app.uiValue("fonts/recent", "") || "").split("|").filter(x => x && x !== family)
        r.unshift(family)
        app.setUiValue("fonts/recent", r.slice(0, 6).join("|"))
        app.toolFont = family
        fp.picked(family)
    }

    Popup {
        popupType: Popup.Item // in the scene, not an OS window (see FMenu)
        id: pop
        y: fp.height + 4
        width: Math.max(fp.width, 280)
        height: 360
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        readonly property var families: Qt.fontFamilies().filter(f => !f.startsWith("@"))
        property var shown: families
        property int recentCount: 0
        function refilter() {
            const q = search.text.trim().toLowerCase()
            if (q) {
                recentCount = 0
                shown = families.filter(f => f.toLowerCase().includes(q))
            } else {
                // Recently used first (ones still installed), then every font.
                const r = (app.uiValue("fonts/recent", "") || "").split("|").filter(x => x && families.indexOf(x) >= 0)
                recentCount = r.length
                shown = r.concat(families)
            }
            list.currentIndex = Math.max(0, shown.indexOf(fp.currentFont))
        }
        property bool coversPreview: false
        function overlapsPreview() {
            if (app.previewInScene) return false
            const w = fp.Window.window, p = w ? w.previewItem : null
            if (!p || !p.visible) return false
            const a = p.mapToGlobal(0, 0), b = fp.mapToGlobal(x, y)
            return b.x < a.x + p.width && b.x + width > a.x && b.y < a.y + p.height && b.y + height > a.y
        }
        onAboutToShow: { search.text = ""; refilter(); coversPreview = overlapsPreview(); if (coversPreview) app.popupOpened() }
        onOpened: { search.forceActiveFocus(); list.positionViewAtIndex(list.currentIndex, ListView.Center) }
        onAboutToHide: if (coversPreview) { coversPreview = false; app.popupClosed() }
        background: Rectangle { radius: Pal.radius + 2; color: Pal.s4; border.width: 1; border.color: Pal.border }
        contentItem: ColumnLayout {
            spacing: 6
            FInput {
                id: search
                Layout.fillWidth: true
                placeholderText: "Search fonts"
                onTextChanged: pop.refilter()
                Keys.onDownPressed: list.incrementCurrentIndex()
                Keys.onUpPressed: list.decrementCurrentIndex()
                Keys.onReturnPressed: if (pop.shown.length) { fp.pick(pop.shown[list.currentIndex]); pop.close() }
                Keys.onEnterPressed: if (pop.shown.length) { fp.pick(pop.shown[list.currentIndex]); pop.close() }
            }
            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: pop.shown
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; Accessible.name: "Scroll" }
                delegate: Item {
                  id: row
                  required property string modelData
                  required property int index
                  // Group labels above the recent fonts and above the full list.
                  readonly property string heading: pop.recentCount === 0 ? "" : index === 0 ? "Recently used" : index === pop.recentCount ? "All fonts" : ""
                  width: ListView.view.width - 8
                  height: 30 + (heading ? 24 : 0)
                  Text {
                    visible: row.heading !== ""
                    text: row.heading
                    x: 10; y: 6
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                  }
                  Rectangle {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                    height: 30
                    readonly property string modelData: row.modelData
                    readonly property int index: row.index
                    radius: Pal.radius - 2
                    color: index === list.currentIndex ? Pal.selected : rowMouse.containsMouse ? Pal.hover : "transparent"
                    clip: true
                    Text {
                        // Some fonts (old vector fonts like Roman) ignore the pixel size and draw
                        // huge; shrink any that overflow the row so they stay in it.
                        readonly property real fit: Math.min(1, 22 / Math.max(1, contentHeight))
                        anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                        width: (parent.width - 18) / fit
                        scale: fit; transformOrigin: Item.Left
                        text: modelData; elide: Text.ElideRight
                        color: Pal.fg; font.family: modelData; font.pixelSize: 14
                    }
                    MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { fp.pick(modelData); pop.close() } }
                  }
                }
                Text {
                    visible: pop.shown.length === 0
                    anchors.centerIn: parent
                    text: "No fonts match"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                }
            }
        }
    }
}
