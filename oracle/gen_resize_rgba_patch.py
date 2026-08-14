"""Generate MODE-RGBA-RESIZE-001 fixtures with local Pillow 11.3.0 and
patch them into the AHK test placeholders."""

import struct

from PIL import Image

fixes = {}

im = Image.new("RGBA", (16, 16))
im.putdata([(x * 16, y * 16, (x + y) * 8, 255 - x - y) for y in range(16) for x in range(16)])
fixes["RGBA_RESIZE32_FIX"] = im.resize((32, 32)).tobytes().hex()
fixes["RGBA_RESIZE79_FIX"] = im.resize((7, 9), Image.Resampling.BILINEAR).tobytes().hex()
fixes["RGBA_RESIZEBOX_FIX"] = im.resize((5, 5), Image.Resampling.BICUBIC, (2.0, 3.0, 14.0, 15.0)).tobytes().hex()
fixes["RGBA_RESIZENEAR_FIX"] = im.resize((5, 5), Image.Resampling.NEAREST).tobytes().hex()

im = Image.new("LA", (8, 8))
im.putdata([(x * 31, 255 - y * 31) for y in range(8) for x in range(8)])
fixes["LA_RESIZE_FIX"] = im.resize((13, 5)).tobytes().hex()

im = Image.new("P", (4, 4))
im.putdata([0, 1, 2, 3] * 4)
fixes["P_RESIZE_FIX"] = im.resize((6, 3), Image.Resampling.BICUBIC).tobytes().hex()

im = Image.new("I;16", (4, 4))
im.putdata([0, 100, 200, 300] * 4)
fixes["I16_RESIZE_FIX"] = im.resize((2, 2)).tobytes().hex()

for name, hexed in sorted(fixes.items()):
    print(name, len(hexed) // 2)

src = open(r"ahk\pillow.test.ahk", encoding="utf-8").read()
for name, hexed in fixes.items():
    if name in src:
        src = src.replace(name, hexed)
open(r"ahk\pillow.test.ahk", "w", encoding="utf-8", newline="\n").write(src)
print("patched")
