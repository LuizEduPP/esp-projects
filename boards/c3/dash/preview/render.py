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
FAINT = (52, 58, 66)
ACCENT = (34, 211, 238)
WARM = (251, 146, 60)
COOL = (96, 165, 250)
GOOD = (52, 211, 153)
SUN = (250, 204, 21)
CLOUD = (148, 163, 184)
RAIN = (56, 130, 246)

big = ImageFont.truetype(BOLD, 42)
mid = ImageFont.truetype(BOLD, 15)
reg = ImageFont.truetype(SANS, 12)
tiny = ImageFont.truetype(SANS, 10)
micro = ImageFont.truetype(BOLD, 9)


def new():
    return Image.new("RGB", (W, H), BG)


def center(d, text, y, font, fill):
    w = d.textlength(text, font=font)
    d.text(((W - w) / 2, y), text, font=font, fill=fill)


def chrome(d, label, page, bars=3):
    d.text((6, 5), label, font=micro, fill=DIM)
    for i in range(3):
        h = 3 + i * 3
        c = ACCENT if i < bars else FAINT
        d.rectangle([W - 22 + i * 5, 12 - h, W - 20 + i * 5, 12], fill=c)
    seg = (W - 12) / 4
    for i in range(4):
        x = 6 + i * seg
        d.rectangle([x, H - 4, x + seg - 4, H - 3], fill=ACCENT if i == page else FAINT)


def icon_sun(d, cx, cy, r=9):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=SUN)


def icon_cloud(d, cx, cy, color=CLOUD):
    d.ellipse([cx - 13, cy - 3, cx - 1, cy + 9], fill=color)
    d.ellipse([cx - 6, cy - 9, cx + 10, cy + 7], fill=color)
    d.ellipse([cx + 4, cy - 3, cx + 15, cy + 8], fill=color)
    d.rectangle([cx - 12, cy + 2, cx + 14, cy + 9], fill=color)


def icon_rain(d, cx, cy):
    icon_cloud(d, cx, cy - 4)
    for i in range(3):
        x = cx - 9 + i * 9
        d.line([x, cy + 9, x - 3, cy + 17], fill=RAIN, width=2)


def screen_clock():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "CESARIO LANGE", 0)
    center(d, "15:47", 26, big, FG)
    d.line([14, 76, W - 14, 76], fill=FAINT, width=1)
    center(d, "SABADO", 82, mid, ACCENT)
    center(d, "29 de agosto", 100, reg, DIM)
    return img


def screen_weather():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "CLIMA", 1)
    d.text((8, 26), "29", font=big, fill=WARM)
    w = d.textlength("29", font=big)
    d.text((8 + w + 2, 32), "°", font=mid, fill=WARM)
    icon_cloud(d, 100, 44)
    d.text((8, 74), "encoberto", font=reg, fill=FG)
    d.line([8, 92, W - 8, 92], fill=FAINT, width=1)
    cols = [("MIN", "20"), ("MAX", "31"), ("UMID", "42%")]
    for i, (k, v) in enumerate(cols):
        x = 8 + i * 40
        d.text((x, 97), k, font=micro, fill=FAINT)
        d.text((x, 108), v, font=reg, fill=DIM)
    return img


def screen_ai():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "AI", 2)
    d.text((6, 18), "\u201c", font=big, fill=FAINT)
    text = "O ceu encoberto promete uma tarde quente e umida."
    words, line, y = text.split(), "", 44
    for word in words:
        probe = (line + " " + word).strip()
        if d.textlength(probe, font=reg) > W - 20:
            d.text((10, y), line, font=reg, fill=FG)
            y += 15
            line = word
        else:
            line = probe
    d.text((10, y), line, font=reg, fill=FG)
    return img


def screen_system():
    img = new()
    d = ImageDraw.Draw(img)
    chrome(d, "SISTEMA", 3)
    rows = [("rede", "hotspot"), ("ip", "10.31.254.103"), ("sinal", "-58 dBm"),
            ("livre", "187 KB"), ("ligado", "0h 12m")]
    for i, (k, v) in enumerate(rows):
        y = 24 + i * 17
        d.text((8, y), k, font=micro, fill=FAINT)
        d.text((46, y - 2), v, font=tiny, fill=FG)
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
