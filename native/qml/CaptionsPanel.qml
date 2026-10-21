import QtQuick
import QtQuick.Layouts

// Live captions: speech to text from one audio source, shown here and sent inside the
// stream as closed captions (viewers turn on CC). Runs offline with whisper.cpp.
Item {
    id: cp
    // The source to listen to, remembered between sessions.
    property string pick: app.uiValue("captions/source", "")
    readonly property var audioSources: app.mixer.map(m => ({ name: m.name, value: m.id }))
    readonly property string pickedName: (audioSources.find(o => o.value === (captions.running ? captions.sourceId : pick)) || {}).name || ""
    function startStop() {
        if (captions.running) return captions.stop()
        const id = pick || (audioSources[0] || {}).value || ""
        if (id) { pick = id; app.setUiValue("captions/source", id); captions.start(id) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // The speech model is downloaded once, on first use.
        Rectangle {
            visible: captions.modelState !== "ready"
            Layout.fillWidth: true
            implicitHeight: dl.implicitHeight + 20
            radius: Pal.radius
            color: Pal.s3
            border.width: 1; border.color: Pal.border
            ColumnLayout {
                id: dl
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 10 }
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.body
                    text: captions.modelState === "downloading" ? "Downloading the speech model… " + Math.round(captions.downloadProgress * 100) + "%"
                        : captions.modelState === "error" ? captions.status
                        : "Captions run on this PC, so your audio never leaves it. They need a one-time 142 MB download."
                }
                FButton {
                    visible: captions.modelState !== "downloading"
                    text: captions.modelState === "error" ? "Try again" : "Download speech model"
                    variant: "secondary"
                    onClicked: captions.downloadModel()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            FSelect {
                Layout.fillWidth: true
                enabled: !captions.running
                placeholder: "Audio source…"
                options: cp.audioSources
                currentValue: cp.pick
                onPicked: (v) => { cp.pick = v; app.setUiValue("captions/source", v) }
            }
            FButton {
                text: captions.running ? "Stop" : "Start"
                variant: captions.running ? "secondary" : "primary"
                enabled: captions.modelState === "ready" && cp.audioSources.length > 0
                onClicked: cp.startStop()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            FSelect {
                Layout.fillWidth: true
                enabled: !captions.running
                options: captions.languages()
                currentValue: captions.language
                onPicked: (v) => captions.language = v
            }
            FSwitch {
                text: "Send to stream"
                checked: captions.toStream
                onClicked: captions.toStream = checked
            }
        }

        // The running transcript, newest at the bottom.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Pal.radius
            color: Pal.s2
            border.width: 1; border.color: Pal.border
            ListView {
                id: transcript
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 4
                model: captions.lines
                onCountChanged: Qt.callLater(positionViewAtEnd)
                delegate: Text {
                    required property var modelData
                    width: ListView.view.width
                    wrapMode: Text.WordWrap
                    color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body
                    text: modelData.text
                }
            }
            Text {
                visible: transcript.count === 0
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                text: captions.running ? "Listening. Text shows up a few seconds after speech." : "Pick a mic or desktop audio, then Start."
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Rectangle { width: 8; height: 8; radius: 4; color: captions.running ? Pal.live : Pal.muted }
            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption
                text: !captions.running ? (captions.status || "Captions are off")
                    : "CC (" + cp.pickedName + ")" + (captions.toStream ? (app.streaming ? " for Stream" : " · sent once you go live") : " · not sent to the stream")
            }
            FButton { iconName: "trash-2"; variant: "ghost"; tip: "Clear the transcript"; onClicked: captions.clear() }
        }
    }
}
