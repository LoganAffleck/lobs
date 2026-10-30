import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The four outputs OBS users reach for, in one place, each showing its own state:
//   [ Replay | Virtual cam | Record ]   [ Go live ]
// Secondary outputs share one quiet group with status dots; Record becomes a red timer while
// recording; Go live is the primary action, a red LIVE timer while streaming, and ending the
// stream asks for a second click so a stray click can't cut you off.
RowLayout {
    id: oc
    spacing: 8
    readonly property var st: app.liveStats || ({})
    readonly property var ss: app.streamStats || ({})

    component Seg: AbstractButton {
        id: seg
        Accessible.role: Accessible.Button
        Accessible.name: text !== "" ? text : (tip !== "" ? tip : iconName)
        property string iconName
        property bool active: false
        property color dot: "#22c55e"
        property string tip
        hoverEnabled: true
        implicitHeight: Pal.ctl
        implicitWidth: segRow.implicitWidth + 20
        background: Rectangle {
            radius: Pal.radius - 2
            color: seg.down ? Pal.s6 : seg.hovered ? Pal.s5 : "transparent"
            Behavior on color { ColorAnimation { duration: Pal.fast } }
        }
        contentItem: Item {
            Row {
                id: segRow
                anchors.centerIn: parent
                spacing: 6
                Item {
                    width: 14; height: 14
                    anchors.verticalCenter: parent.verticalCenter
                    FIcon { anchors.centerIn: parent; name: seg.iconName; size: 14; color: seg.active || seg.hovered ? Pal.fg : Pal.muted }
                    Rectangle { // status dot
                        visible: seg.active
                        width: 7; height: 7; radius: 3.5; color: seg.dot
                        border.width: 1.5; border.color: Pal.s2
                        anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: -3; anchors.topMargin: -3
                    }
                }
                Text { text: seg.text; color: seg.active || seg.hovered ? Pal.fg : Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: seg.active ? 550 : 400; anchors.verticalCenter: parent.verticalCenter }
            }
        }
        ToolTip.visible: hovered && tip.length > 0
        ToolTip.delay: 600
        ToolTip.text: tip
    }

    // ---- secondary outputs ----
    Rectangle {
        implicitHeight: Pal.ctl + 6
        implicitWidth: group.implicitWidth + 6
        radius: Pal.radius
        color: Pal.s2
        border.width: 1; border.color: Pal.border
        RowLayout {
            id: group
            anchors.centerIn: parent
            spacing: 0
            Seg {
                iconName: "history"
                text: "Replay"
                active: app.replayActive
                tip: app.replayActive ? "Replay buffer on (last " + (app.settings.replaySeconds || 30) + " s). Click to turn off."
                                      : "Keep the last " + (app.settings.replaySeconds || 30) + " s to save as a clip"
                onClicked: app.toggleReplay()
            }
            FButton { // appears only while the replay buffer runs
                visible: app.replayActive
                implicitHeight: Pal.ctl - 6
                Layout.rightMargin: 4
                iconName: "save"; text: "Save"; variant: "secondary"
                tip: "Save the last " + (app.settings.replaySeconds || 30) + " s to your recordings folder"
                onClicked: app.saveReplay()
            }
            Rectangle { implicitWidth: 1; implicitHeight: 14; color: Pal.border }
            Seg {
                iconName: "video"
                text: "Virtual cam"
                active: app.vcamActive
                tip: app.vcamActive ? "On as \"OBS Virtual Camera\". Click to stop."
                     : app.vcamAvailable ? "Send the on-air scene to other apps as \"OBS Virtual Camera\""
                     : "Set up the virtual camera. Windows asks for permission once."
                onClicked: app.toggleVirtualCam()
            }
            Rectangle { implicitWidth: 1; implicitHeight: 14; color: Pal.border }
            Seg {
                iconName: app.recording ? "square" : "circle"
                text: app.recording ? (oc.st.recTime || "00:00:00") : "Record"
                active: app.recording
                dot: Pal.live
                tip: app.recording ? "Stop recording (" + (oc.st.recMb || 0).toFixed(0) + " MB)" : "Record the on-air scene"
                onClicked: app.toggleRecording()
            }
        }
    }

    // ---- primary: Go live / LIVE ----
    AbstractButton {
        id: live
        Accessible.role: Accessible.Button
        Accessible.name: app.testStreaming ? "Stop the test stream" : app.streaming ? "Stop streaming" : "Go live"
        property bool confirming: false
        hoverEnabled: true
        implicitHeight: Pal.ctl + 6
        implicitWidth: liveRow.implicitWidth + 24
        Timer { id: confirmTimer; interval: 3000; onTriggered: live.confirming = false }
        onClicked: {
            if (!app.streaming) return app.toggleStreaming()
            if (!confirming) { confirming = true; confirmTimer.restart(); return }
            confirming = false
            app.toggleStreaming()
        }
        background: Rectangle {
            radius: Pal.radius
            color: app.streaming ? (live.confirming ? "#b91c1c" : "#3b0d0d") : Pal.fg
            border.width: app.streaming ? 1 : 0
            border.color: "#7f1d1d"
            Behavior on color { ColorAnimation { duration: Pal.moderate } }
            Rectangle {
                anchors.fill: parent; radius: parent.radius
                color: app.streaming ? "#ffffff" : "#000000"
                opacity: live.hovered ? 0.07 : 0
                Behavior on opacity { NumberAnimation { duration: Pal.fast } }
            }
        }
        contentItem: Item {
            Row {
                id: liveRow
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    visible: app.streaming
                    width: 8; height: 8; radius: 4; color: Pal.live
                    anchors.verticalCenter: parent.verticalCenter
                    SequentialAnimation on opacity { running: app.streaming; loops: Animation.Infinite; NumberAnimation { to: 0.35; duration: 700 } NumberAnimation { to: 1; duration: 700 } }
                }
                FIcon { visible: !app.streaming; name: "radio-tower"; size: 14; color: Pal.s1; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: app.preparingStream ? "Starting…"
                        : !app.streaming ? "Go live"
                        : live.confirming ? "Click again to stop"
                        : (app.testStreaming ? "TEST  " : "LIVE  ") + (oc.ss.time || "00:00:00")
                    color: app.streaming ? "#fee2e2" : Pal.s1
                    font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 600
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    visible: app.streaming && !live.confirming
                    text: oc.ss.stalledSec ? "connection lost?" : oc.ss.health === "reconnecting" ? "reconnecting" + (oc.ss.reconnectIn ? " in " + oc.ss.reconnectIn + " s" : "…") : Math.round(oc.ss.kbps || 0) + " kbps"
                    color: oc.ss.health === "poor" || oc.ss.health === "reconnecting" ? "#fbbf24" : "#fca5a5"
                    font.family: Pal.font; font.pixelSize: Pal.caption
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: app.streaming ? (oc.ss.dropped || 0) + " dropped frames. Click twice to stop." : "Start streaming"
    }
}
