"""Probe BC5 block copy layout into the RGB image buffer."""

import io
import struct

from PIL import Image

# block0: r channel a0=1, a1=254; indices crafted for distinct values
# block1: g channel a0=101, a1=154
def alpha_block(a0, a1, inds):
    # inds: list of 16 3-bit values
    lut1 = sum(v << (3 * n) for n, v in enumerate(inds[:8]))
    lut2 = sum(v << (3 * n) for n, v in enumerate(inds[8:]))
    return bytes([a0, a1]) + struct.pack("<3H", lut1 & 0xFFFF, (lut1 >> 16) & 0xFFFF, lut2 & 0xFFFF)

b0 = alpha_block(1, 254, [7] * 16)   # all aw=7 -> a[7]=255
b1 = alpha_block(101, 154, [0] * 16)  # all aw=0 -> a[0]=101
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"BC5U")[0], 24) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + b0 + b1)); im.load()
print("mode:", im.mode)
print("pixels:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
print("raw bytes:", im.tobytes().hex())

# now b0 all aw=1 (a[1]=254), b1 all aw=1 (a[1]=154)
b0 = alpha_block(1, 254, [1] * 16)
b1 = alpha_block(101, 154, [1] * 16)
im = Image.open(io.BytesIO(hdr + b0 + b1)); im.load()
print("pixels2:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
print("raw bytes2:", im.tobytes().hex())

# DXT1 decode into RGBA image (mode RGBA): check alpha layout
blk = bytes([0x00, 0x00, 0xFF, 0xFF]) + bytes([0x55, 0x55, 0x55, 0x55])
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DXT1")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + blk)); im.load()
print("dxt1 pixels:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
print("dxt1 raw:", im.tobytes().hex())
