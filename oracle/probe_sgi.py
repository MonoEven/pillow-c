"""Probe Pillow 11.3.0 SGI behavior for BEHAV-SGI-001.

Evidence: exact save bytes, reopen matrix, the 16-bit pack/unpack
rounding, the RLE decoder semantics (offset/length tables, run/copy
chunks, the final-chunk quirk), and the exact error messages.
"""

import io
import struct

from PIL import Image

print("== 16-bit raw pack (L -> L;16B) ==")
im = Image.new("L", (8, 1))
im.putdata([0, 1, 127, 128, 129, 254, 255, 16])
print(im.tobytes("raw", "L;16B", 0, -1).hex())

print("== 16-bit raw unpack (L;16B -> L) ==")
# candidate BE samples straddling rounding boundaries
samples = [0x007F, 0x0080, 0x00FF, 0x0100, 0x017F, 0x0180, 0x12FF, 0x1300]
raw = b"".join(struct.pack(">H", v) for v in samples)
im2 = Image.new("L", (len(samples), 1))
im2.frombytes(raw, "raw", "L;16B", 0, -1)
print(list(im2.getdata()))


def dump(name, data):
    print(name, "len", len(data))
    print("header:", data[:512].hex())
    print("payload:", data[512:].hex())


print("== save L bpc1 ==")
im = Image.new("L", (3, 2))
im.putdata([10, 20, 30, 40, 50, 60])
buf = io.BytesIO()
im.save(buf, "SGI")
dump("l1", buf.getvalue())

print("== save RGB bpc1 ==")
im = Image.new("RGB", (3, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
buf = io.BytesIO()
im.save(buf, "SGI")
dump("rgb1", buf.getvalue())

print("== save RGBA bpc1 ==")
im = Image.new("RGBA", (2, 2))
im.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
buf = io.BytesIO()
im.save(buf, "SGI")
dump("rgba1", buf.getvalue())

print("== save L bpc2 ==")
im = Image.new("L", (3, 2))
im.putdata([10, 20, 30, 40, 50, 60])
buf = io.BytesIO()
im.save(buf, "SGI", bpc=2)
dump("l2", buf.getvalue())

print("== save RGB bpc2 ==")
im = Image.new("RGB", (2, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12)])
buf = io.BytesIO()
im.save(buf, "SGI", bpc=2)
dump("rgb2", buf.getvalue())

print("== save 1-row L (dimension rule) ==")
im = Image.new("L", (4, 1))
im.putdata([9, 8, 7, 6])
buf = io.BytesIO()
im.save(buf, "SGI")
dump("l1row", buf.getvalue())

print("== 1x1 L ==")
im = Image.new("L", (1, 1))
im.putdata([200])
buf = io.BytesIO()
im.save(buf, "SGI")
dump("l1x1", buf.getvalue())

print("== save errors ==")
for mode in ["P", "1", "F", "CMYK", "LA", "I", "I;16", "LAB"]:
    im = Image.new(mode, (2, 2))
    try:
        buf = io.BytesIO()
        im.save(buf, "SGI")
        print(mode, "OK", len(buf.getvalue()))
    except Exception as e:
        print(mode, "ERR", type(e).__name__, str(e))

im = Image.new("L", (2, 2))
for bpc in [0, 3, 4]:
    try:
        buf = io.BytesIO()
        im.save(buf, "SGI", bpc=bpc)
        print("bpc", bpc, "OK")
    except Exception as e:
        print("bpc", bpc, "ERR", type(e).__name__, str(e))

print("== reopen matrix (verbatim bpc1) ==")
rgb1 = Image.new("RGB", (3, 2))
rgb1.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
b = io.BytesIO()
rgb1.save(b, "SGI")
ro = Image.open(io.BytesIO(b.getvalue()))
ro.load()
print("rgb1 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(3)])

rgba1 = Image.new("RGBA", (2, 2))
rgba1.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
b = io.BytesIO()
rgba1.save(b, "SGI")
ro = Image.open(io.BytesIO(b.getvalue()))
ro.load()
print("rgba1 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== reopen matrix (verbatim bpc2) ==")
rgb2 = Image.new("RGB", (2, 2))
rgb2.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12)])
b = io.BytesIO()
rgb2.save(b, "SGI", bpc=2)
data = b.getvalue()
ro = Image.open(io.BytesIO(data))
ro.load()
print("rgb2 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])
# verify the 16-bit samples land as expected by reading the raw payload
print("rgb2 payload:", data[512:].hex())

l2 = Image.new("L", (3, 2))
l2.putdata([10, 20, 30, 40, 50, 60])
b = io.BytesIO()
l2.save(b, "SGI", bpc=2)
ro = Image.open(io.BytesIO(b.getvalue()))
ro.load()
print("l2 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(3)])

rgba2 = Image.new("RGBA", (2, 2))
rgba2.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
b = io.BytesIO()
rgba2.save(b, "SGI", bpc=2)
ro = Image.open(io.BytesIO(b.getvalue()))
ro.load()
print("rgba2 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== crafted RLE (bpc1, RGB 3x2) ==")
# channel-major per-row offset+length tables; chunks: copy+run mix,
# rows terminated by 0x00; verify Pillow's decode and the header parse.
xsize, ysize, zsize = 3, 2, 3
rows = [
    [bytes([1, 2, 3]), bytes([4, 5, 6]), bytes([7, 8, 9])],      # top row (y=0, bottom in file)
    [bytes([10, 11, 12]), bytes([13, 14, 15]), bytes([16, 17, 18])],  # bottom row
]
tablen = zsize * ysize
starttab = []
lengthtab = []
chunks = []
cursor = 512 + 8 * tablen
for ch in range(zsize):
    for row in range(ysize):
        starttab.append(cursor)
        rowbytes = rows[row][ch]
        # encode: run of first two (if equal) else copies; simple: literal
        # copy chunk 0x80|3 + bytes, then 0x00 terminator
        enc = bytes([0x80 | len(rowbytes)]) + rowbytes + b"\x00"
        chunks.append(enc)
        lengthtab.append(2)  # two chunks: the copy and the terminator
        cursor += len(enc)
out = bytearray()
out += struct.pack(">h", 474) + bytes([1, 1])  # rle=1, bpc=1
out += struct.pack(">H", 3)  # dimension
out += struct.pack(">HHH", xsize, ysize, zsize)
out += struct.pack(">ll", 0, 255)
out += b"\x00" * 4 + b"\x00" * 79 + b"\x00"
out += struct.pack(">l", 0) + b"\x00" * 404
assert len(out) == 512, len(out)
for v in starttab:
    out += struct.pack(">I", v)
for v in lengthtab:
    out += struct.pack(">I", v)
for c in chunks:
    out += c
data = bytes(out)
print("rle1 len", len(data), "hex", data.hex())
ro = Image.open(io.BytesIO(data))
ro.load()
print("rle1 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(3)])

print("== crafted RLE (bpc1, RGBA 2x2, runs) ==")
xsize, ysize, zsize = 2, 2, 4
pix = {
    (0, 0): (1, 2, 3, 4), (1, 0): (5, 6, 7, 8),
    (0, 1): (9, 10, 11, 12), (1, 1): (13, 14, 15, 16),
}
tablen = zsize * ysize
starttab = []
lengthtab = []
chunks = []
cursor = 512 + 8 * tablen
for ch in range(zsize):
    for row in range(ysize):
        starttab.append(cursor)
        vals = [pix[(x, row)][ch] for x in range(xsize)]
        enc = b""
        if vals[0] == vals[1]:
            enc += bytes([0x00 | 2, vals[0]])  # run of 2
            chunks_len = 2
        else:
            enc += bytes([0x80 | 2]) + bytes(vals)
            chunks_len = 2
        enc += b"\x00"
        chunks.append(enc)
        lengthtab.append(chunks_len)
        cursor += len(enc)
out = bytearray()
out += struct.pack(">h", 474) + bytes([1, 1])
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
print("rle-rgba len", len(data), "hex", data.hex())
ro = Image.open(io.BytesIO(data))
ro.load()
print("rle-rgba reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== crafted RLE (bpc2, RGB 2x2) ==")
xsize, ysize, zsize = 2, 2, 3
tablen = zsize * ysize
starttab = []
lengthtab = []
chunks = []
cursor = 512 + 8 * tablen
# channel-major rows; each pixel channel value v -> 16-bit BE (v, v) like
# Pillow's pack; use some odd 16-bit values via direct sample bytes
samp = [[[0x00, 0x80], [0x01, 0x00]], [[0x00, 0x7F], [0x12, 0x34]]]  # per row, per x: BE samples
for ch in range(zsize):
    for row in range(ysize):
        starttab.append(cursor)
        be = [samp[row][x][ch] if False else None for x in range(xsize)]
        # per-channel value table: channel ch values at row
        vals = []
        for x in range(xsize):
            v = (1 + 4 * x + 16 * row + ch) * 257
            vals.append(struct.pack(">H", v))
        atom = b"".join(vals)
        enc = bytes([0x80 | xsize, 0x00]) + atom + b"\x00\x00"
        # note: expandrow2 reads specifier = src[1]; keep the flags byte second
        chunks.append(enc)
        lengthtab.append(2)
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
ro = Image.open(io.BytesIO(data))
ro.load()
print("rle2 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])
print("rle2 hex:", data.hex())

print("== open errors ==")
for name, blob in [
    ("badmagic", b"\x00\x00" + b"\x00" * 510),
    ("magic-ok-badmode", struct.pack(">h", 474) + bytes([0, 1]) + struct.pack(">HHHH", 2, 2, 2, 2) + b"\x00" * (512 - 14)),
    ("bpc3", struct.pack(">h", 474) + bytes([0, 3]) + struct.pack(">HHHH", 2, 2, 2, 1) + b"\x00" * (512 - 14)),
]:
    try:
        im = Image.open(io.BytesIO(blob))
        im.load()
        print(name, "OK", im.mode)
    except Exception as e:
        print(name, "ERR", type(e).__name__, str(e))

print("== rle overrun error shape ==")
# truncated RLE table: header + 2 bytes only
blob = struct.pack(">h", 474) + bytes([1, 1]) + struct.pack(">HHHH", 2, 2, 2, 1) + b"\x00" * (512 - 14)
try:
    im = Image.open(io.BytesIO(blob))
    im.load()
    print("truncated OK", im.mode)
except Exception as e:
    print("truncated ERR", type(e).__name__, str(e))

print("== header name field (path basename) ==")
im = Image.new("L", (1, 1))
im.putdata([5])
buf = io.BytesIO()
im.save(buf, "SGI")
h = buf.getvalue()[:512]
print("name bytes:", h[24:104].hex())
import os, tempfile
tmp = os.path.join(tempfile.gettempdir(), "sgi-probe-name.sgi")
im.save(tmp, "SGI")
h = open(tmp, "rb").read(512)
print("name bytes from path:", h[24:104].hex())
os.unlink(tmp)
# long name
longname = os.path.join(tempfile.gettempdir(), "a" * 100 + ".sgi")
im.save(longname, "SGI")
h = open(longname, "rb").read(512)
print("long name bytes:", h[24:104].hex())
os.unlink(longname)
