#!/bin/sh
# Regenerate Font Awesome 6 Solid subset for router LVGL UI.
set -eu

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/.." && pwd)"
ASSETS="$DIR/assets"
OUT="$ROOT/src/router/router_icon_font_16.c"
FONT="$ASSETS/fa-solid-900.ttf"

mkdir -p "$ASSETS"
if [ ! -f "$FONT" ]; then
	curl -fsSL -o "$FONT" \
		"https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.x/webfonts/fa-solid-900.ttf"
fi

if [ ! -d "$DIR/node_modules/lv_font_conv" ]; then
	( cd "$DIR" && npm init -y >/dev/null 2>&1 && npm install lv_font_conv --no-save )
fi

node "$DIR/node_modules/lv_font_conv/lv_font_conv.js" \
	--font "$FONT" \
	--size 16 \
	--bpp 4 \
	--no-compress \
	--format lvgl \
	--force-fast-kern-format \
	-r 0xf021,0xf0a0,0xf0c0,0xf132,0xf1c0,0xf1eb,0xf2db,0xf3ed,0xf6ff,0xf796,0xf7c2 \
	-o "$OUT" \
	--lv-font-name router_icon_font_16

# esp32-smartdisplay / Arduino uses simple LVGL include path
sed -i '' 's|"lvgl/lvgl.h"|"lvgl.h"|g' "$OUT" 2>/dev/null || \
	sed -i 's|"lvgl/lvgl.h"|"lvgl.h"|g' "$OUT"

echo "Wrote $OUT"
