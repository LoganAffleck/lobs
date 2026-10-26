#!/bin/bash
# Builds KDDockWidgets 2.4.1 (QtQuick frontend) into deps/kddw (macOS; build-kddw.bat on Windows).
set -euo pipefail
cd "$(dirname "$0")"
QTDIR=${QTDIR:-$HOME/Qt/6.11.1/macos}
# LOBS restyle patches (drop targets + landing preview), applied over the sources
cp kddw-patches/*.qml KDDockWidgets/src/qtquick/views/qml/
# Tab reorder: QtQuick tab bars get moveTabTo (applied once; skipped if it already is)
git -C KDDockWidgets apply --reverse --check ../kddw-patches/tabbar-move.patch 2>/dev/null || git -C KDDockWidgets apply ../kddw-patches/tabbar-move.patch
cmake -S KDDockWidgets -B kddw-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QTDIR" \
	-DCMAKE_INSTALL_PREFIX="$PWD/kddw" -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DKDDockWidgets_QT6=ON \
	-DKDDockWidgets_FRONTENDS=qtquick -DKDDockWidgets_EXAMPLES=OFF -DKDDockWidgets_TESTS=OFF -DKDDockWidgets_DOCS=OFF
cmake --build kddw-build
cmake --install kddw-build
echo KDDW OK
