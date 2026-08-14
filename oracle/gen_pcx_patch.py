import io

from PIL import Image

fixes = {}
im = Image.new("RGB", (3, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
buf = io.BytesIO()
im.save(buf, "PCX")
fixes["PCX_RGB_FIX"] = buf.getvalue().hex()

im = Image.new("L", (4, 2))
im.putdata([7, 7, 7, 7, 1, 2, 3, 4])
buf = io.BytesIO()
im.save(buf, "PCX")
fixes["PCX_L_FIX"] = buf.getvalue().hex()

im = Image.new("P", (2, 2))
im.putpalette([10, 20, 30] + [0] * (768 - 3))
im.putdata([0, 0, 0, 0])
buf = io.BytesIO()
im.save(buf, "PCX")
fixes["PCX_P_FIX"] = buf.getvalue().hex()

im = Image.new("1", (8, 2))
im.putdata([1, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0])
buf = io.BytesIO()
im.save(buf, "PCX")
fixes["PCX_1_FIX"] = buf.getvalue().hex()

for name, hexed in fixes.items():
    print(name, len(hexed) // 2)

src = open(r"ahk\pillow.test.ahk", encoding="utf-8").read()
for name, hexed in fixes.items():
    assert name in src, name
    src = src.replace(name, hexed)
open(r"ahk\pillow.test.ahk", "w", encoding="utf-8", newline="\n").write(src)
print("patched")
