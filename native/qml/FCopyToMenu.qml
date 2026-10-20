import QtQuick

// "Copy to ▸ <scene> ▸ Linked / Independent" for the selected layers (LOBS-020).
FMenu {
    id: m
    title: "Copy to"
    enabled: app.scenes.length > 1 && app.layers.some(l => l.selected)
    Instantiator {
        active: m.populated // built while the menu is open
        model: app.scenes
        delegate: FMenu {
            required property var modelData
            title: modelData.name
            enabled: modelData.id !== app.editSceneId
            FMenuItem { text: "Linked copy"; onTriggered: app.copySelectedTo(modelData.id, true) }
            FMenuItem { text: "Independent copy"; onTriggered: app.copySelectedTo(modelData.id, false) }
        }
        onObjectAdded: (i, o) => m.insertMenu(i, o)
        onObjectRemoved: (i, o) => m.removeMenu(o)
    }
}
