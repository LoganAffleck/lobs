import QtQuick

// Small status badge. Tones: neutral, live, link, ok.
Rectangle {
    id: bd
    property string text
    property string tone: "neutral"
    implicitHeight: 18
    implicitWidth: t.implicitWidth + 12
    radius: 9
    color: tone === "live" ? "#3a1414" : tone === "link" ? "#0f2838" : tone === "ok" ? "#10291a" : Pal.s5
    Text {
        id: t
        anchors.centerIn: parent
        width: Math.min(implicitWidth, bd.width - 12)
        elide: Text.ElideRight
        text: bd.text
        color: bd.tone === "live" ? "#fca5a5" : bd.tone === "link" ? Pal.link : bd.tone === "ok" ? "#86efac" : Pal.muted
        font.family: Pal.font; font.pixelSize: 11; font.weight: 550
    }
}
