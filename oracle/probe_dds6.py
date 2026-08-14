"""Probe BC5 decode at odd sizes (clip path) and DXT1 clip behavior."""

import io
import struct

from PIL import Image


def alpha_block(a0, a1, inds):
    lut1 = sum(v << (3 * n) for n, v in enumerate(inds[:8]))
    lut2 = sum(v << (3 * n) for n, v in enumerate(inds[8:]))
    return bytes([a0, a1]) + struct.pack("<3H", lut1 & 0xFFFF, (lut1 >> 16) & 0xFFFF, lut2 & 0xFFFF)


def dds(fourcc, w, h, payload, bitcount, dxgi=None):
    extra = b""
    if dxgi is not None:
        extra = struct.pack("<5I", dxgi, 3, 0, 0, 1)
    return struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, h, w, (w + 3) * 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", fourcc)[0], bitcount) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + extra + payload


b0 = alpha_block(1, 254, [7] * 16)
b1 = alpha_block(101, 154, [7] * 16)
for size in [(3, 3), (5, 5), (2, 2)]:
    w, h = size
    im = Image.open(io.BytesIO(dds(b"BC5U", w, h, b0 + b1, 24))); im.load()
    print("bc5", size, ":", [im.getpixel((x, y)) for y in range(h) for x in range(w)])

# DXT1 odd sizes (RGBA target, sz=4, aligned)
blk = bytes([0x00, 0xF8, 0x00, 0xF8]) + bytes([0xAA] * 4)
for size in [(3, 3), (5, 5)]:
    w, h = size
    im = Image.open(io.BytesIO(dds(b"DXT1", w, h, blk, 32))); im.load()
    print("dxt1", size, ":", [im.getpixel((x, y)) for y in range(h) for x in range(w)])
