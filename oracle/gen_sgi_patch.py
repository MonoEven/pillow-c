"""Generate BEHAV-SGI-001 fixtures with local Pillow 11.3.0 and patch them
into the AHK test placeholders.

Save fixtures embed the path-basename name field, so they are written
through the same basenames the AHK test uses (A_Temp \\sgi-*.sgi).
Crafted open fixtures (RLE, truncated, bad-mode, bad-compression,
overrun, 16-bit truncation, bad magic) have the zero name field and no
path dependence.
"""

import io
import os
import struct
import tempfile

from PIL import Image

fixes = {}
tmp = tempfile.gettempdir()


def save_fix(key, name, im, **opts):
    path = os.path.join(tmp, name + ".sgi")
    im.save(path, "SGI", **opts)
    data = open(path, "rb").read()
    fixes[key] = data.hex()
    os.unlink(path)


im = Image.new("RGB", (3, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
save_fix("SGI_RGB1_FIX", "sgi-rgb", im)

im = Image.new("L", (3, 2))
im.putdata([10, 20, 30, 40, 50, 60])
save_fix("SGI_L1_FIX", "sgi-l", im)

im = Image.new("RGBA", (2, 2))
im.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
save_fix("SGI_RGBA1_FIX", "sgi-rgba", im)

im = Image.new("L", (3, 2))
im.putdata([10, 20, 30, 40, 50, 60])
save_fix("SGI_L2_FIX", "sgi-l2", im, bpc=2)

im = Image.new("RGB", (2, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12)])
save_fix("SGI_RGB2_FIX", "sgi-rgb2", im, bpc=2)

im = Image.new("RGBA", (2, 2))
im.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
save_fix("SGI_RGBA2_FIX", "sgi-rgba2", im, bpc=2)

im = Image.new("L", (4, 1))
im.putdata([9, 8, 7, 6])
save_fix("SGI_L1ROW_FIX", "sgi-l1row", im)


def header(compression, bpc, dim, x, y, z):
    out = struct.pack(">h", 474) + bytes([compression, bpc])
    out += struct.pack(">HHHH", dim, x, y, z)
    out += struct.pack(">ll", 0, 255)
    out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
    out += struct.pack(">l", 0) + b"\x00" * 404
    assert len(out) == 512
    return out


def rle_file(bpc, xsize, ysize, zsize, rows):
    """rows[ch][row] = list of byte values (bpc1) or BE 16-bit samples
    (bpc2: pass pre-packed ints)."""
    tablen = zsize * ysize
    starttab = []
    lengthtab = []
    chunks = []
    cursor = 512 + 8 * tablen
    for ch in range(zsize):
        for row in range(ysize):
            starttab.append(cursor)
            vals = rows[ch][row]
            if bpc == 1:
                if vals[0] == vals[1]:
                    enc = bytes([0x00 | 2, vals[0]])
                    nchunks = 2
                else:
                    enc = bytes([0x80 | len(vals)]) + bytes(vals)
                    nchunks = 2
                enc += b"\x00"
            else:
                atom = b"".join(struct.pack(">H", v) for v in vals)
                if vals[0] == vals[1]:
                    enc = bytes([0x00, 0x00 | 2]) + struct.pack(">H", vals[0]) + b"\x00\x00"
                    nchunks = 2
                else:
                    enc = bytes([0x00, 0x80 | len(vals)]) + atom + b"\x00\x00"
                    nchunks = 2
            chunks.append(enc)
            lengthtab.append(nchunks)
            cursor += len(enc)
    out = header(1, bpc, 3, xsize, ysize, zsize)
    for v in starttab:
        out += struct.pack(">I", v)
    for v in lengthtab:
        out += struct.pack(">I", v)
    for c in chunks:
        out += c
    return out


# bpc1 RGB 3x2: top row (1,4,7),(2,5,8),(3,6,9); bottom (10,13,16),...
pix = {
    (0, 0): (1, 2, 3), (1, 0): (4, 5, 6), (2, 0): (7, 8, 9),
    (0, 1): (10, 11, 12), (1, 1): (13, 14, 15), (2, 1): (16, 17, 18),
}
rows = [[[pix[(x, row)][ch] for x in range(3)] for row in range(2)] for ch in range(3)]
fixes["SGI_RLE_RGB_FIX"] = rle_file(1, 3, 2, 3, rows).hex()

# bpc1 RGBA 2x2 with run chunks
pix = {
    (0, 0): (1, 2, 3, 4), (1, 0): (5, 6, 7, 8),
    (0, 1): (9, 10, 11, 12), (1, 1): (13, 14, 15, 16),
}
rows = [[[pix[(x, row)][ch] for x in range(2)] for row in range(2)] for ch in range(4)]
fixes["SGI_RLE_RGBA_FIX"] = rle_file(1, 2, 2, 4, rows).hex()

# bpc2 RGB 2x2 with 16-bit samples (v >> 8 expected on reopen)
pix16 = {
    (0, 0): (0x0181, 0x0200, 0x0080), (1, 0): (0x12FF, 0x1300, 0x0100),
    (0, 1): (0x017F, 0x0180, 0x00FF), (1, 1): (0x0000, 0xFFFF, 0x0001),
}
rows = [[[pix16[(x, row)][ch] for x in range(2)] for row in range(2)] for ch in range(3)]
fixes["SGI_RLE_16_FIX"] = rle_file(2, 2, 2, 3, rows).hex()

# verbatim bpc1 RGB 3x2 truncated: 11 payload bytes -> "(2 bytes not processed)"
fixes["SGI_TRUNC_FIX"] = (header(0, 1, 3, 3, 2, 3) + bytes(range(11))).hex()

# valid magic, unsupported (bpc, dimension, zsize) = (1, 2, 2)
fixes["SGI_BADMODE_FIX"] = (header(0, 1, 2, 2, 2, 2)).hex()

# compression=2 -> "cannot load this image"
fixes["SGI_COMP2_FIX"] = (header(2, 1, 3, 2, 2, 3) + bytes(range(12))).hex()

# RLE offset table entry below 512 -> "buffer overrun when reading image file"
out = header(1, 1, 2, 2, 2, 1)
out += struct.pack(">II", 100, 100) + struct.pack(">II", 1, 1)
fixes["SGI_OVERRUN_FIX"] = out.hex()

# bpc2 verbatim L 3x1 truncated (4 of 6 payload bytes) -> "not enough image data"
fixes["SGI_16TRUNC_FIX"] = (header(0, 2, 2, 3, 1, 1) + bytes([0x01, 0x00, 0x02, 0x00])).hex()

# bad magic -> "cannot identify image file <path>"
fixes["SGI_BADMAGIC_FIX"] = (b"\x00" * 512).hex()

for name, hexed in sorted(fixes.items()):
    print(name, len(hexed) // 2)

src = open(r"ahk\pillow.test.ahk", encoding="utf-8").read()
for name, hexed in fixes.items():
    assert name in src, name
    src = src.replace(name, hexed)
open(r"ahk\pillow.test.ahk", "w", encoding="utf-8", newline="\n").write(src)
print("patched")
