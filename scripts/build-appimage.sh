#!/usr/bin/env bash
set -euo pipefail

APP=PatternShelf
BIN=patternshelf
ARCH=x86_64

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOLS="$ROOT/tools"
DIST="$ROOT/dist"
APPDIR="$ROOT/AppDir"
BUILD="$ROOT/build-appimage"

LINUXDEPLOY="$TOOLS/linuxdeploy-${ARCH}.AppImage"
QT_PLUGIN="$TOOLS/linuxdeploy-plugin-qt-${ARCH}.AppImage"

mkdir -p "$TOOLS" "$DIST"

if [ ! -f "$LINUXDEPLOY" ]; then
    echo "Downloading linuxdeploy..."
    wget -O "$LINUXDEPLOY" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
    chmod +x "$LINUXDEPLOY"
fi

if [ ! -f "$QT_PLUGIN" ]; then
    echo "Downloading linuxdeploy Qt plugin..."
    wget -O "$QT_PLUGIN" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"
    chmod +x "$QT_PLUGIN"
fi

echo "Cleaning old AppImage build..."
rm -rf "$APPDIR" "$BUILD"
mkdir -p "$APPDIR"

echo "Building PatternShelf..."
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr

cmake --build "$BUILD"

echo "Installing into AppDir..."
DESTDIR="$APPDIR" cmake --install "$BUILD"

echo "Building AppImage..."
cd "$ROOT"

export QMAKE=/usr/bin/qmake6
export OUTPUT="$DIST/${APP}-${ARCH}.AppImage"

# Workaround for linuxdeploy's bundled strip failing on modern Arch RELR binaries.
export NO_STRIP=true

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --plugin qt \
    --output appimage

echo
echo "Done:"
ls -lh "$DIST/${APP}-${ARCH}.AppImage"
