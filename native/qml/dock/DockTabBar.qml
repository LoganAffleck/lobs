import QtQuick
import QtQuick.Controls
import com.kdab.dockwidgets 2.0
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
import LOBS

// Panel tab strip (KDDockWidgets TabBarBase, styled as Fluid Functionalism tabs).
// Each tab has a grip: drag it to move the panel (a blue preview shows where it lands),
// drop it on another panel to tab them together, drag along the strip to reorder,
// double-click to float it. Close appears on hover.
KDDW.TabBarBase {
    id: root
    Accessible.role: Accessible.PageTabList
    Accessible.name: "Panel tabs"
    implicitHeight: 36

    function getTabAtIndex(index) { return tabRow.children[index] }
    function getTabIndexAtPosition(globalPoint) {
        for (let i = 0; i < tabRow.children.length; ++i) {
            const tab = tabRow.children[i]
            if (tab.tabIndex === undefined) continue
            if (tab.contains(tab.mapFromGlobal(globalPoint.x, globalPoint.y))) return i
        }
        return -1
    }

    HoverHandler { id: stripHover }

    // The drag controller asks where a dragged tab would go among these (tabSlots: in this bar's
    // coordinates, in order).
    function tabSlots() {
        const out = []
        for (let i = 0; i < tabRow.children.length; ++i) {
            const t = tabRow.children[i]
            if (t.tabIndex === undefined) continue
            out.push({ title: t.title, x: tabRow.x + t.x, w: t.width })
        }
        return out.sort((a, b) => a.x - b.x)
    }
    onInMainWindowChanged: if (inMainWindow) mainWin.panelDrag.registerTabBar(root)
    Component.onCompleted: if (inMainWindow) mainWin.panelDrag.registerTabBar(root)
    Component.onDestruction: if (inMainWindow) mainWin.panelDrag.unregisterTabBar(root)

    // Docked panels: LOBS handles press/drag/click here (the panel keeps its slot while you drag;
    // see PanelDragController). Floating windows fall through to KDDockWidgets' own dragging.
    readonly property var mainWin: Window.window
    readonly property bool inMainWindow: !!mainWin && mainWin.panelDrag !== undefined // floating windows have no drag controller
    MouseArea {
        id: tabMouse
        Accessible.role: Accessible.PageTabList
        Accessible.name: "Panel tabs"
        z: root.mouseAreaZ.z + 2
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: tabRow.x + tabRow.width
        enabled: root.inMainWindow
        hoverEnabled: false
        property point pressAt
        property int pressIndex: -1
        property bool dragging: false
        function indexAt(mx, my) {
            const g = mapToGlobal(mx, my)
            return root.getTabIndexAtPosition(g)
        }
        onPressed: (m) => { pressAt = Qt.point(m.x, m.y); pressIndex = indexAt(m.x, m.y); dragging = false }
        onPositionChanged: (m) => {
            if (!pressed || pressIndex < 0) return
            const g = mapToGlobal(m.x, m.y)
            if (!dragging && Math.hypot(m.x - pressAt.x, m.y - pressAt.y) > 6) {
                const tab = root.getTabAtIndex(pressIndex)
                dragging = tab && root.mainWin.panelDrag.begin(tab.title, g.x, g.y)
            }
            if (dragging) root.mainWin.panelDrag.move(g.x, g.y)
        }
        onReleased: (m) => {
            if (dragging) {
                const g = mapToGlobal(m.x, m.y)
                dragging = false
                root.mainWin.panelDrag.end(g.x, g.y)
            } else if (pressIndex >= 0) {
                const tab = root.getTabAtIndex(pressIndex)
                if (tab && tab.isCurrent && tab.helpAt(mapToGlobal(m.x, m.y))) tab.openHelp()
                else root.currentTabIndex = pressIndex
            }
        }
        onCanceled: if (dragging) { dragging = false; root.mainWin.panelDrag.cancel() }
        onDoubleClicked: if (root.groupCpp) root.groupCpp.titleBar.onFloatClicked()
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Pal.border }

    Row {
        id: tabRow
        z: root.mouseAreaZ.z + 1
        x: 6
        height: parent.height
        spacing: 2
        property int hoveredIndex: -1
        Repeater {
            model: root.groupCpp ? root.groupCpp.tabBar.dockWidgetModel : 0
            Item {
                id: tab
                required property int index
                required property string title
                readonly property int tabIndex: index
                readonly property bool isCurrent: root.groupCpp && index === root.groupCpp.currentIndex
                readonly property bool isHovered: tabRow.hoveredIndex === index
                height: tabRow.height
                width: row.implicitWidth + 20
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 5
                    anchors.bottomMargin: 5
                    radius: Pal.radius - 2
                    color: tab.isCurrent ? Pal.s5 : Pal.hover
                    opacity: tab.isCurrent || tab.isHovered ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Pal.fast } }
                }
                Row {
                    id: row
                    anchors.centerIn: parent
                    spacing: 4
                    FIcon {
                        name: "grip-vertical"
                        size: 12
                        color: Pal.muted
                        opacity: tab.isHovered ? 1 : (tab.isCurrent ? 0.55 : 0.35)
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on opacity { NumberAnimation { duration: Pal.fast } }
                    }
                    Text {
                        text: tab.title
                        color: tab.isCurrent ? Pal.fg : Pal.muted
                        font.family: Pal.font
                        font.pixelSize: Pal.body
                        font.weight: tab.isCurrent ? 550 : 400
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    // Help for this tab's panel: only on the active tab, inside it.
                    FHelp {
                        id: tabHelp
                        readonly property var entry: tab.isCurrent && root.inMainWindow ? root.mainWin.panelDrag.entryForTitle(tab.title) : null
                        visible: entry !== null && entry.help !== undefined
                        implicitWidth: 20; implicitHeight: 20
                        anchors.verticalCenter: parent.verticalCenter
                        title: tab.title
                        points: entry && entry.help ? entry.help : []
                    }
                }
                function helpAt(globalPoint) { // the strip's MouseArea forwards clicks on the (?)
                    if (!tabHelp.visible) return false
                    const p = tabHelp.mapFromGlobal(globalPoint.x, globalPoint.y)
                    return tabHelp.contains(p)
                }
                function openHelp() { tabHelp.clicked() }
                ToolTip.visible: tab.isHovered && !Singletons.helpers.isDragging && !(root.mainWin && root.mainWin.panelDrag && root.mainWin.panelDrag.active)
                ToolTip.delay: 900
                ToolTip.text: "Drag to move · double-click to float"
            }
        }
        Connections {
            target: root.tabBarCpp
            function onHoveredTabIndexChanged(index) { tabRow.hoveredIndex = index }
        }
    }

    FButton {
        id: closeButton
        z: root.mouseAreaZ.z + 1
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        iconName: "x"
        variant: "ghost"
        opacity: stripHover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Pal.fast } }
        tip: "Close panel"
        onClicked: if (root.tabBarCpp && root.groupCpp) root.tabBarCpp.closeAtIndex(root.groupCpp.currentIndex)
    }
}
