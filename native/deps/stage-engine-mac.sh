#!/bin/bash
# Stages the OBS engine for LOBS.app from OBS's official macOS release into deps/engine-mac:
# libobs.framework, its dylibs and the browser source's CEF framework and helpers (Frameworks),
# the modules listed in engine-modules-mac.txt (PlugIns) and libobs's helper programs, such as
# the recording muxer, which it starts from the app's own folder (MacOS). OBS's Qt is left out: LOBS
# ships its own Qt (the same 6.11.1), so only one Qt is ever loaded.
set -euo pipefail
cd "$(dirname "$0")"
OBS_VERSION=${OBS_VERSION:-32.2.2}
DMG=downloads/obs/OBS-Studio-$OBS_VERSION-macOS-Apple.dmg
OUT=engine-mac
[ -f "$DMG" ] || { echo "Missing $DMG (download it from the OBS $OBS_VERSION release)"; exit 1; }
MNT=$(mktemp -d)
hdiutil attach -nobrowse -readonly -mountpoint "$MNT" "$DMG" >/dev/null
trap 'hdiutil detach "$MNT" >/dev/null' EXIT
SRC="$MNT/OBS.app/Contents"
rm -rf "$OUT" && mkdir -p "$OUT/Frameworks" "$OUT/PlugIns" "$OUT/MacOS"
for f in "$SRC/Frameworks/"*; do
	case "$(basename "$f")" in
	Qt*.framework | Sparkle.framework | Syphon.framework | obs-scripting.dylib) ;;
	*) ditto "$f" "$OUT/Frameworks/$(basename "$f")" ;;
	esac
done
# CEF finds its helper apps by the main app's name ("<App> Helper (GPU).app" and so on), so
# OBS's helpers become LOBS's: renamed, with bundle ids under LOBS's.
for sfx in "" " (GPU)" " (Renderer)" " (Plugin)"; do
	o="OBS Helper$sfx" n="LOBS Helper$sfx"
	mv "$OUT/Frameworks/$o.app" "$OUT/Frameworks/$n.app"
	mv "$OUT/Frameworks/$n.app/Contents/MacOS/$o" "$OUT/Frameworks/$n.app/Contents/MacOS/$n"
	plist="$OUT/Frameworks/$n.app/Contents/Info.plist"
	id=$(plutil -extract CFBundleIdentifier raw "$plist")
	plutil -replace CFBundleExecutable -string "$n" "$plist"
	plutil -replace CFBundleName -string "$n" "$plist"
	plutil -replace CFBundleIdentifier -string "${id/com.obsproject.obs-studio/com.loganaffleck.lobs}" "$plist"
done
for f in "$SRC/MacOS/"*; do
	[ "$(basename "$f")" = OBS ] || ditto "$f" "$OUT/MacOS/$(basename "$f")"
done
grep -v '^#' ../engine-modules-mac.txt | while read -r m; do
	[ -n "$m" ] && ditto "$SRC/PlugIns/$m.plugin" "$OUT/PlugIns/$m.plugin"
done
echo "$OBS_VERSION" > "$OUT/version.txt"
echo "ENGINE OK ($OBS_VERSION)"
