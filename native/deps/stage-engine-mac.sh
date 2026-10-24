#!/bin/bash
# Stages the OBS engine for LOBS.app from OBS's official macOS release into deps/engine-mac:
# libobs.framework, its dylibs and the browser source's CEF framework and helpers (Frameworks),
# and the modules listed in engine-modules-mac.txt (PlugIns). OBS's Qt is left out: LOBS
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
rm -rf "$OUT" && mkdir -p "$OUT/Frameworks" "$OUT/PlugIns"
for f in "$SRC/Frameworks/"*; do
	case "$(basename "$f")" in
	Qt*.framework | Sparkle.framework | Syphon.framework | obs-scripting.dylib) ;;
	*) ditto "$f" "$OUT/Frameworks/$(basename "$f")" ;;
	esac
done
grep -v '^#' ../engine-modules-mac.txt | while read -r m; do
	[ -n "$m" ] && ditto "$SRC/PlugIns/$m.plugin" "$OUT/PlugIns/$m.plugin"
done
echo "$OBS_VERSION" > "$OUT/version.txt"
echo "ENGINE OK ($OBS_VERSION)"
