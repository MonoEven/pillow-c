"""Test BC6 bit-offset hypotheses against the wheel's decode."""

import io
import struct

from PIL import Image

block = bytes.fromhex("f08faa002c5ed460e200108df5118df5")

# variant decodes: endpoints start bit varies (2 or 5), partition/indices follow
packings0 = [
    116, 132, 180, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 48, 49, 50, 51, 52, 164, 112, 113, 114, 115, 64, 65,
    66, 67, 68, 176, 160, 161, 162, 163, 80, 81, 82, 83, 84, 177, 128,
    129, 130, 131, 96, 97, 98, 99, 100, 178, 144, 145, 146, 147, 148, 179,
]


def get_bit(src, bit):
    by = bit >> 3
    bit &= 7
    return (src[by] >> bit) & 1


def get_bits(src, bit, count):
    if not count:
        return 0
    by = bit >> 3
    bit &= 7
    if bit + count <= 8:
        return (src[by] >> bit) & ((1 << count) - 1)
    x = src[by] | (src[by + 1] << 8)
    return (x >> bit) & ((1 << count) - 1)


def endpoints_for(start):
    eps = [0] * 12
    for i in range(75):
        di = packings0[i]
        dw = di >> 4
        dbit = di & 15
        eps[dw] |= get_bit(block, start + i) << dbit
    return eps


for start in [2, 3, 4, 5, 6, 7, 8]:
    eps = endpoints_for(start)
    print(start, [hex(e) for e in eps[:6]])

hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DX10")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + struct.pack("<5I", 95, 3, 0, 0, 1)
im = Image.open(io.BytesIO(hdr + block)); im.load()
print("wheel:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
