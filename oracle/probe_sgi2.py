"""Second SGI probe: pin 16-bit unpack rounding + correct bpc2 RLE."""

import io
import struct

from PIL import Image

print("== unpack rounding boundary ==")
samples = [0x017F, 0x0180, 0x0181, 0x01FF, 0x0200, 0x0201]
raw = b"".join(struct.pack(">H", v) for v in samples)
im2 = Image.new("L", (len(samples), 1))
im2.frombytes(raw, "raw", "L;16B", 0, -1)
print("in :", [hex(v) for v in samples])
print("out:", list(im2.getdata()))

print("== crafted RLE (bpc2, RGB 2x2) corrected ==")
xsize, ysize, zsize = 2, 2, 3
tablen = zsize * ysize
starttab = []
lengthtab = []
chunks = []
cursor = 512 + 8 * tablen
pix = {
    (0, 0): (0x0181, 0x0200, 0x0080), (1, 0): (0x12FF, 0x1300, 0x0100),
    (0, 1): (0x017F, 0x0180, 0x00FF), (1, 1): (0x0000, 0xFFFF, 0x0001),
}
for ch in range(zsize):
    for row in range(ysize):
        starttab.append(cursor)
        vals = [pix[(x, row)][ch] for x in range(xsize)]
        atom = b"".join(struct.pack(">H", v) for v in vals)
        # expandrow2 reads the specifier's SECOND byte as flags/count
        if vals[0] == vals[1]:
            enc = bytes([0x00, 0x00 | 2]) + struct.pack(">H", vals[0]) + b"\x00\x00"
            nchunks = 2
        else:
            enc = bytes([0x00, 0x80 | 2]) + atom + b"\x00\x00"
            nchunks = 2
        chunks.append(enc)
        lengthtab.append(nchunks)
        cursor += len(enc)
out = bytearray()
out += struct.pack(">h", 474) + bytes([1, 2])
out += struct.pack(">H", 3)
out += struct.pack(">HHH", xsize, ysize, zsize)
out += struct.pack(">ll", 0, 255)
out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
out += struct.pack(">l", 0) + b"\x00" * 404
for v in starttab:
    out += struct.pack(">I", v)
for v in lengthtab:
    out += struct.pack(">I", v)
for c in chunks:
    out += c
data = bytes(out)
print("rle2b len", len(data))
ro = Image.open(io.BytesIO(data))
ro.load()
print("rle2b reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])
print("rle2b hex:", data.hex())

print("== RLE final-chunk quirk probe (bpc1, L 3x1) ==")
# one row, channel 0: first chunk copies 2 bytes, last chunk specifier 0x01
# (nonzero, n==1) -> Pillow stops; leftover pixel stays 0 (fresh buffer)
xsize, ysize, zsize = 3, 1, 1
tablen = zsize * ysize
starttab = [512 + 8 * tablen]
lengthtab = [2]
chunk = bytes([0x80 | 2, 7, 8]) + b"\x01"
out = bytearray()
out += struct.pack(">h", 474) + bytes([1, 1])
out += struct.pack(">H", 2)
out += struct.pack(">HHH", xsize, ysize, zsize)
out += struct.pack(">ll", 0, 255)
out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
out += struct.pack(">l", 0) + b"\x00" * 404
out += struct.pack(">I", starttab[0]) + struct.pack(">I", lengthtab[0])
out += chunk
data = bytes(out)
ro = Image.open(io.BytesIO(data))
ro.load()
print("quirk reopen:", ro.mode, ro.size, [ro.getpixel((x, 0)) for x in range(3)])

print("== RLE early-stop row-reuse quirk (bpc1, L 3x2) ==")
# row0 fills fully; row1 stops after 2 pixels -> stale third byte from row0?
xsize, ysize, zsize = 3, 2, 1
tablen = zsize * ysize
row0 = bytes([0x80 | 3, 1, 2, 3]) + b"\x00"
row1 = bytes([0x80 | 2, 9, 9]) + b"\x01"  # stops after 2
starttab = [512 + 8 * tablen, 512 + 8 * tablen + len(row0)]
lengthtab = [2, 2]
out = bytearray()
out += struct.pack(">h", 474) + bytes([1, 1])
out += struct.pack(">H", 2)
out += struct.pack(">HHH", xsize, ysize, zsize)
out += struct.pack(">ll", 0, 255)
out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
out += struct.pack(">l", 0) + b"\x00" * 404
for v in starttab:
    out += struct.pack(">I", v)
for v in lengthtab:
    out += struct.pack(">I", v)
out += row0 + row1
data = bytes(out)
ro = Image.open(io.BytesIO(data))
ro.load()
print("quirk2 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(3)])
