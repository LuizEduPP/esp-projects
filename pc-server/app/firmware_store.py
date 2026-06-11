import json
import re
from pathlib import Path
from typing import Any

FIRMWARE_DIR = Path(__file__).resolve().parent.parent / "firmware"
MANIFEST_PATH = FIRMWARE_DIR / "manifest.json"


def load_manifest() -> dict[str, Any]:
    if not MANIFEST_PATH.is_file():
        return {}
    return json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))


def _parse_version(version: str) -> tuple[int, ...]:
    parts = re.findall(r"\d+", version)
    return tuple(int(p) for p in parts) if parts else (0,)


def version_newer(offered: str, current: str) -> bool:
    return _parse_version(offered) > _parse_version(current)


def firmware_info(role: str, current_version: str, base_url: str = "") -> dict[str, Any] | None:
    manifest = load_manifest()
    entry = manifest.get(role)
    if not entry:
        return None

    offered = str(entry.get("version", ""))
    rel_path = str(entry.get("file", f"{role}/firmware.bin"))
    bin_path = FIRMWARE_DIR / rel_path
    if not offered or not bin_path.is_file():
        return None
    if not version_newer(offered, current_version):
        return None

    url = f"{base_url.rstrip('/')}/api/v1/firmware/{role}/firmware.bin" if base_url else f"/api/v1/firmware/{role}/firmware.bin"
    return {
        "update": True,
        "version": offered,
        "url": url,
        "size": bin_path.stat().st_size,
    }


def firmware_binary_path(role: str) -> Path | None:
    manifest = load_manifest()
    entry = manifest.get(role)
    if not entry:
        return None
    path = FIRMWARE_DIR / str(entry.get("file", f"{role}/firmware.bin"))
    return path if path.is_file() else None
