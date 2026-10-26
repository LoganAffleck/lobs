import QtQuick
import LOBS
import com.kdab.dockwidgets 2.0 as KDDW

// LOBS's own panel drag for docked panels. Unlike a tear-off, nothing moves until you let go:
// the panel keeps its slot (dimmed, "Moving…"), a chip follows the cursor, and a blue zone shows
// where it will land — half of the hovered panel near its edges, the whole panel near its middle
// (tab them together), or a full side strip near the window's outer edges.
Item {
    id: ctl
    anchors.fill: parent
    z: 900
    visible: active

    required property Item dockArea
    property var panels: []          // [{ dock, guest, icon }]
    property var dragged: null       // entry from panels
    property bool active: false
    property var target: null        // { kind: "side"|"tab"|"outer", entry, location, rect }

    readonly property int tabStrip: 36
    // Tab strips in the main window (they register themselves): over one, a drop goes between
    // its tabs, at the exact place, rather than at the end of the group.
    property var tabBars: []
    function registerTabBar(b) { if (tabBars.indexOf(b) < 0) tabBars = tabBars.concat([b]) }
    function unregisterTabBar(b) { tabBars = tabBars.filter(x => x !== b) }
    function tabTarget(x, y) {
        for (const bar of tabBars) {
            if (!bar || !bar.visible || bar.width < 2) continue
            const o = bar.mapToItem(ctl, 0, 0)
            if (x < o.x || x > o.x + bar.width || y < o.y || y > o.y + bar.height) continue
            const slots = bar.tabSlots()
            if (!slots.length) return null
            const titles = slots.map(s => s.title)
            // Only the dragged panel here: nowhere else to put it.
            if (titles.length === 1 && dragged && titles[0] === dragged.dock.title) return null
            let index = slots.length
            for (let i = 0; i < slots.length; ++i)
                if (x - o.x < slots[i].x + slots[i].w / 2) { index = i; break }
            const lx = index < slots.length ? slots[index].x - 2 : slots[slots.length - 1].x + slots[slots.length - 1].w + 1
            return { kind: "tabAt", titles: titles, index: index, rect: Qt.rect(o.x + lx, o.y + 6, 3, bar.height - 12) }
        }
        return null
    }
    readonly property int edgeBand: 36

    function entryForTitle(title) {
        for (const p of panels) if (p.dock.title === title) return p
        return null
    }
    // A panel's visual rect (its tab strip + body) in this overlay's coordinates.
    function rectOf(p) {
        const g = p.guest
        if (!g || !g.visible || g.width < 2) return null
        const tl = g.mapToItem(ctl, 0, 0)
        return Qt.rect(tl.x - 1, tl.y - tabStrip - 1, g.width + 2, g.height + tabStrip + 2)
    }
    function inside(r, x, y) { return r && x >= r.x && y >= r.y && x <= r.x + r.width && y <= r.y + r.height }

    function begin(title, gx, gy) {
        const p = entryForTitle(title)
        if (!p) return false
        dragged = p
        chipIcon.name = p.icon
        chipText.text = p.dock.title
        active = true
        app.popupOpened() // the native preview would cover the overlay
        move(gx, gy)
        return true
    }

    function move(gx, gy) {
        if (!active) return
        const pt = ctl.mapFromGlobal(gx, gy)
        chip.x = pt.x + 14
        chip.y = pt.y + 10
        target = computeTarget(pt.x, pt.y)
        const own = rectOf(dragged)
        slot.visible = own !== null
        if (own) { slot.x = own.x; slot.y = own.y; slot.width = own.width; slot.height = own.height }
    }

    function computeTarget(x, y) {
        const area = dockArea.mapToItem(ctl, 0, 0)
        const A = Qt.rect(area.x, area.y, dockArea.width, dockArea.height)
        if (!inside(A, x, y)) return null
        const onTabs = tabTarget(x, y)
        if (onTabs) return onTabs
        // Outer edges of the whole layout: dock along that full side.
        const third = 0.28
        if (x - A.x < edgeBand) return { kind: "outer", location: KDDW.KDDockWidgets.Location_OnLeft, rect: Qt.rect(A.x, A.y, A.width * third, A.height) }
        if (A.x + A.width - x < edgeBand) return { kind: "outer", location: KDDW.KDDockWidgets.Location_OnRight, rect: Qt.rect(A.x + A.width * (1 - third), A.y, A.width * third, A.height) }
        if (A.y + A.height - y < edgeBand) return { kind: "outer", location: KDDW.KDDockWidgets.Location_OnBottom, rect: Qt.rect(A.x, A.y + A.height * (1 - third), A.width, A.height * third) }
        if (y - A.y < edgeBand) return { kind: "outer", location: KDDW.KDDockWidgets.Location_OnTop, rect: Qt.rect(A.x, A.y, A.width, A.height * third) }
        for (const p of panels) {
            if (p === dragged || !p.dock.isOpen) continue
            const r = rectOf(p)
            if (!inside(r, x, y)) continue
            const fx = (x - r.x) / r.width, fy = (y - r.y) / r.height
            // Middle (or anywhere on the tab strip) = tab together; otherwise the nearest side.
            if ((fx > 0.3 && fx < 0.7 && fy > 0.3 && fy < 0.7) || y - r.y < tabStrip)
                return { kind: "tab", entry: p, rect: r }
            const dl = fx, dr = 1 - fx, dt = fy, db = 1 - fy, m = Math.min(dl, dr, dt, db)
            if (m === dl) return { kind: "side", entry: p, location: KDDW.KDDockWidgets.Location_OnLeft, rect: Qt.rect(r.x, r.y, r.width / 2, r.height) }
            if (m === dr) return { kind: "side", entry: p, location: KDDW.KDDockWidgets.Location_OnRight, rect: Qt.rect(r.x + r.width / 2, r.y, r.width / 2, r.height) }
            if (m === dt) return { kind: "side", entry: p, location: KDDW.KDDockWidgets.Location_OnTop, rect: Qt.rect(r.x, r.y, r.width, r.height / 2) }
            return { kind: "side", entry: p, location: KDDW.KDDockWidgets.Location_OnBottom, rect: Qt.rect(r.x, r.y + r.height / 2, r.width, r.height / 2) }
        }
        return null
    }

    function end(gx, gy) {
        if (!active) return
        move(gx, gy)
        const t = target, p = dragged
        finish()
        if (!t || !p) return
        const w = t.rect.width, h = t.rect.height
        if (t.kind === "tabAt") {
            const name = p.dock.uniqueName
            const from = t.titles.indexOf(p.dock.title)
            if (from >= 0) { // same group: from where it was to the gap it was dropped in
                dockTabs.move(name, t.index > from ? t.index - 1 : t.index)
            } else {
                const host = t.titles.map(entryForTitle).find(e => !!e) // a panel of that group LOBS knows
                if (!host) return
                host.dock.addDockWidgetAsTab(p.dock)
                dockTabs.move(name, t.index)
            }
        } else if (t.kind === "tab") {
            t.entry.dock.addDockWidgetAsTab(p.dock)
        } else if (t.kind === "side") {
            dockArea.addDockWidget(p.dock, t.location, t.entry.dock, Qt.size(w, h))
        } else {
            dockArea.addDockWidget(p.dock, t.location, null, Qt.size(w, h))
        }
        p.dock.dockWidget.setAsCurrentTab()
    }

    function cancel() { finish() }
    function finish() {
        if (!active) return
        active = false
        target = null
        dragged = null
        app.popupClosed()
    }

    Keys.onEscapePressed: cancel()
    focus: active

    // The panel's own slot stays put, dimmed, until the drop.
    Rectangle {
        id: slot
        radius: Pal.radius + 2
        color: "#99171717"
        border.width: 1
        border.color: Pal.borderStrong
        Text {
            anchors.centerIn: parent
            text: "Moving " + (ctl.dragged ? ctl.dragged.dock.title : "") + "…"
            color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
        }
    }

    // Where it will land.
    Rectangle {
        id: zone
        visible: ctl.target !== null
        x: ctl.target ? ctl.target.rect.x : 0
        y: ctl.target ? ctl.target.rect.y : 0
        width: ctl.target ? ctl.target.rect.width : 0
        height: ctl.target ? ctl.target.rect.height : 0
        radius: Pal.radius + 2
        color: "#336b97ff"
        border.width: 2
        border.color: Pal.focus
        Behavior on x { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on y { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on width { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Behavior on height { NumberAnimation { duration: Pal.fast; easing.type: Easing.OutCubic } }
        Rectangle {
            visible: ctl.target && ctl.target.kind === "tab"
            anchors.centerIn: parent
            radius: Pal.radius
            color: Pal.focus
            width: tabLabel.implicitWidth + 20
            height: Pal.ctl
            Text { id: tabLabel; anchors.centerIn: parent; text: "Add as tab"; color: "white"; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
        }
    }

    // Chip that follows the cursor.
    Rectangle {
        id: chip
        radius: Pal.radius
        color: Pal.s5
        border.width: 1
        border.color: Pal.borderStrong
        width: chipRow.implicitWidth + 20
        height: Pal.ctl
        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 6
            FIcon { id: chipIcon; anchors.verticalCenter: parent.verticalCenter }
            Text { id: chipText; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
