import QtQuick

// A name edited in place (layer rows, scene rows and tiles): shown while `active`; Enter or
// clicking away keeps it, Esc puts it back. `renamed` comes only for a real change.
TextInput {
    id: ed
    property bool active: false
    property string original
    signal renamed(string name)
    signal finished() // either way: the holder clears `active`
    visible: active
    color: Pal.fg; selectionColor: Pal.focus; selectedTextColor: "white"
    font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550
    clip: true; selectByMouse: true
    property bool done: false
    onActiveChanged: if (active) { done = false; text = original; selectAll(); forceActiveFocus() }
    function finish(keep) {
        if (done) return
        done = true
        const t = text.trim()
        if (keep && t !== "" && t !== original) renamed(t)
        finished()
    }
    onAccepted: finish(true)
    onActiveFocusChanged: if (!activeFocus && active) finish(true)
    Keys.onEscapePressed: finish(false)
    Rectangle { anchors.fill: parent; anchors.margins: -3; z: -1; radius: 4; color: Pal.s1; border.width: 1; border.color: Pal.focus }
}
