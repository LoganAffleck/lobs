import QtQuick
import QtQuick.Layouts

// Panel search: a field with a clear button. `query` is the trimmed, lower-cased text.
// Esc clears it (and asks the panel to close the search with `dismissed`).
FInput {
    id: sf
    readonly property string query: text.trim().toLowerCase()
    signal dismissed()
    placeholderText: "Search"
    leftPadding: 28
    rightPadding: text.length ? 28 : 10
    function matches(...fields) { return !query || fields.some(f => String(f || "").toLowerCase().includes(query)) }
    FIcon { name: "search"; size: 13; color: Pal.muted; x: 9; anchors.verticalCenter: parent.verticalCenter }
    FButton {
        visible: sf.text.length > 0
        iconName: "x"; variant: "ghost"
        implicitWidth: 22; implicitHeight: 22
        anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter
        tip: "Clear"
        onClicked: { sf.text = ""; sf.forceActiveFocus() }
    }
    Keys.onEscapePressed: { if (text.length) text = ""; else sf.dismissed() }
}
