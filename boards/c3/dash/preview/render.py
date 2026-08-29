import os
from PIL import Image, ImageDraw, ImageFont

W = H = 128
SCALE = 4
OUT = os.path.join(os.path.dirname(__file__), "screens.png")

SANS = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf"
BOLD = "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf"

BG = (0, 0, 0)
FG = (232, 234, 237)
DIM = (122, 130, 140)
FAINT = (48, 54, 62)
ACCENT = (34, 211, 238)
WARM = (251, 146, 60)
COOL = (96, 165, 250)
SUN = (250, 204, 21)
CLOUD = (148, 163, 184)
RAIN = (56, 130, 246)

big = ImageFont.truetype(BOLD, 40)
mid = ImageFont.truetype(BOLD, 15)
reg = ImageFont.truetype(SANS, 12)
micro = ImageFont.truetype(BOLD, 9)

PAD = 6
HEADER_Y = 17
FOOTER_Y = 122


def new():
    return Image.new("RGB", (W, H), BG)


def center(d, text, y, font, fill):
    d.text(((W - d.textlength(text, font=font)) / 2, y), text, font=font, fill=fill)


def right(d, text, y, font, fill):
    d.text((W - PAD - d.textlength(text, font=font), y), text, font=font, fill=fill)


def chrome(d, label, page, bars=3):
    d.text((PAD, 4), label, font=micro, fill=DIM)
    for i in range(3):
        h = 3 + i * 3
        c = ACCENT if i < bars else FAINT
        d.rectangle([W - 20 + i * 5, 12 - h, W - 18 + i * 5, 12], fill=c)
    d.line([PAD, HEADER_Y, W - PAD, HEADER_Y], fill=FAINT, width=1)
    seg = (W - 2 * PAD) / 4
    for i in range(4):
        x = PAD + i * seg
        d.rectangle([x, FOOTER_Y, x + seg - 4, FOOTER_Y + 1], fill=ACCENT if i == page else FAINT)


def icon_sun(d, cx, cy, r=9):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=SUN)


def icon_cloud(d, cx, cy, color=CLOUD):
    d.ellipse([cx - 13, cy - 3, cx - 1, cy + 9], fill=color)
    d.ellipse([cx - 6, cy - 9, cx + 10, cy + 7], fill=color)
    d.ellipse([cx + 4, cy - 3, cx + 15, cy + 8], fill=color)
    d.rectangle([cx - 12, cy + 2, cx + 14, cy + 9], fill=color)


def icon_rain(d, cx, cy):
    icon_cloud(d, cx, cy - 5)
    for i in range(3):
        x = cx - 9 + i * 9
        d.line([x, cy + 8, x - 3, cy + 15], fill=RAIN, width=2)


def screen_clock():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "CESARIO LANGE", 0)
    center(d, "15:47", 26, big, FG)
    center(d, "SABADO", 78, mid, ACCENT)
    center(d, "29 de agosto", 98, reg, DIM)
    return img


def screen_weather():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "CLIMA", 1)
    d.text((PAD, 24), "29", font=big, fill=WARM)
    w = d.textlength("29", font=big)
    d.ellipse([PAD + w + 3, 30, PAD + w + 9, 36], outline=WARM, width=2)
    icon_cloud(d, 100, 46)
    d.text((PAD, 72), "encoberto", font=reg, fill=FG)
    d.line([PAD, 92, W - PAD, 92], fill=FAINT, width=1)
    cols = [("MIN", "20"), ("MAX", "31"), ("UMID", "42%")]
    step = (W - 2 * PAD) / 3
    for i, (k, v) in enumerate(cols):
        cx = PAD + step * i + step / 2
        d.text((cx - d.textlength(k, font=micro) / 2, 98), k, font=micro, fill=FAINT)
        d.text((cx - d.textlength(v, font=reg) / 2, 106), v, font=reg, fill=FG)
    return img


def screen_ai():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "AI", 2)
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
    d.rectangle([PAD, top, PAD + 1, top + len(lines) * 15 - 4], fill=ACCENT)
    for i, ln in enumerate(lines):
        d.text((PAD + 8, top + i * 15), ln, font=reg, fill=FG)
    return img


def screen_system():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "SISTEMA", 3)
    rows = [("rede", "hotspot"), ("ip", "10.31.254.103"), ("sinal", "-58 dBm"),
            ("livre", "187 KB"), ("ligado", "0h 12m"), ("bateria", "sem sensor")]
    for i, (k, v) in enumerate(rows):
        y = 25 + i * 16
        d.text((PAD, y), k, font=micro, fill=FAINT)
        right(d, v, y, micro, FG)
        if i < len(rows) - 1:
            d.line([PAD, y + 12, W - PAD, y + 12], fill=(24, 27, 32), width=1)
    return img


def main():
    screens = [screen_clock(), screen_weather(), screen_ai(), screen_system()]
    gap = 8
    sheet = Image.new("RGB", (len(screens) * (W + gap) + gap, H + 2 * gap), (18, 18, 20))
    for i, s in enumerate(screens):
        sheet.paste(s, (gap + i * (W + gap), gap))
    sheet = sheet.resize((sheet.width * SCALE, sheet.height * SCALE), Image.NEAREST)
    sheet.save(OUT)
    print(OUT)


main()
