"""Probe BC5/DXT1 clip behavior at odd sizes (fixed block counts)."""

import io
import struct

from PIL import Image


def alpha_block(a0, a1, inds):
    lut1 = sum(v << (3 * n) for n, v in enumerate(inds[:8]))
    lut2 = sum(v << (3 * n) for n, v in enumerate(inds[8:]))
    return bytes([a0, a1]) + struct.pack("<3H", lut1 & 0xFFFF, (lut1 >> 16) & 0xFFFF, lut2 & 0xFFFF)


def dds(fourcc, w, h, payload, bitcount):
    return struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, h, w, (w + 3) * 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", fourcc)[0], bitcount) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + payload


b0 = alpha_block(1, 254, [7] * 16)
b1 = alpha_block(101, 154, [7] * 16)
im = Image.open(io.BytesIO(dds(b"BC5U", 2, 2, b0 + b1, 24))); im.load()
print("bc5 2x2:", [im.getpixel((x, y)) for y in range(2) for x in range(2)])

im = Image.open(io.BytesIO(dds(b"BC5U", 5, 5, (b0 + b1) * 4, 24))); im.load()
print("bc5 5x5:", [im.getpixel((x, y)) for y in range(5) for x in range(5)])

blk = bytes([0x00, 0xF8, 0x00, 0xF8]) + bytes([0xAA] * 4)
for size in [(3, 3), (5, 5)]:
    w, h = size
    blocks = blk * ((w + 3) // 4) * ((h + 3) // 4)
    im = Image.open(io.BytesIO(dds(b"DXT1", w, h, blocks, 32))); im.load()
    print("dxt1", size, ":", [im.getpixel((x, y)) for y in range(h) for x in range(w)])

# 16-bit RGBA open (bitcount 16, 4 masks) — bytecount 2
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x41, 0, 16) + struct.pack("<4I", 0xF000, 0x0F00, 0x00F0, 0x000F) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + struct.pack("<HHHH", 0xF000, 0x0F00, 0x00F0, 0x000F))); im.load()
print("rgba4444:", [im.getpixel((x, y)) for y in range(2) for x in range(2)])
