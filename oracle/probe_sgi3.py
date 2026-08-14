"""Third SGI probe: truncated verbatim error + compression!=0/1 tile-less open."""

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


print("== truncated verbatim (bpc1 RGB) ==")
blob = header(0, 1, 3, 3, 2, 3) + bytes([1, 2, 3] * 3)
try:
    im = Image.open(io.BytesIO(blob))
    im.load()
    print("truncated-verbatim OK", im.mode)
except Exception as e:
    print("truncated-verbatim ERR", type(e).__name__, str(e))

print("== compression=2 (no tile) ==")
blob = header(2, 1, 3, 2, 2, 3) + bytes([1, 2, 3, 4] * 3)
try:
    im = Image.open(io.BytesIO(blob))
    print("mode/size before load:", im.mode, im.size)
    im.load()
    print("comp2 OK", im.mode, [im.getpixel((x, y)) for y in range(2) for x in range(2)])
except Exception as e:
    print("comp2 ERR", type(e).__name__, str(e))

print("== verbatim bpc1 exact reopen pixels (bottom-up check) ==")
blob = header(0, 1, 3, 2, 2, 3) + bytes([
    9, 10, 11, 12,   # R band, row0 (bottom)
    1, 2, 3, 4,      # R band, row1 (top)
    13, 14, 15, 16,  # G row0
    5, 6, 7, 8,      # G row1
    17, 18, 19, 20,  # B row0
    21, 22, 23, 24,  # B row1
])
im = Image.open(io.BytesIO(blob))
im.load()
print("verbatim reopen:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== L 1x1 dimension=1 reopen ==")
blob = header(0, 1, 1, 1, 1, 1) + bytes([200])
im = Image.open(io.BytesIO(blob))
im.load()
print("l1x1:", im.mode, im.size, im.getpixel((0, 0)))

print("== L 4x1 dimension=1 reopen ==")
blob = header(0, 1, 1, 4, 1, 1) + bytes([9, 8, 7, 6])
im = Image.open(io.BytesIO(blob))
im.load()
print("l4x1:", im.mode, im.size, [im.getpixel((x, 0)) for x in range(4)])

print("== L;16B dimension=2 (bpc2 verbatim) reopen ==")
samples = [0x0180, 0x0201, 0x0000]
blob = header(0, 2, 2, 3, 1, 1) + b"".join(struct.pack(">H", v) for v in samples)
im = Image.open(io.BytesIO(blob))
im.load()
print("l16b:", im.mode, im.size, [im.getpixel((x, 0)) for x in range(3)])

print("== 16-bit verbatim truncated mid-sample ==")
blob = header(0, 2, 2, 3, 1, 1) + b"".join(struct.pack(">H", v) for v in [1, 2]) + b"\x00"
try:
    im = Image.open(io.BytesIO(blob))
    im.load()
    print("l16b-trunc OK", [im.getpixel((x, 0)) for x in range(3)])
except Exception as e:
    print("l16b-trunc ERR", type(e).__name__, str(e))
