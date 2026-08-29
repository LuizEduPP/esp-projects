import os
import sys
from PIL import Image, ImageDraw, ImageFont

W = H = 128
SCALE = 4
HERE = os.path.dirname(__file__)

SANS = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf"
BOLD = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf"

big = ImageFont.truetype(BOLD, 40)
mid = ImageFont.truetype(BOLD, 15)
reg = ImageFont.truetype(SANS, 12)
micro = ImageFont.truetype(BOLD, 9)

PAD = 6
HEADER_Y = 17
FOOTER_Y = 122

PALETTES = {
    "midnight": dict(
        bg=(8, 11, 16), fg=(230, 237, 243), dim=(125, 137, 152), faint=(38, 45, 56),
        hair=(22, 27, 34), accent=(125, 211, 252), warm=(251, 191, 36),
        cool=(129, 180, 255), sun=(250, 204, 21), cloud=(148, 163, 184), rain=(96, 165, 250),
    ),
    "amber": dict(
        bg=(0, 0, 0), fg=(255, 216, 160), dim=(176, 128, 66), faint=(64, 44, 20),
        hair=(34, 24, 12), accent=(255, 159, 28), warm=(255, 122, 41), cool=(214, 190, 130),
        sun=(255, 190, 60), cloud=(180, 145, 95), rain=(150, 190, 210),
    ),
    "mint": dict(
        bg=(6, 16, 13), fg=(232, 245, 238), dim=(120, 148, 136), faint=(34, 56, 48),
        hair=(18, 32, 27), accent=(52, 211, 153), warm=(251, 191, 36),
        cool=(110, 200, 230), sun=(250, 204, 21), cloud=(150, 175, 168), rain=(96, 190, 220),
    ),
    "paper": dict(
        bg=(240, 242, 245), fg=(20, 24, 31), dim=(104, 114, 128), faint=(196, 202, 210),
        hair=(220, 224, 230), accent=(11, 114, 231), warm=(200, 80, 20), cool=(30, 100, 200),
        sun=(230, 160, 20), cloud=(140, 152, 168), rain=(40, 110, 200),
    ),
}


def new(P):
    return Image.new("RGB", (W, H), P["bg"])


def center(d, P, text, y, font, fill):
    d.text(((W - d.textlength(text, font=font)) / 2, y), text, font=font, fill=fill)


def right(d, text, y, font, fill):
    d.text((W - PAD - d.textlength(text, font=font), y), text, font=font, fill=fill)


def chrome(d, P, label, page, bars=3):
    d.text((PAD, 4), label, font=micro, fill=P["dim"])
    for i in range(3):
        h = 3 + i * 3
        c = P["accent"] if i < bars else P["faint"]
        d.rectangle([W - 20 + i * 5, 12 - h, W - 18 + i * 5, 12], fill=c)
    d.line([PAD, HEADER_Y, W - PAD, HEADER_Y], fill=P["faint"], width=1)
    seg = (W - 2 * PAD) / 4
    for i in range(4):
        x = PAD + i * seg
        d.rectangle([x, FOOTER_Y, x + seg - 4, FOOTER_Y + 1],
                    fill=P["accent"] if i == page else P["faint"])


def icon_cloud(d, P, cx, cy, color=None):
    color = color or P["cloud"]
    d.ellipse([cx - 13, cy - 3, cx - 1, cy + 9], fill=color)
    d.ellipse([cx - 6, cy - 9, cx + 10, cy + 7], fill=color)
    d.ellipse([cx + 4, cy - 3, cx + 15, cy + 8], fill=color)
    d.rectangle([cx - 12, cy + 2, cx + 14, cy + 9], fill=color)


def screen_clock(P):
    img = new(P)
    d = ImageDraw.Draw(img)
    chrome(d, P, "CESARIO LANGE", 0)
    center(d, P, "15:47", 26, big, P["fg"])
    center(d, P, "SABADO", 78, mid, P["accent"])
    center(d, P, "29 de agosto", 98, reg, P["dim"])
    return img


def screen_weather(P):
    img = new(P)
    d = ImageDraw.Draw(img)
    chrome(d, P, "CLIMA", 1)
    d.text((PAD, 24), "29", font=big, fill=P["warm"])
    w = d.textlength("29", font=big)
    d.ellipse([PAD + w + 3, 30, PAD + w + 9, 36], outline=P["warm"], width=2)
    icon_cloud(d, P, 100, 46)
    d.text((PAD, 72), "encoberto", font=reg, fill=P["fg"])
    d.line([PAD, 92, W - PAD, 92], fill=P["faint"], width=1)
    cols = [("MIN", "20"), ("MAX", "31"), ("UMID", "42%")]
    step = (W - 2 * PAD) / 3
    for i, (k, v) in enumerate(cols):
        cx = PAD + step * i + step / 2
        d.text((cx - d.textlength(k, font=micro) / 2, 98), k, font=micro, fill=P["dim"])
        d.text((cx - d.textlength(v, font=reg) / 2, 106), v, font=reg, fill=P["fg"])
    return img


def screen_ai(P):
    img = new(P)
    d = ImageDraw.Draw(img)
    chrome(d, P, "AI", 2)
    text = "O ceu encoberto promete uma tarde quente e umida."
    lines, line = [], ""
    for word in text.split():
        probe = (line + " " + word).strip()
        if d.textlength(probe, font=reg) > W - 24:
            lines.append(line)
            line = word
        else:
            line = probe
    lines.append(line)
    top = 24 + max(0, (94 - len(lines) * 15)) // 2
    d.rectangle([PAD, top, PAD + 1, top + len(lines) * 15 - 4], fill=P["accent"])
    for i, ln in enumerate(lines):
        d.text((PAD + 8, top + i * 15), ln, font=reg, fill=P["fg"])
    return img


def screen_system(P):
    img = new(P)
    d = ImageDraw.Draw(img)
    chrome(d, P, "SISTEMA", 3)
    rows = [("rede", "hotspot"), ("ip", "10.31.254.103"), ("sinal", "-58 dBm"),
            ("livre", "187 KB"), ("ligado", "0h 12m")]
    for i, (k, v) in enumerate(rows):
        y = 27 + i * 18
        d.text((PAD, y), k, font=micro, fill=P["dim"])
        right(d, v, y, micro, P["fg"])
        if i < len(rows) - 1:
            d.line([PAD, y + 13, W - PAD, y + 13], fill=P["hair"], width=1)
    return img


def sheet(names):
    gap = 8
    rows = len(names)
    label_w = 0 if rows == 1 else 76
    img = Image.new("RGB", (label_w + 4 * (W + gap) + gap, rows * (H + gap) + gap), (18, 18, 20))
    d = ImageDraw.Draw(img)
    for r, name in enumerate(names):
        P = PALETTES[name]
        y = gap + r * (H + gap)
        if label_w:
            d.text((8, y + H // 2 - 6), name, font=ImageFont.truetype(BOLD, 13), fill=(235, 235, 235))
        for c, fn in enumerate([screen_clock, screen_weather, screen_ai, screen_system]):
            img.paste(fn(P), (label_w + gap + c * (W + gap), y))
    return img.resize((img.width * SCALE, img.height * SCALE), Image.NEAREST)


def main():
    names = sys.argv[1:] or ["midnight"]
    if names == ["all"]:
        names = list(PALETTES)
    out = os.path.join(HERE, "palettes.png" if len(names) > 1 else "screens.png")
    sheet(names).save(out)
    print(out)


main()
