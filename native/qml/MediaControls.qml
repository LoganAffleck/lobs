import QtQuick
import QtQuick.Layouts

// Play, restart, stop and seek for the selected video or audio file. Playback is a live
// control: it works on air while live lock is on, like show and hide.
Rectangle {
    id: bar
    property string sourceId: ""
    property var info: ({ state: "none", timeMs: 0, durationMs: 0 })
    readonly property bool playing: info.state === "playing" || info.state === "opening" || info.state === "buffering"
    function refresh() { if (sourceId !== "") info = app.mediaInfo(sourceId) }
    function fmt(ms) {
        const s = Math.max(0, Math.floor(ms / 1000)), h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, sec = s % 60
        return (h ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(sec).padStart(2, "0")
    }
    onSourceIdChanged: refresh()
    Timer { interval: 250; repeat: true; running: bar.visible && bar.sourceId !== ""; onTriggered: bar.refresh() }

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 12
    radius: 10
    color: Pal.s3
    border.width: 1; border.color: Pal.border

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: 6; anchors.rightMargin: 10
        spacing: 2
        FButton { iconName: bar.playing ? "pause" : "play"; variant: "ghost"; tip: bar.playing ? "Pause" : "Play"; onClicked: { app.mediaCommand(bar.sourceId, "playPause"); bar.refresh() } }
        FButton { iconName: "rotate-ccw"; variant: "ghost"; tip: "Play from the start"; onClicked: { app.mediaCommand(bar.sourceId, "restart"); bar.refresh() } }
        FButton { iconName: "square"; variant: "ghost"; tip: "Stop"; onClicked: { app.mediaCommand(bar.sourceId, "stop"); bar.refresh() } }
        FFillSlider {
            Layout.preferredWidth: 180
            Layout.leftMargin: 6
            label: ""
            from: 0
            to: Math.max(1, bar.info.durationMs)
            value: bar.info.timeMs
            format: (v) => bar.fmt(v) + " / " + bar.fmt(bar.info.durationMs)
            onReleased: (v) => { app.mediaCommand(bar.sourceId, "seek", v); bar.refresh() }
        }
    }
}
