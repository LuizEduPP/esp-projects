#!/usr/bin/env bash
set -euo pipefail

PIO_ROOT="${HOME}/.platformio/penv/lib"
RULES_SRC="$(find "$PIO_ROOT" -path '*/platformio/assets/system/99-platformio-udev.rules' 2>/dev/null | head -1)"

if [[ -z "$RULES_SRC" || ! -f "$RULES_SRC" ]]; then
  echo "99-platformio-udev.rules not found." >&2
  echo "Install the PlatformIO IDE extension first." >&2
  exit 1
fi

echo "Installing udev rules..."
sudo cp "$RULES_SRC" /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Loading USB serial driver (cdc_acm)..."
sudo modprobe cdc_acm

if ! groups "$USER" | grep -q '\bdialout\b'; then
  echo "Adding $USER to dialout group..."
  sudo usermod -aG dialout "$USER"
  echo "Log out and back in (or reboot) for the dialout group to apply."
fi

echo ""
echo "Serial ports (TTL):"
ls -la /dev/ttyUSB* /dev/serial/by-id/* 2>/dev/null | grep -v Espressif || echo "  (none — plug the USB-TTL adapter)"
