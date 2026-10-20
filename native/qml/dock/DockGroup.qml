import QtQuick
import com.kdab.dockwidgets 2.0
import LOBS

// A panel group: rounded surface-2 card with the tab strip on top (or a title bar when floating).
// Adapted from KDDockWidgets' Group.qml (GPL-2.0-only OR GPL-3.0-only), MDI parts removed.
Rectangle {
    id: root
    property GroupView groupCpp
    readonly property QtObject titleBarCpp: groupCpp ? groupCpp.titleBar : null
    readonly property int nonContentsHeight: (titleBar.item ? titleBar.item.heightWhenVisible : 0) + tabbar.implicitHeight + (2 * contentsMargin) + titleBarContentsMargin
    property int contentsMargin: 1
    property int titleBarContentsMargin: 0
    readonly property bool hasCustomMouseEventRedirector: false
    readonly property bool tabsAtTop: !groupCpp || !groupCpp.tabsAtBottom
    property alias tabBarHeight: tabbar.height

    anchors.fill: parent
    radius: Pal.radius + 2
    color: Pal.s2

    onGroupCppChanged: if (groupCpp) groupCpp.setStackLayout(stackLayout)
    onNonContentsHeightChanged: if (groupCpp) groupCpp.geometryUpdated()

    Loader {
        id: titleBar
        readonly property QtObject titleBarCpp: root.titleBarCpp
        source: root.groupCpp ? Singletons.widgetFactory.titleBarFilename() : ""
        anchors { top: parent.top; left: parent.left; right: parent.right }
    }
    Loader {
        id: tabbar
        readonly property GroupView groupCpp: root.groupCpp
        readonly property bool hasCustomMouseEventRedirector: root.hasCustomMouseEventRedirector
        source: groupCpp ? Singletons.widgetFactory.tabbarFilename() : ""
        anchors {
            left: parent.left
            right: parent.right
            top: (titleBar && titleBar.visible) ? titleBar.bottom : parent.top
            leftMargin: 1
            rightMargin: 1
            topMargin: 1
        }
    }
    Rectangle { // rounded outline on top of the contents (QtQuick clips rectangles, not radii)
        anchors.fill: parent
        z: 1000
        radius: root.radius
        color: "transparent"
        border.width: 1
        border.color: Pal.border
    }
    Item {
        id: stackLayout
        anchors {
            left: parent.left
            right: parent.right
            top: tabbar.visible ? tabbar.bottom : ((titleBar && titleBar.visible) ? titleBar.bottom : parent.top)
            bottom: parent.bottom
            margins: root.contentsMargin
        }
    }
}
