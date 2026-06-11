#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION_FILE="$ROOT/firmware/VERSION"

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "missing $VERSION_FILE" >&2
  exit 1
fi

VERSION="$(tr -d ' \n\r\t' < "$VERSION_FILE")"

gen_header() {
  local dir="$1"
  cat > "$dir/include/firmware_version.h" <<EOF
#pragma once
#define FW_VERSION "$VERSION"
EOF
}

gen_header "$ROOT/esp32-cyd/uart-peer/firmware"
gen_header "$ROOT/goouuu-esp32-s3-cam/uart-peer/firmware"

mkdir -p "$ROOT/pc-server/firmware/cyd" "$ROOT/pc-server/firmware/s3"

cat > "$ROOT/pc-server/firmware/manifest.json" <<EOF
{
  "cyd": {
    "version": "$VERSION",
    "file": "cyd/firmware.bin"
  },
  "s3": {
    "version": "$VERSION",
    "file": "s3/firmware.bin"
  }
}
EOF

echo "firmware version: $VERSION"

if [[ "${1:-}" == "--build" ]]; then
  yarn --cwd "$ROOT/esp32-cyd/uart-peer" fw:build
  yarn --cwd "$ROOT/goouuu-esp32-s3-cam/uart-peer" fw:build
fi

CYD_BIN="$ROOT/esp32-cyd/uart-peer/firmware/.pio/build/cyd/firmware.bin"
S3_BIN="$ROOT/goouuu-esp32-s3-cam/uart-peer/firmware/.pio/build/goouuu-s3-cam/firmware.bin"

copy_bin() {
  local src="$1"
  local dest="$2"
  cp "$src" "$dest.tmp"
  mv "$dest.tmp" "$dest"
}

if [[ -f "$CYD_BIN" ]]; then
  copy_bin "$CYD_BIN" "$ROOT/pc-server/firmware/cyd/firmware.bin"
  echo "copied cyd -> pc-server/firmware/cyd/firmware.bin ($(wc -c < "$ROOT/pc-server/firmware/cyd/firmware.bin") bytes)"
else
  echo "warn: cyd bin not found ($CYD_BIN)" >&2
fi

if [[ -f "$S3_BIN" ]]; then
  copy_bin "$S3_BIN" "$ROOT/pc-server/firmware/s3/firmware.bin"
  echo "copied s3 -> pc-server/firmware/s3/firmware.bin ($(wc -c < "$ROOT/pc-server/firmware/s3/firmware.bin") bytes)"
else
  echo "warn: s3 bin not found ($S3_BIN)" >&2
fi

echo "manifest: pc-server/firmware/manifest.json"
