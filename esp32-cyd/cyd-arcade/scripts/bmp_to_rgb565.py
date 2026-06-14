#!/usr/bin/env python3
"""Converte BMP 24-bit do Circus Charlie para arrays RGB565 (PROGMEM)."""
import struct
import sys
from pathlib import Path


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def load_bmp(path: Path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{path}: not BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    w = struct.unpack_from("<i", data, 18)[0]
    h = struct.unpack_from("<i", data, 22)[0]
    bpp = struct.unpack_from("<H", data, 28)[0]
    if bpp != 24:
        raise ValueError(f"{path}: bpp={bpp}, need 24")
    h_abs = abs(h)
    flip = h > 0
    row = ((w * 3 + 3) // 4) * 4
    pixels = []
    for y in range(h_abs):
        src_y = (h_abs - 1 - y) if flip else y
        row_off = offset + src_y * row
        for x in range(w):
            b, g, r = data[row_off + x * 3 : row_off + x * 3 + 3]
            pixels.append(rgb565(r, g, b))
    return w, h_abs, pixels


def scale_down(w, h, pixels, div):
    if div <= 1:
        return w, h, pixels
    nw, nh = max(1, w // div), max(1, h // div)
    out = []
    for y in range(nh):
        for x in range(nw):
            sx = min(w - 1, x * div + div // 2)
            sy = min(h - 1, y * div + div // 2)
            out.append(pixels[sy * w + sx])
    return nw, nh, out


def apply_chroma(w, h, pixels, key=(0, 0, 0), tol=8):
    key565 = rgb565(*key)
    out = []
    for c in pixels:
        r = ((c >> 11) & 0x1F) << 3
        g = ((c >> 5) & 0x3F) << 2
        b = (c & 0x1F) << 3
        if abs(r - key[0]) <= tol and abs(g - key[1]) <= tol and abs(b - key[2]) <= tol:
            out.append(0x0001)
        else:
            out.append(c)
    return out


def emit_c(name, w, h, pixels, out_h):
    var = f"circus_{name}"
    out_h.write(f"static const uint16_t {var}[] PROGMEM = {{\n")
    for i, c in enumerate(pixels):
        if i % 12 == 0:
            out_h.write("    ")
        out_h.write(f"0x{c:04X},")
        if i % 12 == 11:
            out_h.write("\n")
        else:
            out_h.write(" ")
    if len(pixels) % 12 != 0:
        out_h.write("\n")
    out_h.write("};\n")
    out_h.write(f"static const CircusSprite circus_sp_{name} = {{ {var}, {w}, {h} }};\n\n")


def main():
    if len(sys.argv) < 3:
        print("Usage: bmp_to_rgb565.py <assets_dir> <output.h>")
        sys.exit(1)
    assets = Path(sys.argv[1])
    out_path = Path(sys.argv[2])

    specs = [
        ("player0", "player0.bmp", 2, (0, 0, 0)),
        ("player1", "player1.bmp", 2, (0, 0, 0)),
        ("player2", "player2.bmp", 2, (0, 0, 0)),
        ("jar0", "front.bmp", 2, (0, 0, 0)),
        ("jar1", "front2.bmp", 2, (0, 0, 0)),
        ("ring_l0", "enemy_b.bmp", 3, (0, 0, 0)),
        ("ring_l1", "enemy_1b.bmp", 3, (0, 0, 0)),
        ("ring_r0", "enemy_f.bmp", 3, (0, 0, 0)),
        ("ring_r1", "enemy_1f.bmp", 3, (0, 0, 0)),
        ("cash", "cash.bmp", 1, (0, 0, 0)),
        ("bg_tile", "back_normal.bmp", 2, (0, 0, 0)),
    ]

    with out_path.open("w", encoding="utf-8") as out:
        out.write("#pragma once\n#include <stdint.h>\n#include <pgmspace.h>\n\n")
        out.write("typedef struct {\n")
        out.write("    const uint16_t* data;\n")
        out.write("    int16_t w;\n")
        out.write("    int16_t h;\n")
        out.write("} CircusSprite;\n\n")
        out.write("#define CIRCUS_CHROMA 0x0001\n\n")

        names = []
        for name, fname, div, key in specs:
            path = assets / fname
            if not path.exists():
                print(f"skip missing {fname}")
                continue
            w, h, px = load_bmp(path)
            w, h, px = scale_down(w, h, px, div)
            px = apply_chroma(w, h, px, key)
            emit_c(name, w, h, px, out)
            names.append(name)
            print(f"{name}: {w}x{h} from {fname}")

        out.write("enum {\n")
        for i, n in enumerate(names):
            out.write(f"    CIRCUS_SP_{n.upper()} = {i},\n")
        out.write(f"    CIRCUS_SP_COUNT = {len(names)}\n")
        out.write("};\n\n")

        out.write("static const CircusSprite* const circus_sprites[CIRCUS_SP_COUNT] = {\n")
        for n in names:
            out.write(f"    &circus_sp_{n},\n")
        out.write("};\n")

    print(f"Wrote {out_path}")


if __name__ == "__main__":
    main()
