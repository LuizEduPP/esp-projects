#!/usr/bin/env bash
set -euo pipefail

if command -v pio >/dev/null 2>&1; then
  exec pio "$@"
fi

PIO="${HOME}/.platformio/penv/bin/pio"
if [[ -x "$PIO" ]]; then
  exec "$PIO" "$@"
fi

echo "PlatformIO not found. Install the PlatformIO IDE extension or:" >&2
echo "  python3 -m pip install -U platformio" >&2
exit 127
