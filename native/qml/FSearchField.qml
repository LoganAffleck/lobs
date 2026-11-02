import QtQuick
import QtQuick.Layouts

// Panel search: a field with an x. `query` is the trimmed, lower-cased text.
// The x (or Esc) clears it and closes the search (`dismissed`).
FInput {
    id: sf
    readonly property string query: text.trim().toLowerCase()
    signal dismissed()
    placeholderText: "Search"
    leftPadding: 28
    rightPadding: 28
    function matches(...fields) { return !query || fields.some(f => String(f || "").toLowerCase().includes(query)) }
    FIcon { name: "search"; size: 13; color: Pal.muted; x: 9; anchors.verticalCenter: parent.verticalCenter }
    FButton {
        iconName: "x"; variant: "ghost"
        implicitWidth: 22; implicitHeight: 22
        anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter
        tip: "Close search"
        onClicked: { sf.text = ""; sf.dismissed() }
    }
    Keys.onEscapePressed: { text = ""; sf.dismissed() }
}
