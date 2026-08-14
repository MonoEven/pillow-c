"""Simulate Pillow's C BCN decode/put_block and diff against the real
Pillow output to pin the exact semantics."""

import io
import struct

from PIL import Image


def decode_bc3_alpha(src, sign=0):
    a0 = src[0]
    a1 = src[1]
    lut1 = src[2] | (src[3] << 8) | (src[4] << 16)
    lut2 = src[5] | (src[6] << 8) | (src[7] << 16)
    if sign:
        a0 = (a0 + 128) & 0xFF
        a1 = (a1 + 128) & 0xFF
    a = [0] * 8
    a[0] = a0
    a[1] = a1
    if a0 > a1:
        a[2] = (6 * a0 + 1 * a1) // 7
        a[3] = (5 * a0 + 2 * a1) // 7
        a[4] = (4 * a0 + 3 * a1) // 7
        a[5] = (3 * a0 + 4 * a1) // 7
        a[6] = (2 * a0 + 5 * a1) // 7
        a[7] = (1 * a0 + 6 * a1) // 7
    else:
        a[2] = (4 * a0 + 1 * a1) // 5
        a[3] = (3 * a0 + 2 * a1) // 5
        a[4] = (2 * a0 + 3 * a1) // 5
        a[5] = (1 * a0 + 4 * a1) // 5
        a[6] = 0
        a[7] = 255
    out = [0] * 16
    for n in range(8):
        aw = 7 & (lut1 >> (3 * n))
        out[n] = a[aw]
    for n in range(8):
        aw = 7 & (lut2 >> (3 * n))
        out[8 + n] = a[aw]
    return out


def alpha_block(a0, a1, inds):
    lut1 = sum(v << (3 * n) for n, v in enumerate(inds[:8]))
    lut2 = sum(v << (3 * n) for n, v in enumerate(inds[8:]))
    return bytes([a0, a1]) + struct.pack("<3H", lut1 & 0xFFFF, (lut1 >> 16) & 0xFFFF, lut2 & 0xFFFF)


b0 = alpha_block(1, 254, [7] * 16)
b1 = alpha_block(101, 154, [7] * 16)
print("b0:", b0.hex())
print("sim r:", decode_bc3_alpha(b0))
print("sim g:", decode_bc3_alpha(b1))

hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"BC5U")[0], 24) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + b0 + b1)); im.load()
print("real:", im.tobytes().hex())
print("real px:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])

# aligned-copy prediction from the simulated col
r = decode_bc3_alpha(b0)
g = decode_bc3_alpha(b1)
aligned = b"".join(bytes([r[i], g[i], 0]) for i in range(16))
print("aligned sim:", aligned.hex())
