"""Fifth-ish DDS probe: fix LA16 flags and header-size error shape."""

import io
import struct

from PIL import Image

print("== LA16 luminance reopen (flags 0x20001) ==")
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20001, 0, 16) + struct.pack("<4I", 0xFF, 0xFF, 0xFF, 0xFF000000) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + bytes([10, 4, 20, 5, 30, 6, 40, 7]))); im.load()
print("la16:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== header size errors ==")
blob = b"DDS " + struct.pack("<I", 100) + b"\x00" * 120
try:
    im = Image.open(io.BytesIO(blob)); im.load()
    print("hdr100 OK")
except Exception as e:
    print("hdr100 ERR", type(e).__name__, str(e))
blob = b"DDS " + struct.pack("<I", 124) + b"\x00" * 100
try:
    im = Image.open(io.BytesIO(blob)); im.load()
    print("short-hdr OK")
except Exception as e:
    print("short-hdr ERR", type(e).__name__, str(e))

print("== P8 palette reopen ==")
pal = b"".join(bytes([i, i, i, 255]) for i in range(256))
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 2, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20, 0, 8) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + pal + bytes([0, 1, 2, 3]))); im.load()
print("p8:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)], list(im.getpalette("RGBA")[:8]))

print("== BC4/ATI1 crafted open (bcn n=4) ==")
# one 8-byte alpha block: a0=16, a1=240, indices
block = bytes([16, 240]) + struct.pack("<3H", 0x9249, 0x2492, 0x4924)
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"BC4U")[0], 8) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + block)); im.load()
print("bc4:", im.mode, im.size, [im.getpixel((x, y)) for y in range(4) for x in range(4)])

print("== BC5U crafted open (n=5) ==")
b0 = bytes([16, 240]) + struct.pack("<3H", 0x9249, 0x2492, 0x4924)
b1 = bytes([0, 255]) + struct.pack("<3H", 0x0000, 0x0000, 0x0000)
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"BC5U")[0], 24) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + b0 + b1)); im.load()
print("bc5u:", im.mode, im.size, [im.getpixel((x, y)) for y in range(4) for x in range(4)])

print("== 16-bit L (unsupported) ==")
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20000, 0, 16) + struct.pack("<4I", 0xFF, 0xFF, 0xFF, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
try:
    im = Image.open(io.BytesIO(hdr + b"\x00" * 8)); im.load()
    print("lum16 OK")
except Exception as e:
    print("lum16 ERR", type(e).__name__, str(e))
