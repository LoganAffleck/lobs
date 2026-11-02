import QtQuick

// The LOBS gem, turning: frames of the site's ray-traced logo stone (web/scripts/gem-frames.mjs).
//   "flip": a quarter roll about the horizontal axis from the logo pose (0 → count - 1: rolling
//           down; back to 0: rolling up). Frame 0 is the logo. Doesn't wrap.
//   "spin": a third of a turn about its own axis, which loops (the d20 repeats every 120°).
// The frames are JPEGs on black; shaders/gem.frag turns brightness into coverage and mixes the
// two frames either side of `frame`, so a fractional frame is a blend.
Item {
    id: gem
    property string sequence: "flip"
    readonly property bool loops: sequence === "spin"
    readonly property int count: loops ? 120 : 45
    property real frame: 0
    implicitWidth: 64; implicitHeight: 64

    readonly property real f: loops ? ((frame % count) + count) % count : Math.max(0, Math.min(count - 1, frame))
    readonly property int i0: Math.floor(f)
    readonly property int i1: loops ? (i0 + 1) % count : Math.min(count - 1, i0 + 1)
    function src(i) { return "qrc:/qt/qml/LOBS/resources/gem/" + sequence + "-" + i + ".jpg" }

    Image { id: imgA; visible: false; source: gem.src(gem.i0); smooth: true; mipmap: true }
    Image { id: imgB; visible: false; source: gem.src(gem.i1); smooth: true; mipmap: true }
    ShaderEffect {
        anchors.fill: parent
        property variant a: imgA
        property variant b: imgB
        property real t: gem.f - gem.i0
        fragmentShader: "qrc:/qt/qml/LOBS/shaders/gem.frag.qsb"
    }
}
