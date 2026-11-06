import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import LOBS

FScrollView {
    id: sv
    property size kddockwidgets_min_size: Qt.size(280, 200)
    clip: true
    contentWidth: availableWidth
    property var d: app.inspector

    component Body: Text { color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; elide: Text.ElideRight }
    component Caption: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption }
    component RowLabel: Text { color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 52 }

    ColumnLayout {
        width: sv.availableWidth
        spacing: 0
        Item { implicitHeight: 8 }

        ColumnLayout {
            id: toolDefaults
            readonly property string t: app.tool
            readonly property bool usesFill: t === "rect" || t === "ellipse" || t === "laser"
            readonly property bool usesStroke: t !== "laser"
            readonly property bool lineTool: t === "arrow" || t === "pencil"
            readonly property var names: ({ text: "Text", rect: "Rectangle", ellipse: "Ellipse", arrow: "Arrow", pencil: "Pencil", laser: "Laser pointer" })
            visible: sv.d.mode === "none" && t !== "select"
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 10
            Text { text: toolDefaults.names[toolDefaults.t] || ""; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.body; font.weight: 550 }
            Caption {
                Layout.fillWidth: true; wrapMode: Text.WordWrap
                text: toolDefaults.t === "laser" ? "A fading trail. Nothing is added to the scene."
                    : toolDefaults.t === "text" ? "Drag up or down to set the size, then type. Esc or click away to finish."
                                                 : "Applies to the next shape. Select a shape to change it."
            }
            RowLayout {
                visible: toolDefaults.t === "text"
                Layout.fillWidth: true; spacing: 4
                FFontPicker { Layout.fillWidth: true; currentFont: app.toolFont; onPicked: (f) => app.toolFont = f }
                FButton { text: "B"; font.bold: true; variant: "ghost"; selected: app.toolBold; tip: "Bold"; implicitWidth: Pal.ctl; onClicked: app.toolBold = !app.toolBold }
                FButton { text: "I"; font.italic: true; variant: "ghost"; selected: app.toolItalic; tip: "Italic"; implicitWidth: Pal.ctl; onClicked: app.toolItalic = !app.toolItalic }
            }
            RowLayout {
                visible: toolDefaults.usesFill
                Layout.fillWidth: true; spacing: 6
                Text { text: toolDefaults.t === "laser" ? "Color" : "Fill"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
                FColorField { Layout.fillWidth: true; color: app.toolFill; onPicked: (c) => app.toolFill = c }
                FButton { visible: toolDefaults.t !== "laser"; text: "None"; variant: "ghost"; selected: !app.toolFill; tip: "No fill (outline only)"; onClicked: app.toolFill = "" }
            }
            RowLayout {
                visible: toolDefaults.usesStroke
                Layout.fillWidth: true; spacing: 6
                Text { text: toolDefaults.lineTool || toolDefaults.t === "text" ? "Color" : "Stroke"; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 44 }
                FColorField { Layout.fillWidth: true; color: app.toolStroke; onPicked: (c) => app.toolStroke = c }
            }
            FFillSlider {
                visible: toolDefaults.t === "laser"
                Layout.fillWidth: true
                label: "Trail length"
                from: 150; to: 2000; decimals: 0
                value: app.laserTrailMs
                format: (v) => (v / 1000).toFixed(2) + " s"
                onReleased: (v) => app.laserTrailMs = Math.round(v)
            }
            FFillSlider {
                visible: toolDefaults.usesStroke && toolDefaults.t !== "text"
                Layout.fillWidth: true
                label: toolDefaults.lineTool ? "Thickness" : "Stroke width"
                from: 0; to: 48; decimals: 0
                value: app.toolStrokeWidth
                format: (v) => Math.round(v) + " px"
                onReleased: (v) => app.toolStrokeWidth = Math.round(v)
            }
        }

        Caption {
            visible: sv.d.mode === "none" && app.tool === "select"
            Layout.fillWidth: true
            Layout.margins: 24
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: "Select a layer or source to edit it."
        }

        ColumnLayout {
            visible: sv.d.mode === "group" || sv.d.mode === "multi" || (sv.d.mode === "layer" && !sv.d.source)
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.topMargin: 4
            spacing: 2
            Row {
                spacing: 8
                FIcon { visible: sv.d.mode !== "multi"; size: 16; name: sv.d.mode === "group" ? "folder" : (sv.d.sourceIcon || ""); anchors.verticalCenter: parent.verticalCenter }
                Text { text: sv.d.mode === "multi" ? sv.d.count + " layers" : (sv.d.name || ""); color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550; anchors.verticalCenter: parent.verticalCenter }
            }
        }

        SourceSection {
            visible: sv.d.mode === "layer" && !!sv.d.source
            src: sv.d.source || ({})
            topDivider: false
        }

        FSection {
            visible: sv.d.mode === "layer" || sv.d.mode === "group"
            title: "Transform"; group: "transform"; linkedWith: sv.d.linkTransform || ""; sections: sv.d.sections || ({})
            ColumnLayout {
                id: tf
                readonly property bool isLayer: sv.d.mode === "layer"
                property bool lockAspect: app.uiFlag("inspector/lockAspect", true)
                spacing: 6; Layout.fillWidth: true
                RowLayout {
                    visible: tf.isLayer
                    spacing: 4; Layout.fillWidth: true
                    RowLabel { text: "Layer fit" }
                    FButton { iconName: "shrink"; text: "Fit"; variant: "ghost"; onClicked: app.fit("fit"); tip: "Fit inside the canvas, keeping proportions" }
                    FButton { iconName: "expand"; text: "Fill"; variant: "ghost"; onClicked: app.fit("fill"); tip: "Cover the whole canvas, keeping proportions" }
                    FButton { iconName: "ratio"; text: "Original"; variant: "ghost"; onClicked: app.fit("aspect"); tip: "Use the source's proportions" }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    spacing: 6; Layout.fillWidth: true
                    RowLabel { text: "Position" }
                    FNumField { label: "X"; live: true; value: sv.d.x || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("x", v); onScrubbed: (v, f) => app.scrubTransform("x", v, f) }
                    FNumField { label: "Y"; live: true; value: sv.d.y || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("y", v); onScrubbed: (v, f) => app.scrubTransform("y", v, f) }
                    FButton { visible: tf.isLayer; iconName: "crosshair"; variant: "ghost"; tip: "Center on the canvas"; onClicked: app.fit("center") }
                }
                RowLayout {
                    spacing: 6; Layout.fillWidth: true
                    RowLabel { text: "Size" }
                    FNumField { label: "W"; live: true; from: 1; value: sv.d.w || 0; enabled: tf.isLayer; Layout.fillWidth: true; onCommit: (v) => app.setSize("w", v, tf.lockAspect); onScrubbed: (v, f) => app.scrubTransform("w", v, f, tf.lockAspect) }
                    FNumField { label: "H"; live: true; from: 1; value: sv.d.h || 0; enabled: tf.isLayer; Layout.fillWidth: true; onCommit: (v) => app.setSize("h", v, tf.lockAspect); onScrubbed: (v, f) => app.scrubTransform("h", v, f, tf.lockAspect) }
                    FButton {
                        visible: tf.isLayer
                        iconName: tf.lockAspect ? "link-2" : "unlink-2"
                        variant: "ghost"; selected: tf.lockAspect
                        tip: tf.lockAspect ? "Unlock proportions" : "Lock proportions"
                        onClicked: { tf.lockAspect = !tf.lockAspect; app.setUiFlag("inspector/lockAspect", tf.lockAspect) }
                    }
                }
                RowLayout {
                    visible: tf.isLayer
                    spacing: 6; Layout.fillWidth: true
                    RowLabel { text: "Rotate" }
                    FNumField { suffix: "°"; live: true; from: -360; to: 360; value: sv.d.rot || 0; Layout.fillWidth: true; onCommit: (v) => app.setTransform("rot", v); onScrubbed: (v, f) => app.scrubTransform("rot", v, f) }
                    FButton { iconName: "flip-horizontal-2"; variant: "ghost"; selected: !!sv.d.flipH; tip: "Mirror left to right"; onClicked: app.setTransform("flipH", sv.d.flipH ? 0 : 1) }
                    FButton { iconName: "flip-vertical-2"; variant: "ghost"; selected: !!sv.d.flipV; tip: "Mirror top to bottom"; onClicked: app.setTransform("flipV", sv.d.flipV ? 0 : 1) }
                }
            }
        }

        FSection {
            id: cropSec
            readonly property bool croppable: !!sv.d.croppable
            readonly property real va: sv.d.visibleAspect || 0
            readonly property bool cropped: !!(sv.d.cl || sv.d.ct || sv.d.cr || sv.d.cb)
            property bool open: app.uiFlag("inspector/cropOpen", false)
            readonly property var ratios: [0, 16 / 9, 4 / 3, 1, 9 / 16]
            readonly property int ratioIndex: Math.max(0, ratios.findIndex(r => r > 0 && Math.abs(app.cropAspect - r) < 0.001))
            visible: sv.d.mode === "layer"
            title: croppable ? "Crop & corners" : "Corners"; group: "crop"; linkedWith: sv.d.linkCrop || ""; sections: sv.d.sections || ({})
            RowLayout {
                visible: cropSec.croppable
                spacing: 6; Layout.fillWidth: true
                RowLabel { text: "Crop" }
                FButton {
                    Layout.fillWidth: true
                    iconName: "crop"; text: app.cropMode ? "Done" : "Crop"
                    variant: app.cropMode ? "primary" : "secondary"
                    tip: app.cropMode ? "Finish cropping (Enter). Esc cancels." : "Drag an edge on the canvas to crop, or inside to move it. Shortcut: double-click a layer."
                    onClicked: app.cropMode = !app.cropMode
                }
                FButton {
                    variant: "ghost"
                    iconName: cropSec.open ? "chevron-up" : "chevron-down"
                    tip: cropSec.open ? "Hide crop values" : "Show crop values"
                    onClicked: { cropSec.open = !cropSec.open; app.setUiFlag("inspector/cropOpen", cropSec.open) }
                }
            }
            RowLayout {
                visible: cropSec.croppable
                Layout.fillWidth: true; spacing: 6
                RowLabel { text: "Aspect" }
                FTabs { model: ["Free", "16:9", "4:3", "1:1", "9:16"]; currentIndex: cropSec.ratioIndex; onActivated: (i) => app.setCropAspect(cropSec.ratios[i]) }
                Item { Layout.fillWidth: true }
            }
            RowLayout {
                visible: cropSec.croppable && cropSec.open
                spacing: 6; Layout.fillWidth: true
                RowLabel {}
                FNumField { label: "L"; live: true; from: 0; value: sv.d.cl || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("l", v); onScrubbed: (v, f) => app.scrubCrop("l", Math.round(v), f) }
                FNumField { label: "T"; live: true; from: 0; value: sv.d.ct || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("t", v); onScrubbed: (v, f) => app.scrubCrop("t", Math.round(v), f) }
                FNumField { label: "R"; live: true; from: 0; value: sv.d.cr || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("r", v); onScrubbed: (v, f) => app.scrubCrop("r", Math.round(v), f) }
                FNumField { label: "B"; live: true; from: 0; value: sv.d.cb || 0; Layout.fillWidth: true; onCommit: (v) => app.setCrop("b", v); onScrubbed: (v, f) => app.scrubCrop("b", Math.round(v), f) }
                FButton { iconName: "rotate-ccw"; variant: "ghost"; enabled: cropSec.cropped; tip: "Remove the crop"; onClicked: app.cropToAspect(0) }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                RowLabel { text: "Corners" }
                FFillSlider {
                    Layout.fillWidth: true
                    label: ""
                    from: 0; to: 50; decimals: 0
                    value: sv.d.radius || 0
                    format: (v) => v <= 0.5 ? "Square" : v >= 49.5 ? "Round" : Math.round(v) + "%"
                    onMoved: (v) => app.setCornerRadius(v, false)
                    onReleased: (v) => app.setCornerRadius(v, true)
                }
            }
        }

        FSection {
            visible: sv.d.mode === "layer"
            title: "Filters"; group: "filters"; linkedWith: sv.d.linkFilters || ""; sections: sv.d.sections || ({})
            Repeater {
                id: filterCards
                readonly property int shown: (sv.d.filters || []).length
                property int built: 0
                onShownChanged: built = Math.max(built, shown)
                Component.onCompleted: built = shown
                model: built
                delegate: Rectangle {
                    id: card
                    required property int index
                    property var modelData: ({ enabled: false, label: "", params: [] })
                    readonly property var live: (sv.d.filters || [])[index]
                    onLiveChanged: if (live) modelData = live
                    Component.onCompleted: if (live) modelData = live
                    visible: index < filterCards.shown
                    Layout.fillWidth: true
                    implicitHeight: fcol.implicitHeight + 16
                    radius: Pal.radius + 2
                    color: Pal.s3
                    border.width: 1
                    border.color: Pal.border
                    ColumnLayout {
                        id: fcol
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            FSwitch { checked: card.modelData.enabled; onClicked: app.toggleFilter(card.index) }
                            Body { text: card.modelData.label; font.weight: 550; Layout.fillWidth: true; Layout.leftMargin: 4 }
                            FButton { iconName: "trash-2"; variant: "ghost"; onClicked: app.removeFilter(card.index); tip: "Remove filter" }
                        }
                        ObsProperties {
                            visible: !!card.modelData.obs && !!card.modelData.loaded && card.visible
                            active: visible
                            scope: "filter"; targetId: card.modelData.layer || ""; index: card.index
                        }
                        Caption {
                            visible: !!card.modelData.obs && !card.modelData.loaded
                            Layout.fillWidth: true; wrapMode: Text.WordWrap
                            text: "Its plugin isn't loaded, so this filter does nothing. Add the plugin in Settings > Plugins."
                        }
                        Repeater {
                            id: paramRows
                            readonly property int shown: card.modelData.params.length
                            property int built: 0
                            onShownChanged: built = Math.max(built, shown)
                            Component.onCompleted: built = shown
                            model: built
                            delegate: Loader {
                                required property int index
                                Layout.fillWidth: true
                                property var p: ({ key: "", kind: "slider", value: 0, visible: false })
                                readonly property var live: card.modelData.params[index]
                                onLiveChanged: if (live) p = live
                                Component.onCompleted: if (live) p = live
                                visible: index < paramRows.shown && p.visible
                                active: p.visible
                                sourceComponent: p.kind === "color" ? colorParam : p.kind === "choice" ? choiceParam : p.kind === "file" ? fileParam
                                               : p.kind === "toggle" ? toggleParam : numParam
                                Component {
                                    id: numParam
                                    FFillSlider {
                                        label: p.label
                                        from: p.min; to: p.max; decimals: p.decimals; displayScale: p.scale; suffix: p.suffix
                                        value: Number(p.value)
                                        onMoved: (v) => app.setFilterValue(card.index, p.key, v, false)
                                        onReleased: (v) => app.setFilterValue(card.index, p.key, v, true)
                                    }
                                }
                                Component {
                                    id: colorParam
                                    RowLayout {
                                        spacing: 6
                                        Text { text: p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.fillWidth: true }
                                        FColorField { color: String(p.value); onPicked: (c) => app.setFilterValue(card.index, p.key, c, true) }
                                    }
                                }
                                Component {
                                    id: choiceParam
                                    RowLayout {
                                        spacing: 6
                                        Text { text: p.label; color: Pal.muted; font.family: Pal.font; font.pixelSize: Pal.caption; Layout.preferredWidth: 90 }
                                        FSelect {
                                            Layout.fillWidth: true
                                            options: p.options
                                            currentValue: String(p.value)
                                            placeholder: String(p.value)
                                            onPicked: (v) => app.setFilterValue(card.index, p.key, v, true)
                                        }
                                    }
                                }
                                Component {
                                    id: fileParam
                                    RowLayout {
                                        spacing: 6
                                        FInput {
                                            Layout.fillWidth: true
                                            placeholderText: p.label
                                            text: String(p.value)
                                            onEditingFinished: if (text !== String(p.value)) app.setFilterValue(card.index, p.key, text, true)
                                        }
                                        FButton {
                                            iconName: "folder-open"; variant: "ghost"; tip: "Choose " + p.label.toLowerCase()
                                            onClicked: filterFilePicker.pick(card.index, p.key, p.label, p.fileFilter)
                                        }
                                    }
                                }
                                Component {
                                    id: toggleParam
                                    FSwitch { text: p.label; checkable: false; checked: !!p.value; onClicked: app.setFilterValue(card.index, p.key, !p.value, true) }
                                }
                            }
                        }
                    }
                }
            }
            FSelect {
                Layout.fillWidth: true
                placeholder: "Add filter…"
                options: app.filterTypes()
                onPicked: (v) => app.addFilter(v)
            }
            FileDialog {
                id: filterFilePicker
                property int filterIndex: -1
                property string key
                function pick(index, k, label, filter) {
                    filterIndex = index; key = k; title = label
                    nameFilters = [filter, "All files (*)"]
                    open()
                }
                onAccepted: app.setFilterValue(filterIndex, key, decodeURIComponent(selectedFile.toString().replace(/^file:\/{3}/, "")), true)
            }
        }

        FSection {
            visible: sv.d.mode === "layer" || sv.d.mode === "group" || sv.d.mode === "multi"
            title: "Visibility"; group: "visibility"; linkedWith: sv.d.linkVisibility || ""; sections: sv.d.sections || ({})
            FSwitch { text: "Visible"; checked: sv.d.visible !== false; onClicked: app.setVisibleFlag(checked) }
            FFillSlider {
                Layout.fillWidth: true
                label: "Opacity"
                value: sv.d.opacity !== undefined ? sv.d.opacity : 1
                decimals: 0; displayScale: 100; suffix: "%"
                onMoved: (v) => app.setOpacity(v, false)
                onReleased: (v) => app.setOpacity(v, true)
            }
        }

        ColumnLayout {
            visible: sv.d.mode === "source"
            Layout.fillWidth: true
            Layout.margins: 12
            Layout.topMargin: 4
            spacing: 8
            Row { spacing: 8
                FIcon { size: 16; name: sv.d.icon || ""; anchors.verticalCenter: parent.verticalCenter }
                Text { text: sv.d.name || ""; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.title; font.weight: 550; anchors.verticalCenter: parent.verticalCenter } }
            SourceFields { d: sv.d; active: sv.d.mode === "source" }
            AudioSettings {
                readonly property var entry: sv.d.mode === "source" ? app.mixer.find(m => m.id === sv.d.id) : undefined
                visible: !!entry
                s: entry || ({ id: "", name: "", icon: "", volume: 0, muted: false, monitor: "off", tracks: [], global: true, warnings: [], fx: [], mono: false, balance: 0.5, syncMs: 0 })
                Layout.fillWidth: true; Layout.topMargin: 8
            }
            Text { visible: !!sv.d.video; text: "Used in"; color: Pal.fg; font.family: Pal.font; font.pixelSize: Pal.subtitle; font.weight: 550; Layout.topMargin: 8 }
            Repeater {
                model: sv.d.video ? (sv.d.usedIn || []) : []
                delegate: FButton { required property var modelData; text: modelData.scene; variant: "ghost"; onClicked: { app.editScene(modelData.sceneId); app.selectLayer(modelData.instId, false) } }
            }
            Caption { visible: !!sv.d.video && (sv.d.usedIn || []).length === 0; text: "Not in any scene. Drag it onto the canvas."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }
        Item { Layout.fillHeight: true }
    }
}
