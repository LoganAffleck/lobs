import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What's new: once, on the first start after an update (installed by LOBS or by hand), the
// release's highlights. Add each release's to `notes` when bumping the version (BUILDING.md).
FDialog {
    id: dlg
    title: "What's new in LOBS " + version
    width: 480
    property string version: ""
    readonly property var notes: ({
        "0.7.9": [
            { icon: "film", title: "A clearer Transitions window", text: "Stingers play in the preview, show their length and a frame, and warn when the video is missing." },
            { icon: "sliders-horizontal", title: "Switch point on a timeline", text: "Drag where the scenes switch along the stinger's own length." },
            { icon: "square", title: "No see-through canvas", text: "Empty parts of the canvas stay black instead of showing your desktop." },
        ],
        "0.7.8": [
            { icon: "zap", title: "Opens faster", text: "LOBS is ready about 0.4 seconds sooner. Dialogs are built when you first need them." },
            { icon: "circle", title: "Start recording on launch", text: "Start LOBS with --startrecording to record right away, like OBS's flag." },
        ],
        "0.7.7": [
            { icon: "sparkles", title: "Transitions on the canvas", text: "Switching scenes plays your transition, stingers included, like OBS's preview." },
            { icon: "crop", title: "Free crop and resize", text: "Crop doesn't lock to a ratio unless you pick one. Browser layers and shapes resize freely." },
            { icon: "arrow-up-right", title: "Draw many arrows", text: "The arrow tool stays on after each arrow, like the pencil." },
        ],
        "0.7.6": [
            { icon: "search", title: "Search your scenes", text: "Scenes has a search button too, like Layers and Sources." },
            { icon: "expand", title: "Resizing keeps the shape", text: "Dragging an edge scales the layer instead of stretching it. Hold Alt to scale from the center." },
            { icon: "sparkles", title: "This card", text: "After each update, the highlights show here once." },
        ],
    })
    readonly property var items: notes[version] || []
    // Called at startup: shows the card if this is a new version with highlights, then remembers it.
    function check() {
        const v = appUpdater.current, last = app.uiValue("app/lastVersion", "")
        app.setUiValue("app/lastVersion", v)
        if (last && last !== v && notes[v]) { version = v; open() }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 14
        Repeater {
            model: dlg.items
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 12
                Rectangle {
                    readonly property color accent: Pal.link // a color, so its channels can be read
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: 32; implicitHeight: 32; radius: 16
                    color: Qt.rgba(accent.r, accent.g, accent.b, 0.18)
                    FIcon { anchors.centerIn: parent; name: modelData.icon; size: 16; color: Pal.fg }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text { text: modelData.title; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 600 }
                    Text { text: modelData.text; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 4
            FButton { text: "Release notes"; variant: "ghost"; onClicked: Qt.openUrlExternally("https://github.com/LoganAffleck/lobs/releases/tag/v" + dlg.version) }
            Item { Layout.fillWidth: true }
            FButton { text: "Got it"; variant: "primary"; onClicked: dlg.close() }
        }
    }
}
