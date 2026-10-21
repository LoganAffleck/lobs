import QtQuick
import QtQuick.Layouts

GridLayout {
    id: stats
    // The numbers behind the status bar: the stats popup and the Stats panel show the same view.
    property bool showTitle: false
    readonly property var st: app.liveStats || ({})
    readonly property var ss: app.streamStats || ({})
    function warn(pct) { return pct > 1 ? "#fbbf24" : Pal.muted }
    columns: 2; columnSpacing: 16; rowSpacing: 6
    component K: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body }
    component V: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; Layout.alignment: Qt.AlignRight }
    Text { visible: stats.showTitle; text: "Stats"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.columnSpan: 2 }
    K { text: "Frame rate" }          V { text: (stats.st.fps || 0).toFixed(2) + " / " + (stats.st.targetFps || 60) + " fps" }
    K { text: "Render time" }         V { text: (stats.st.renderMs || 0).toFixed(2) + " ms" }
    K { text: "Frames missed (render lag)" } V { text: (stats.st.lagged || 0) + " / " + (stats.st.rendered || 0) + " (" + (stats.st.laggedPct || 0).toFixed(1) + "%)"; color: stats.warn(stats.st.laggedPct || 0) }
    K { text: "Frames skipped (encoder lag)" } V { text: (stats.st.skipped || 0) + " / " + (stats.st.encoded || 0) + " (" + (stats.st.skippedPct || 0).toFixed(1) + "%)"; color: stats.warn(stats.st.skippedPct || 0) }
    K { text: "CPU" }                 V { text: (stats.st.cpu || 0).toFixed(1) + "%" }
    K { text: "Memory" }              V { text: (stats.st.memoryMb || 0).toFixed(0) + " MB" }
    K { text: "Free space for recordings" } V { text: (stats.st.diskFreeGb || 0).toFixed(1) + " GB" }
    Rectangle { Layout.columnSpan: 2; Layout.fillWidth: true; implicitHeight: 1; color: Pal.border }
    K { text: "Stream" }              V { text: app.streaming ? (stats.ss.time || "") + " · " + Math.round(stats.ss.kbps || 0) + " kbps" : "Off" }
    K { visible: app.streaming; text: "Dropped frames (network)" } V { visible: app.streaming; text: (stats.ss.dropped || 0) + " / " + (stats.ss.total || 0) + " (" + (stats.ss.dropPct || 0).toFixed(1) + "%)"; color: stats.warn(stats.ss.dropPct || 0) }
    K { text: "Recording" }           V { text: app.recording ? (stats.st.recTime || "") + " · " + (stats.st.recMb || 0).toFixed(0) + " MB" : "Off" }
    K { text: "Replay buffer" }       V { text: app.replayActive ? "On · last " + (app.settings.replaySeconds || 30) + " s" : "Off" }
    K { text: "Virtual camera" }      V { text: app.vcamActive ? "On" : "Off" }
}
