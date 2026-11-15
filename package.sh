#!/usr/bin/env bash
set -euo pipefail

export PS3DEV="${PS3DEV:-/mnt/d/ps3dev-sdk}"
export PSL1GHT="${PSL1GHT:-$PS3DEV}"
export PORTLIBS="${PORTLIBS:-$PS3DEV/portlibs/ppu}"
export PATH="$PS3DEV/bin:$PS3DEV/ppu/bin:$PATH"

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

TITLE_ID="CNBLPS300"
CONTENT_ID="UP0001-CNBLPS300_00-0000000000000000"
PKG_DIR="$ROOT/pkg_staging/$TITLE_ID"
DIST_DIR="$ROOT/dist"

if [ ! -f "$ROOT/cannonball_ps3.elf" ]; then
    "$ROOT/build.sh"
fi

if [ ! -f "$ROOT/assets/ICON0.PNG" ] || [ ! -f "$ROOT/assets/PIC1.PNG" ]; then
    python3 "$ROOT/assets/make_assets.py"
fi

rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/USRDIR/res" "$PKG_DIR/USRDIR/docs" "$DIST_DIR"

cp -f "$ROOT/cannonball_ps3.elf" "$PKG_DIR/USRDIR/cannonball_ps3.elf"
ppu-strip "$PKG_DIR/USRDIR/cannonball_ps3.elf"
sprxlinker "$PKG_DIR/USRDIR/cannonball_ps3.elf"
make_self_npdrm "$PKG_DIR/USRDIR/cannonball_ps3.elf" "$PKG_DIR/USRDIR/EBOOT.BIN" "$CONTENT_ID"
rm -f "$PKG_DIR/USRDIR/cannonball_ps3.elf"

sfo -f "$ROOT/sfo.xml" "$PKG_DIR/PARAM.SFO" --title="Cannonball PS3" --appid="$TITLE_ID"

cp -f "$ROOT/assets/ICON0.PNG" "$PKG_DIR/ICON0.PNG"
cp -f "$ROOT/assets/PIC1.PNG" "$PKG_DIR/PIC1.PNG"

cp -f "$ROOT/res/tilemap.bin" "$PKG_DIR/USRDIR/res/tilemap.bin"
cp -f "$ROOT/res/tilepatch.bin" "$PKG_DIR/USRDIR/res/tilepatch.bin"
cp -f "$ROOT/res/font.ttf" "$PKG_DIR/USRDIR/res/font.ttf"
cp -f "$ROOT/res/config.xml" "$PKG_DIR/USRDIR/res/config.xml"

if [ -f "$ROOT/docs/license.txt" ]; then
    cp -f "$ROOT/docs/license.txt" "$PKG_DIR/USRDIR/docs/license.txt"
fi
if [ -f "$ROOT/README.md" ]; then
    cp -f "$ROOT/README.md" "$PKG_DIR/USRDIR/README.md"
fi
if [ -f "$ROOT/CREDITS.md" ]; then
    cp -f "$ROOT/CREDITS.md" "$PKG_DIR/USRDIR/CREDITS.md"
fi

pkg --contentid="$CONTENT_ID" "$PKG_DIR/" "$DIST_DIR/Cannonball_PS3.pkg"
cp -f "$DIST_DIR/Cannonball_PS3.pkg" "$DIST_DIR/Cannonball_PS3.gnpdrm.pkg"
package_finalize "$DIST_DIR/Cannonball_PS3.gnpdrm.pkg"

ls -lh "$DIST_DIR/Cannonball_PS3.pkg" "$DIST_DIR/Cannonball_PS3.gnpdrm.pkg"
