import QtQuick
import "qrc:/kddockwidgets/qtquick/views/qml/" as KDDW
import LOBS

// Title bar, only shown on floating panel windows (docked panels show tabs instead).
KDDW.TitleBarBase {
    id: root
    color: Pal.s3
    heightWhenVisible: 34
    Text {
        text: root.title
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        color: Pal.fg
        font.family: Pal.font
        font.pixelSize: Pal.body
        font.weight: 550
    }
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        FButton { visible: root.floatButtonVisible; iconName: "layout-grid"; variant: "ghost"; tip: "Dock back"; onClicked: root.floatButtonClicked() }
        FButton { visible: root.maximizeButtonVisible; iconName: "expand"; variant: "ghost"; tip: "Maximize"; onClicked: root.maximizeButtonClicked() }
        FButton { enabled: root.closeButtonEnabled; iconName: "x"; variant: "ghost"; tip: "Close"; onClicked: root.closeButtonClicked() }
    }
}
