import QtQuick
import QtQuick.Controls

// Number field with an inline label and unit; accepts math like 1920/2. Up/Down nudge (Shift ×10).
// Drag left/right on it to scrub the value (Shift ×10, Ctrl ×0.1); a plain click types.
FInput {
    id: root
    Accessible.role: Accessible.SpinBox
    Accessible.name: label !== "" ? label : "Number"
    property string label
    property string suffix
    property real value
    property real from: -1e9
    property real to: 1e9
    property real step: 1
    // live: scrubbing reports every step (scrubbed(v, false)) and the release (scrubbed(v, true)),
    // so the caller can preview and make one undo step. Otherwise only the release commits.
    property bool live: false
    signal commit(real v)
    signal scrubbed(real v, bool final)
    // A fixed natural width, so layouts split evenly and fields don't resize as digits change.
    implicitWidth: 64
    leftPadding: lab.implicitWidth + 16
    rightPadding: suffix ? unit.implicitWidth + 14 : 10
    text: fmt(value)
    function fmt(v) { return String(Math.round(v * 100) / 100) }
    function clampV(v) { return Math.max(from, Math.min(to, v)) }
    Text { id: lab; text: root.label; x: 8; anchors.verticalCenter: parent.verticalCenter; color: scrub.scrubbing ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    Text { id: unit; visible: root.suffix !== ""; text: root.suffix; anchors.right: parent.right; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    // Tiny arithmetic parser: + - * / and parentheses.
    function calc(src) {
        let i = 0
        const s = src.replace(/\s+/g, "")
        function num() {
            if (s[i] === "(") { i++; const v = sum(); i++; return v }
            if (s[i] === "-") { i++; return -num() }
            const m = /^[0-9]*\.?[0-9]+/.exec(s.slice(i))
            if (!m) throw "bad"
            i += m[0].length
            return Number(m[0])
        }
        function prod() { let v = num(); while (s[i] === "*" || s[i] === "/") { const op = s[i++]; const r = num(); v = op === "*" ? v * r : v / r } return v }
        function sum() { let v = prod(); while (s[i] === "+" || s[i] === "-") { const op = s[i++]; const r = prod(); v = op === "+" ? v + r : v - r } return v }
        const v = sum()
        if (i !== s.length) throw "bad"
        return v
    }
    onEditingFinished: {
        let v; try { v = calc(text) } catch (e) { v = NaN }
        if (isFinite(v) && v !== value) commit(clampV(v))
        else text = fmt(value)
    }
    Keys.onUpPressed: (e) => commit(clampV(value + step * (e.modifiers & Qt.ShiftModifier ? 10 : 1)))
    Keys.onDownPressed: (e) => commit(clampV(value - step * (e.modifiers & Qt.ShiftModifier ? 10 : 1)))
    onValueChanged: if (!activeFocus) text = fmt(value)

    // Scrub: covers the field until it has focus; while you're typing in it, the label still scrubs
    // (the text area takes the mouse for the caret and selection).
    MouseArea {
        id: scrub
        Accessible.role: Accessible.SpinBox
        Accessible.name: root.label !== "" ? root.label : "Number"
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: root.activeFocus ? root.leftPadding : root.width
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor
        property real startX: 0
        property real startV: 0
        property real lastV: 0
        property bool scrubbing: false
        onPressed: (m) => { startX = m.x; startV = root.value; lastV = root.value; scrubbing = false }
        onPositionChanged: (m) => {
            if (!pressed) return
            const dx = m.x - startX
            if (!scrubbing && Math.abs(dx) < 3) return
            if (!scrubbing && root.activeFocus) root.focus = false // scrubbing replaces what was being typed
            scrubbing = true
            const k = (m.modifiers & Qt.ShiftModifier) ? 10 : (m.modifiers & Qt.ControlModifier) ? 0.1 : 1
            const v = root.clampV(Math.round((startV + (dx - (dx > 0 ? 3 : -3)) * root.step * k) * 10) / 10)
            if (v === lastV) return
            lastV = v
            root.text = root.fmt(v)
            if (root.live) root.scrubbed(v, false)
        }
        onReleased: {
            if (scrubbing) {
                scrubbing = false
                if (root.live) root.scrubbed(lastV, true)
                else if (lastV !== root.value) root.commit(lastV)
            } else {
                root.forceActiveFocus()
                root.selectAll()
            }
        }
        onCanceled: { if (scrubbing && root.live) root.scrubbed(lastV, true); scrubbing = false }
    }
}
