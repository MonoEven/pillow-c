"""Fourth SGI probe: pin the truncated-verbatim byte count + 1-band L truncation."""

import io
import struct

from PIL import Image


def header(compression, bpc, dim, x, y, z):
    out = struct.pack(">h", 474) + bytes([compression, bpc])
    out += struct.pack(">HHHH", dim, x, y, z)
    out += struct.pack(">ll", 0, 255)
    out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
    out += struct.pack(">l", 0) + b"\x00" * 404
    return out


for n in [0, 1, 4, 8, 11, 12]:
    blob = header(0, 1, 3, 3, 2, 3) + bytes(range(n))
    try:
        im = Image.open(io.BytesIO(blob))
        im.load()
        print("payload", n, "OK", im.mode)
    except Exception as e:
        print("payload", n, "ERR", type(e).__name__, str(e))

for n in [0, 1, 3, 4]:
    blob = header(0, 1, 2, 4, 1, 1) + bytes(range(n))
    try:
        im = Image.open(io.BytesIO(blob))
        im.load()
        print("L payload", n, "OK", im.mode)
    except Exception as e:
        print("L payload", n, "ERR", type(e).__name__, str(e))
