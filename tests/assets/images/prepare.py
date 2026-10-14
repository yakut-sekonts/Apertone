"""Regenerate original synthetic WebP/GIF/AVIF fixtures with Pillow 11.3.0."""
import hashlib
import json
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
image = Image.new("RGBA", (32, 24))
colors = [(255, 0, 0, 255), (0, 255, 0, 128), (0, 0, 255, 0), (128, 128, 128, 255)]
for y in range(24):
    for x in range(32):
        image.putpixel((x, y), colors[(y >= 12) * 2 + (x >= 16)])
image.save(root / "quadrants.webp", format="WEBP", lossless=True, exact=True, method=6)

first = Image.new("RGB", (32, 24), (255, 0, 0))
first.putpixel((31, 23), (0, 0, 255))
second = Image.new("RGB", (32, 24), (0, 255, 0))
first.save(root / "two-frames.gif", save_all=True, append_images=[second],
           duration=[100, 100], loop=0, optimize=False, disposal=2)

opaque = Image.new("RGB", (64, 48))
for y in range(48):
    for x in range(64):
        opaque.putpixel((x, y), colors[(y >= 24) * 2 + (x >= 32)][:3])
opaque.save(root / "opaque.avif", format="AVIF", quality=100, subsampling="4:4:4")
print(json.dumps({path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in root.iterdir()
                  if path.suffix in (".webp", ".gif", ".heic", ".avif")}, indent=2))
