"""Sixth DDS probe: truncation behavior for bcn and dds_rgb tiles."""

import io
import struct

from PIL import Image


def hdr(flags, w, h, pitch, pfflags, fourcc, bitcount, masks):
    return struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, flags, h, w, pitch, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, pfflags, fourcc, bitcount) + struct.pack("<4I", *masks) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)


print("== truncated DXT1 payload (4 of 8 bytes) ==")
dxt = hdr(0x81007, 4, 4, 28, 0x4, struct.unpack("<I", b"DXT1")[0], 32, [0] * 4)
try:
    im = Image.open(io.BytesIO(dxt + b"\x66\x29\x00\x00")); im.load()
    print("trunc-dxt1 OK", im.mode, [im.getpixel((x, y)) for y in range(4) for x in range(4)])
except Exception as e:
    print("trunc-dxt1 ERR", type(e).__name__, str(e))

print("== empty BCN payload ==")
try:
    im = Image.open(io.BytesIO(dxt)); im.load()
    print("empty-dxt1 OK", im.mode, [im.getpixel((x, y)) for y in range(4) for x in range(4)])
except Exception as e:
    print("empty-dxt1 ERR", type(e).__name__, str(e))

print("== truncated dds_rgb payload ==")
rgb = hdr(0x100F, 2, 2, 6, 0x40, 0, 24, [0x00FF0000, 0x0000FF00, 0x000000FF, 0])
try:
    im = Image.open(io.BytesIO(rgb + bytes([1, 2, 3]))); im.load()
    print("trunc-rgb OK", im.mode, [im.getpixel((x, y)) for y in range(2) for x in range(2)])
except Exception as e:
    print("trunc-rgb ERR", type(e).__name__, str(e))

print("== truncated L8 raw payload ==")
lum = hdr(0x100F, 2, 2, 2, 0x20000, 0, 8, [0xFF000000] * 3 + [0])
try:
    im = Image.open(io.BytesIO(lum + bytes([10, 20]))); im.load()
    print("trunc-l8 OK", im.mode, [im.getpixel((x, y)) for y in range(2) for x in range(2)])
except Exception as e:
    print("trunc-l8 ERR", type(e).__name__, str(e))
