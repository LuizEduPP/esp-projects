#!/usr/bin/env bash
# Renders the firmware's real LVGL screens on the host, so layout can be
# checked without flashing the board.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
fw="$here/../firmware"
lvgl="$fw/.pio/libdeps/spaceman-c3/lvgl"
out="$here/.build"

if [ ! -d "$lvgl" ]; then
  echo "LVGL not found at $lvgl — run 'yarn dash:build' first." >&2
  exit 1
fi

flags=(
  -DLV_CONF_SKIP=1
  -DLV_COLOR_DEPTH=16
  -DLV_USE_LOG=0
  -DLV_MEM_SIZE=1048576
  -DLV_DEF_REFR_PERIOD=20
  -DLV_FONT_MONTSERRAT_10=1
  -DLV_FONT_MONTSERRAT_12=1
  -DLV_FONT_MONTSERRAT_14=1
  -DLV_FONT_MONTSERRAT_16=1
  -DLV_FONT_MONTSERRAT_20=1
  -DLV_FONT_MONTSERRAT_28=1
  -DLV_USE_CHART=1
  -I"$lvgl"
  -I"$lvgl/src"
  -I"$fw/include"
)

mkdir -p "$out"

if [ ! -f "$out/liblvgl.a" ]; then
  echo "building lvgl (first run only)..."
  mapfile -t sources < <(find "$lvgl/src" -name '*.c')
  for src in "${sources[@]}"; do
    obj="$out/$(echo "${src#$lvgl/src/}" | tr '/' '_').o"
    if [ ! -f "$obj" ]; then
      gcc -Os -w "${flags[@]}" -c "$src" -o "$obj" &
      while [ "$(jobs -r | wc -l)" -ge "$(nproc)" ]; do wait -n; done
    fi
  done
  wait
  ar rcs "$out/liblvgl.a" "$out"/*.o
fi

g++ -Os -w "${flags[@]}" \
  "$here/sim_main.cpp" "$fw/src/screens.cpp" \
  "$out/liblvgl.a" -lm -o "$out/uisim"

"$out/uisim" "$out"
python3 "$here/sheet.py" "$out" page
python3 "$here/sheet.py" "$out" stress
