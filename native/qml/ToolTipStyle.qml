import QtQuick
import QtQuick.Controls

// Styles and places the one tooltip every control shares (ToolTip.toolTip): a dark card in the
// app font that wraps long text, kept clear of the canvas preview (a native window above all
// QML that would cut it off). It flips to whichever side of its control is free; if none is,
// the preview steps aside while the tooltip shows, as menus do.
Item {
    id: ts
    visible: false
    width: 0; height: 0
    property Item host: null // unused; kept so existing callers stay valid
    property Item previewItem: null
    property ToolTip tip: null // the shared instance, picked up once the window exists
    property bool coversPreview: false

    property Component background: Component {
        Rectangle { radius: Pal.radius; color: Pal.s5; border.width: 1; border.color: Pal.borderStrong }
    }
    property Component content: Component {
        Text {
            text: ts.tip.text
            color: Pal.fg
            font.family: Pal.font
            font.pixelSize: Pal.body
            wrapMode: Text.Wrap
            width: Math.min(implicitWidth, 280)
            lineHeight: 1.1
        }
    }

    function rectOf(item) { const p = item.mapToGlobal(0, 0); return { x: p.x, y: p.y, w: item.width, h: item.height } }
    function hits(a, b) { return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y }

    function place() {
        const t = tip, owner = t ? t.parent : null
        if (!owner || !previewItem || !previewItem.visible) return
        const pv = rectOf(previewItem), o = owner.mapToGlobal(0, 0)
        const w = t.width, h = t.height, gap = 6
        // Candidates relative to the owner: above (default), below, left, right, above-left-aligned.
        const spots = [
            { x: (owner.width - w) / 2, y: -h - gap },
            { x: (owner.width - w) / 2, y: owner.height + gap },
            { x: owner.width - w, y: -h - gap },
            { x: owner.width - w, y: owner.height + gap },
            { x: -w - gap, y: (owner.height - h) / 2 },
            { x: owner.width + gap, y: (owner.height - h) / 2 },
        ]
        for (const s of spots) {
            if (!hits({ x: o.x + s.x, y: o.y + s.y, w: w, h: h }, pv)) {
                t.x = s.x; t.y = s.y
                return
            }
        }
        coversPreview = true
        app.setCovering("tooltip", true)
    }

    property Connections conn: Connections {
        target: ts.tip
        function onAboutToShow() { Qt.callLater(ts.place) }
        function onWidthChanged() { if (ts.tip.visible && !ts.coversPreview) ts.place() }
        // The shared tooltip can hide without aboutToHide (its owner just stops asking), so go by visibility.
        function onVisibleChanged() { if (!ts.tip.visible && ts.coversPreview) { ts.coversPreview = false; app.setCovering("tooltip", false) } }
    }

    function init() {
        tip = ts.ToolTip.toolTip // attached here: the shared instance every control uses
        if (tip) tip.popupType = Popup.Item // in the scene, not an OS window (see FMenu)
        if (!tip) return Qt.callLater(init)
        tip.background = background.createObject(tip)
        tip.contentItem = content.createObject(tip)
        tip.padding = 8
        tip.leftPadding = 10; tip.rightPadding = 10
        tip.font.family = Pal.font
        tip.font.pixelSize = Pal.body
    }
    Component.onCompleted: Qt.callLater(init)
}
