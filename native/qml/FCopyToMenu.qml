import QtQuick

// "Copy to ▸ <scene> ▸ Linked / Independent" for the selected layers (LOBS-020).
FMenu {
    id: m
    title: "Copy to"
    enabled: app.scenes.length > 1 && app.layers.some(l => l.selected)
    // A choice in a scene menu doesn't close the menu this one sits in (the layer row menu stayed
    // open): whoever holds it closes it on this.
    signal copied()
    Instantiator {
        active: m.populated // built while the menu is open
        model: app.scenes
        delegate: FMenu {
            required property var modelData
            title: modelData.name
            enabled: modelData.id !== app.editSceneId
            // Close the menus first: the copy changes the scene list, which rebuilds these scene
            // menus and ends this handler before anything after it runs.
            FMenuItem { text: "Linked copy"; onTriggered: { const id = modelData.id; m.copied(); app.copySelectedTo(id, true) } }
            FMenuItem { text: "Independent copy"; onTriggered: { const id = modelData.id; m.copied(); app.copySelectedTo(id, false) } }
        }
        onObjectAdded: (i, o) => m.insertMenu(i, o)
        onObjectRemoved: (i, o) => m.removeMenu(o)
    }
}
