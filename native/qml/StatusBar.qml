import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom status bar: what's running and how healthy it is, at a glance. Click for full stats.
Rectangle {
    id: bar
    readonly property int edgeRoom: Window.window && Window.window.screenEdgeRoom !== undefined ? Window.window.screenEdgeRoom : 0 // filling the screen
    implicitHeight: 26 + edgeRoom
    color: Pal.s1
    readonly property var st: app.liveStats || ({})
    readonly property var ss: app.streamStats || ({})
    function warn(pct) { return pct > 1 ? "#fbbf24" : Pal.muted }
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Pal.border }

    component Seg: Row {
        property alias text: t.text
        property color tone: Pal.muted
        property color dot: "transparent"
        spacing: 6
        Rectangle { visible: parent.dot !== "transparent"; width: 7; height: 7; radius: 3.5; color: parent.dot; anchors.verticalCenter: parent.verticalCenter }
        Text { id: t; color: parent.tone; font.family: Pal.font; font.pixelSize: Pal.caption; anchors.verticalCenter: parent.verticalCenter }
    }

    MouseArea { Accessible.role: Accessible.Button; Accessible.name: "Stream and recording stats"; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: statsPopup.opened ? statsPopup.close() : statsPopup.open() }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.bottomMargin: bar.edgeRoom
        spacing: 16
        Text { text: app.status; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; elide: Text.ElideRight; Layout.fillWidth: true }
        Seg {
            visible: app.streaming
            dot: Pal.live
            // Amber when the connection is fair, red when poor or reconnecting.
            tone: bar.ss.health === "poor" || bar.ss.health === "reconnecting" ? "#f87171" : bar.ss.health === "fair" ? "#fbbf24" : Pal.fg
            text: bar.ss.reconnecting ? (app.testStreaming ? "TEST " : "LIVE ") + (bar.ss.time || "00:00:00") + " · Reconnecting…"
                : (app.testStreaming ? "TEST " : "LIVE ") + (bar.ss.time || "00:00:00") + " · " + Math.round(bar.ss.kbps || 0) + " kbps · " + (bar.ss.dropped || 0) + " dropped (" + (bar.ss.dropPct || 0).toFixed(1) + "%)"
        }
        Seg { visible: app.recording; dot: Pal.live; tone: Pal.fg; text: "REC " + (bar.st.recTime || "00:00:00") + " · " + (bar.st.recMb || 0).toFixed(0) + " MB" }
        Seg { visible: app.replayActive; dot: "#22c55e"; text: "Replay on" }
        Seg { visible: app.vcamActive; dot: "#22c55e"; text: "Virtual camera" }
        Seg { text: "CPU " + (bar.st.cpu || 0).toFixed(1) + "%"; tone: (bar.st.cpu || 0) > 80 ? "#fbbf24" : Pal.muted }
        Seg {
            text: (bar.st.fps || 0).toFixed(0) + " / " + (bar.st.targetFps || 60) + " fps · " + (bar.st.renderMs || 0).toFixed(1) + " ms"
            tone: (bar.st.fps || 0) < (bar.st.targetFps || 60) - 2 ? "#fbbf24" : Pal.muted
        }
        Seg {
            // OBS's own warning level, over the last 10 s (LOBS-005); the popup shows totals.
            visible: (bar.st.recentLaggedPct || 0) > 1 || (bar.st.recentSkippedPct || 0) > 1
            tone: "#fbbf24"
            text: "Lagging " + (bar.st.recentLaggedPct || 0).toFixed(1) + "% · skipped " + (bar.st.recentSkippedPct || 0).toFixed(1) + "%"
        }
        Seg { visible: (bar.st.diskFreeGb || 99) < 10; tone: "#fbbf24"; text: (bar.st.diskFreeGb || 0).toFixed(1) + " GB free" }
    }

    Popup {
        popupType: Popup.Item // in the scene, not an OS window (see FMenu)
        id: statsPopup
        x: parent.width - width - 8
        y: -height - 6
        width: 340
        padding: 14
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        property bool coversPreview: false
        // Only step the native preview aside if this popup actually overlaps it (no flash otherwise).
        function overlapsPreview() {
            if (app.previewInScene) return false
            const w = parent ? parent.Window.window : null
            const p = w ? w.previewItem : null
            if (!p || !p.visible || !parent) return false
            const a = p.mapToGlobal(0, 0), b = parent.mapToGlobal(x, y)
            return b.x < a.x + p.width && b.x + width > a.x && b.y < a.y + p.height && b.y + height > a.y
        }
        onAboutToShow: { coversPreview = overlapsPreview(); if (coversPreview) app.popupOpened() }
        onAboutToHide: if (coversPreview) { coversPreview = false; app.popupClosed() }
        background: Rectangle { radius: Pal.radius + 4; color: Pal.s4; border.width: 1; border.color: Pal.border }
        contentItem: StatsView { showTitle: true }
    }
}
