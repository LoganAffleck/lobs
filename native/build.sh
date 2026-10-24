#!/bin/bash
# Configure, build and assemble dist/mac/LOBS.app (macOS; build.bat on Windows).
# The OBS engine comes from OBS's official macOS release (deps/stage-engine-mac.sh) and goes
# inside the app: libobs and its dylibs in Contents/Frameworks, modules in Contents/PlugIns.
set -euo pipefail
cd "$(dirname "$0")"
QTDIR=${QTDIR:-$HOME/Qt/6.11.1/macos}
APP=dist/mac/LOBS.app
[ -d deps/engine-mac ] || deps/stage-engine-mac.sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QTDIR" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
rm -f "$APP/Contents/MacOS/LOBS" # macdeployqt rewrites it below, so it must be linked fresh each time
cmake --build build
# Qt and the QML modules, into a bundle without the engine: macdeployqt would take OBS's
# module bundles in Contents/PlugIns for Qt plugins. Resources goes too: macdeployqt skips QML
# it finds already there, and its links into PlugIns would point nowhere.
rm -rf "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" "$APP/Contents/Resources"
"$QTDIR/bin/macdeployqt" "$APP" -qmldir=qml -libpath=deps/kddw/lib -libpath=deps/whisper/lib -no-codesign
rm -rf "$APP/Contents/PlugIns/sqldrivers" # Qt's database drivers: LOBS has no database
# KDDockWidgets and whisper.cpp (live captions), by the names LOBS links them as; macdeployqt
# leaves non-Qt @rpath libraries out.
cp -L deps/kddw/lib/libkddockwidgets-qt6.3.dylib deps/whisper/lib/libwhisper.1.dylib deps/whisper/lib/libggml*.dylib \
	"$APP/Contents/Frameworks/"
# obs-browser links Qt Widgets; ship ours so only one Qt is loaded.
rsync -a --exclude Headers "$QTDIR/lib/QtWidgets.framework" "$APP/Contents/Frameworks/"
# Bundled engine: OBS's own release build, only the modules in engine-modules-mac.txt.
rsync -a deps/engine-mac/PlugIns/ "$APP/Contents/PlugIns/"
rsync -a deps/engine-mac/Frameworks/ "$APP/Contents/Frameworks/"
cp engine-modules-mac.txt "$APP/Contents/Resources/engine-modules.txt"
# License and notices ship with the app
cp ../LICENSE "$APP/Contents/Resources/LICENSE.txt"
cp ../THIRD_PARTY_NOTICES.md "$APP/Contents/Resources/"
mkdir -p "$APP/Contents/Resources/fonts" "$APP/Contents/Resources/icons"
cp fonts/* "$APP/Contents/Resources/fonts/"
cp icons/* resources/icon/lobs-*.png "$APP/Contents/Resources/icons/"
# App icon, rendered from the SVG by LOBS itself (--render-svg runs before the engine loads)
if [ ! -f build/AppIcon.icns ] || [ resources/lobs-icon.svg -nt build/AppIcon.icns ]; then
	ICONSET=build/AppIcon.iconset
	mkdir -p "$ICONSET"
	for s in 16 32 128 256 512; do
		"$APP/Contents/MacOS/LOBS" --render-svg resources/lobs-icon.svg "$ICONSET/icon_${s}x${s}.png" $s
		"$APP/Contents/MacOS/LOBS" --render-svg resources/lobs-icon.svg "$ICONSET/icon_${s}x${s}@2x.png" $((s * 2))
	done
	iconutil -c icns -o build/AppIcon.icns "$ICONSET"
fi
cp build/AppIcon.icns "$APP/Contents/Resources/"
# Ad-hoc signature for running on this Mac. Release builds are signed with a Developer ID and
# notarized instead (PLAN-MACOS.md, Phase 4).
codesign --force --deep --sign - "$APP" 2>&1 | grep -v "replacing existing signature" || true
echo BUILD OK
