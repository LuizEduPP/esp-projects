import os
import sys

from PIL import Image

SCALE = 3
GAP = 8

src = sys.argv[1]
prefix = sys.argv[2] if len(sys.argv) > 2 else "page"
out = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "screens.png" if prefix == "page" else prefix + ".png",
)

names = [prefix + str(i) for i in range(8)]
if prefix == "page":
    names.append("prov")

frames = []
for name in names:
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
