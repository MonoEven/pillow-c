import io

from PIL import Image

im = Image.new("RGB", (3, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
buf = io.BytesIO()
im.save(buf, "PCX")
data = buf.getvalue()
print("len", len(data))
print(data[:128].hex())
print("data section:", data[128:].hex())
ro = Image.open(io.BytesIO(data))
ro.load()
print("reopen:", ro.mode, ro.size, ro.getpixel((0, 0)), ro.getpixel((2, 1)))
