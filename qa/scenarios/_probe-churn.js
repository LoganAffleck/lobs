// Diagnostic (LOBS-022): which building block leaks when created and destroyed? MB per 1000.
t.include("_lib.js");
skipSetup();
t.setTimeout(300);
story("probe-churn", "create/destroy building blocks", () => {
	const cases = {
		"Item": "Item {}",
		"Rectangle": "Rectangle { width: 10; height: 10 }",
		"Text": "Text { text: 'hello' }",
		"MouseArea": "MouseArea { width: 10; height: 10 }",
		"FIcon": "FIcon { name: 'eye' }",
		"FButton text": "FButton { text: 'Save' }",
		"FButton icon": "FButton { iconName: 'eye' }",
		"FButton tip": "FButton { iconName: 'eye'; tip: 'Hide' }",
		"FBadge": "FBadge { text: '2 fx' }",
		"Behavior": "Item { Behavior on opacity { NumberAnimation { duration: 80 } } }",
		"Image provider": "Image { source: 'image://icon/eye?c=ffffff&w=2'; sourceSize: Qt.size(16,16) }",
		"Pal binding": "Rectangle { color: Pal.fg; radius: Pal.radius }",
		"app binding": "Item { property var x: app.layers }",
	};
	Object.keys(cases).forEach(k => t.metric(k, Math.round(t.churn(cases[k], 1000) * 10) / 10));
	check("probe ran", true);
});
