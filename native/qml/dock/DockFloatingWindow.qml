import QtQuick
import com.kdab.dockwidgets 2.0
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
import LOBS

// A floated-out panel window. Adapted from KDDockWidgets' FloatingWindow.qml.
Rectangle {
    id: root
    readonly property FloatingWindowView floatingWindowCpp: parent
    readonly property TitleBarView titleBarCpp: floatingWindowCpp ? floatingWindowCpp.titleBar : null
    readonly property DropAreaView dropAreaCpp: floatingWindowCpp ? floatingWindowCpp.dropArea : null
    readonly property int titleBarHeight: titleBar.heightWhenVisible
    property int margins: 1
    anchors.fill: parent
    color: Pal.s1
    border.width: 1
    border.color: Pal.borderStrong
    onTitleBarHeightChanged: if (floatingWindowCpp) floatingWindowCpp.geometryUpdated()
    Loader {
        id: titleBar
        readonly property TitleBarView titleBarCpp: root.titleBarCpp
        readonly property int heightWhenVisible: item ? item.heightWhenVisible : 0
        source: Singletons.widgetFactory.titleBarFilename()
        anchors { top: parent.top; left: parent.left; right: parent.right; margins: root.margins }
    }
    KDDW.DropArea {
        id: dropArea
        dropAreaCpp: root.dropAreaCpp
        anchors { left: parent.left; right: parent.right; top: titleBar.bottom; bottom: parent.bottom; margins: root.margins }
    }
    onDropAreaCppChanged: {
        if (dropAreaCpp) {
            dropAreaCpp.parent = dropArea
            dropAreaCpp.anchors.fill = dropArea
        }
    }
}
