#!/usr/bin/env python3
"""Rasteriza icones de mock/ui-icons-sheet.svg -> PNG + mascara alpha."""
from __future__ import annotations

import io
import os
import re

import cairosvg
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEET = os.path.join(ROOT, "mock", "ui-icons-sheet.svg")
OUT_PNG = os.path.join(ROOT, "firmware", "assets", "icons")
OUT_H = os.path.join(ROOT, "firmware", "include", "ui_icon_data.h")
RASTER = 128
OUT_SIZE = 32
ALPHA_CUT = 28

# Ordem = UiIcon enum em ui_icons.h
ICON_IDS = [
    "ico-gb", "ico-sd", "ico-cart", "ico-folder", "ico-gear", "ico-grid",
    "ico-chev-l", "ico-chev-r", "ico-pause", "ico-play", "ico-save", "ico-load",
    "ico-sliders", "ico-target", "ico-exit", "ico-palette", "ico-speed", "ico-sun",
    "ico-globe", "ico-check", "ico-x", "ico-select", "ico-cross",
]


def load_defs(text: str) -> str:
    m = re.search(r"<defs>.*?</defs>", text, re.S)
    if not m:
        raise SystemExit(f"Sem <defs> em {SHEET}")
    return m.group(0)


def symbol_svg(defs: str, icon_id: str) -> str:
    # <use> herda stroke/fill do <symbol> — igual ao preview no sheet
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">\n'
        f"{defs}\n"
        f'<use href="#{icon_id}" width="24" height="24"/>\n'
        "</svg>"
    )


def rasterize(svg: str) -> Image.Image:
    png = cairosvg.svg2png(bytestring=svg.encode("utf-8"), output_width=RASTER, output_height=RASTER)
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    return img.resize((OUT_SIZE, OUT_SIZE), Image.Resampling.LANCZOS)


def alpha_bytes(img: Image.Image) -> list[int]:
    px = img.load()
    out: list[int] = []
    for y in range(OUT_SIZE):
        for x in range(OUT_SIZE):
            _, _, _, a = px[x, y]
            out.append(0 if a <= ALPHA_CUT else min(255, a))
    return out


def c_name(icon_id: str) -> str:
    return icon_id.replace("ico-", "").upper().replace("-", "_")


def main() -> None:
    text = open(SHEET, encoding="utf-8").read()
    defs = load_defs(text)
    os.makedirs(OUT_PNG, exist_ok=True)

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        f"// Gerado de {os.path.relpath(SHEET, ROOT)} via cairosvg + use href",
        f"#define UI_ICON_SIZE {OUT_SIZE}",
        "",
    ]

    for icon_id in ICON_IDS:
        if f'id="{icon_id}"' not in defs:
            raise SystemExit(f"Symbol missing: {icon_id}")
        img = rasterize(symbol_svg(defs, icon_id))
        name = icon_id.replace("ico-", "")
        img.save(os.path.join(OUT_PNG, f"{name}.png"))
        alpha = alpha_bytes(img)
        cn = c_name(icon_id)
        lines.append(f"static const uint8_t ICO_{cn}_A[{OUT_SIZE * OUT_SIZE}] = {{")
        for i in range(0, len(alpha), 12):
            chunk = ", ".join(f"{v:3d}" for v in alpha[i : i + 12])
            lines.append(f"    {chunk},")
        lines.append("};")
        lines.append("")

    lines.append(f"#define UI_ICON_BITMAP_COUNT {len(ICON_IDS)}")
    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print(f"OK {len(ICON_IDS)} icons from {SHEET} -> {OUT_PNG}")


if __name__ == "__main__":
    main()
