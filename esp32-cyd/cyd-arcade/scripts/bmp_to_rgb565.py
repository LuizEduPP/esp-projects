#!/usr/bin/env python3
"""Converte BMP 24-bit do Circus Charlie para arrays RGB565 (PROGMEM)."""
import struct
import sys
from pathlib import Path

# Escala WinAPI → CYD (448 px originais → 288 px de jogo)
S = 288 / 448


def rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def sz(v: float) -> int:
    return max(1, int(round(v)))


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


def resize_nn(w, h, pixels, tw, th):
    out = []
    for y in range(th):
        sy = min(h - 1, y * h // th)
        for x in range(tw):
            sx = min(w - 1, x * w // tw)
            out.append(pixels[sy * w + sx])
    return tw, th, out


def apply_chroma(w, h, pixels, key=(0, 0, 0), tol=8):
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

    mag = (255, 0, 255)
    blk = (0, 0, 0)
    pw, ph = sz(66 * S), sz(63 * S)
    jw, jh = sz(50 * S), sz(50 * S)
    rlw, rrh = sz(26 * S), sz(132 * 1.1 * S)
    rrw = sz(23 * S)
    cw, ch = sz(25 * S), sz(26 * S)
    aw, ah = sz(65 * 1.3 * S), sz(64 * 1.3 * S)
    dw, dh = sz(66 * 1.3 * S), sz(67 * 1.3 * S)
    fw, fh = sz(67 * 1.25 * S), sz(92)
    mw, mh = sz(86 * S), sz(30 * S)

    # (nome, arquivo, largura, altura, chroma)
    specs = [
        ("player0", "player0.bmp", pw, ph, mag),
        ("player1", "player1.bmp", pw, ph, mag),
        ("player2", "player2.bmp", pw, ph, mag),
        ("jar0", "front.bmp", jw, jh, mag),
        ("jar1", "front2.bmp", jw, jh, mag),
        ("ring_l0", "enemy_b.bmp", rlw, rrh, mag),
        ("ring_l1", "enemy_1b.bmp", rlw, rrh, mag),
        ("ring_r0", "enemy_f.bmp", rrw, rrh, mag),
        ("ring_r1", "enemy_1f.bmp", rrw, rrh, mag),
        ("cash", "cash.bmp", cw, ch, mag),
        ("bg_tile", "back_normal.bmp", aw, ah, blk),
        ("bg_tile2", "back_normal2.bmp", aw, ah, blk),
        ("bg_deco", "back_deco.bmp", dw, dh, blk),
        ("field_way", "back.bmp", fw, fh, blk),
        ("miter", "miter.bmp", mw, mh, blk),
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
        for name, fname, tw, th, key in specs:
            path = assets / fname
            if not path.exists():
                print(f"skip missing {fname}")
                continue
            w, h, px = load_bmp(path)
            w, h, px = resize_nn(w, h, px, tw, th)
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
