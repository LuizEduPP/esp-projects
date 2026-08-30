#!/usr/bin/env bash
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bin="$here/firmware/.pio/build/spaceman-c3/firmware.bin"
api="${DASH_API_URL:-https://dash-esp32.kmali.online}"

[ -f "$bin" ] || { echo "compile antes: yarn dash:build"; exit 1; }

version="$(grep -o 'DASH_FW_VERSION=\\"[^\\]*' "$here/firmware/platformio.ini" | head -1 | sed 's/.*\\"//')"
[ -n "$version" ] || { echo "DASH_FW_VERSION nao encontrada no platformio.ini"; exit 1; }

echo "publicando $version ($(stat -c%s "$bin") bytes) em $api"

curl -fsS -X POST "$api/firmware?version=$version" \
  -H "content-type: application/octet-stream" \
  -H "x-ota-token: ${DASH_OTA_TOKEN:-}" \
  --data-binary "@$bin"

echo
