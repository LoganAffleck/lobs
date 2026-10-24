import QtQuick
import QtQuick.Layouts
import LOBS.Web

// A Twitch page in a panel (chat, stream info), in the engine's browser. Sign in inside the
// panel once; it stays signed in. Asks for the channel name if setup didn't get it.
Rectangle {
    id: tp
    property string kind: "chat" // chat | info
    property bool live: true
    property string channel: ""      // shared by the panels (set by the window)
    signal channelPicked(string name)
    property bool editing: false     // "Change channel" shows the name field again
    readonly property bool hasPage: !!channel && !editing
    color: Pal.s1
    readonly property string url: !hasPage ? ""
        : kind === "chat" ? "https://www.twitch.tv/popout/" + channel + "/chat?darkpopout"
        : "https://dashboard.twitch.tv/popout/u/" + channel + "/stream-manager/edit-stream-info"
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        // Thin toolbar: reload, open in your browser, change channel.
        RowLayout {
            visible: tp.hasPage
            Layout.fillWidth: true; Layout.leftMargin: 8; Layout.rightMargin: 4
            implicitHeight: 30; spacing: 2
            Caption { text: tp.channel; Layout.fillWidth: true; elide: Text.ElideRight }
            FButton { visible: Math.abs(view.zoom - 1) > 0.01; text: Math.round(view.zoom * 100) + "%"; variant: "ghost"; tip: "Zoom (" + chrome.keys("Ctrl") + " + / " + chrome.keys("Ctrl") + " -). Click for 100%."; onClicked: view.zoom = 1 }
            FButton { iconName: "refresh-cw"; variant: "ghost"; tip: "Reload"; onClicked: view.reload() }
            FButton { iconName: "external-link"; variant: "ghost"; tip: "Open in your browser"; onClicked: Qt.openUrlExternally(tp.url.replace("?darkpopout", "")) }
            FButton { iconName: "settings"; variant: "ghost"; tip: "Change channel"; onClicked: { chIn.text = tp.channel; tp.editing = true } }
        }
        BrowserView {
            id: view
            visible: tp.hasPage
            Layout.fillWidth: true; Layout.fillHeight: true
            url: tp.url
            live: tp.live && visible
            zoom: Number(app.uiValue("twitch/zoom/" + tp.kind, "1"))
            onZoomChanged: { app.setUiValue("twitch/zoom/" + tp.kind, String(zoom)); zoomBadge.opacity = 1; zoomFade.restart() }
            // Shows the zoom level for a moment after it changes.
            Rectangle {
                id: zoomBadge
                anchors { top: parent.top; right: parent.right; margins: 8 }
                width: zt.implicitWidth + 16; height: 24; radius: 12
                color: Pal.s5; border.width: 1; border.color: Pal.border
                opacity: 0
                Behavior on opacity { NumberAnimation { duration: Pal.moderate } }
                Text { id: zt; anchors.centerIn: parent; text: Math.round(view.zoom * 100) + "%"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.caption }
                Timer { id: zoomFade; interval: 1200; onTriggered: zoomBadge.opacity = 0 }
            }
            Caption { visible: !view.ready; anchors.centerIn: parent; text: "Loading Twitch…" }
        }
        ColumnLayout {
            visible: !tp.hasPage
            Layout.fillWidth: true; Layout.fillHeight: true; Layout.margins: 16
            spacing: 8
            Item { Layout.fillHeight: true }
            Text { text: tp.kind === "chat" ? "Twitch chat" : "Stream info"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550 }
            Caption { Layout.fillWidth: true; text: "As in twitch.tv/yourname." }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                FInput { id: chIn; Layout.fillWidth: true; placeholderText: "yourname"; onAccepted: go.clicked() }
                FButton {
                    id: go; text: "Open"; variant: "primary"
                    onClicked: {
                        const c = chIn.text.trim().replace(/^.*twitch\.tv\//i, "").replace(/\/.*$/, "").toLowerCase()
                        if (!c) return
                        tp.editing = false; tp.channelPicked(c)
                    }
                }
            }
            Item { Layout.fillHeight: true }
        }
    }
}
