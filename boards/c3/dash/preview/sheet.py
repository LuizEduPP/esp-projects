import os
import sys

from PIL import Image

SCALE = 3
GAP = 8
NAMES = ["page0", "page1", "page2", "page3", "page4", "page5", "prov"]
TITLES = ["relogio", "clima", "previsao", "grafico", "ai", "sistema", "wifi"]

src = sys.argv[1]
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "screens.png")

frames = []
for name in NAMES:
    path = os.path.join(src, name + ".ppm")
    if os.path.exists(path):
        frames.append(Image.open(path).convert("RGB"))

if not frames:
    raise SystemExit("no frames rendered")

cols = 4
rows = (len(frames) + cols - 1) // cols
w, h = frames[0].size
sheet = Image.new("RGB", (cols * (w + GAP) + GAP, rows * (h + GAP) + GAP), (16, 16, 18))
for i, frame in enumerate(frames):
    x = GAP + (i % cols) * (w + GAP)
    y = GAP + (i // cols) * (h + GAP)
    sheet.paste(frame, (x, y))

sheet = sheet.resize((sheet.width * SCALE, sheet.height * SCALE), Image.NEAREST)
sheet.save(out)
print(out)
