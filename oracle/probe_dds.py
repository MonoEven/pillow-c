"""Probe Pillow 11.3.0 DDS behavior for BEHAV-DDS-001.

Evidence: exact raw-save bytes (L/LA/RGB/RGBA, mask quirks), DXT1/3/5
and BC2/BC3/BC5 save bytes, the reopen matrix (raw masks, L/LA, P,
BCN), and the exact error messages.
"""

import io
import struct

from PIL import Image

def dump(name, data):
    print(name, "len", len(data))
    print("header:", data[:128].hex())
    print("payload:", data[128:].hex())

im = Image.new("RGB", (2, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12)])
b = io.BytesIO()
im.save(b, "DDS")
dump("rgb-raw", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("rgb-raw reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

im = Image.new("RGBA", (2, 2))
im.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
b = io.BytesIO()
im.save(b, "DDS")
dump("rgba-raw", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("rgba-raw reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

im = Image.new("L", (2, 2))
im.putdata([10, 20, 30, 40])
b = io.BytesIO()
im.save(b, "DDS")
dump("l-raw", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("l-raw reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

im = Image.new("LA", (2, 2))
im.putdata([(10, 4), (20, 5), (30, 6), (40, 7)])
b = io.BytesIO()
im.save(b, "DDS")
dump("la-raw", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("la-raw reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== save errors ==")
for mode in ["P", "1", "F", "CMYK", "I", "I;16", "LAB", "RGBX"]:
    m = Image.new(mode, (2, 2))
    try:
        b = io.BytesIO()
        m.save(b, "DDS")
        print(mode, "OK")
    except Exception as e:
        print(mode, "ERR", type(e).__name__, str(e))

im = Image.new("L", (2, 2))
try:
    b = io.BytesIO()
    im.save(b, "DDS", pixel_format="BC5")
    print("BC5-L OK")
except Exception as e:
    print("BC5-L ERR", type(e).__name__, str(e))
try:
    b = io.BytesIO()
    im.save(b, "DDS", pixel_format="BC9")
    print("BC9 ERR-free OK")
except Exception as e:
    print("BC9 ERR", type(e).__name__, str(e))

print("== DXT saves (RGB/RGBA/L 4x4) ==")
rgb = Image.new("RGB", (4, 4))
rgb.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12),
             (13, 14, 15), (16, 17, 18), (19, 20, 21), (22, 23, 24),
             (25, 26, 27), (28, 29, 30), (31, 32, 33), (34, 35, 36),
             (37, 38, 39), (40, 41, 42), (43, 44, 45), (46, 47, 48)])
rgba = Image.new("RGBA", (4, 4))
rgba.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16),
              (17, 18, 19, 20), (21, 22, 23, 24), (25, 26, 27, 28), (29, 30, 31, 32),
              (33, 34, 35, 36), (37, 38, 39, 40), (41, 42, 43, 44), (45, 46, 47, 48),
              (49, 50, 51, 52), (53, 54, 55, 56), (57, 58, 59, 60), (61, 62, 63, 64)])
l = Image.new("L", (4, 4))
l.putdata(list(range(16)))
for fmt in ["DXT1", "DXT3", "DXT5"]:
    b = io.BytesIO()
    rgb.save(b, "DDS", pixel_format=fmt)
    dump("rgb-" + fmt, b.getvalue())
    ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
    print("rgb-" + fmt + " reopen:", [ro.getpixel((x, y)) for y in range(4) for x in range(4)])
    b = io.BytesIO()
    rgba.save(b, "DDS", pixel_format=fmt)
    dump("rgba-" + fmt, b.getvalue())
    ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
    print("rgba-" + fmt + " reopen:", [ro.getpixel((x, y)) for y in range(4) for x in range(4)])
b = io.BytesIO()
l.save(b, "DDS", pixel_format="DXT1")
dump("l-dxt1", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("l-dxt1 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(4) for x in range(4)])

print("== BC2/BC3/BC5 saves ==")
for fmt in ["BC2", "BC3"]:
    b = io.BytesIO()
    rgba.save(b, "DDS", pixel_format=fmt)
    dump("rgba-" + fmt, b.getvalue())
    ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
    print("rgba-" + fmt + " reopen:", [ro.getpixel((x, y)) for y in range(4) for x in range(4)])
b = io.BytesIO()
rgb.save(b, "DDS", pixel_format="BC5")
dump("rgb-bc5", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("rgb-bc5 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(4) for x in range(4)])

print("== DXT1 RGBA with transparency (endpoint swap quirk) ==")
rgba_t = Image.new("RGBA", (4, 4))
rgba_t.putdata([(10, 20, 30, 0), (10, 20, 30, 255)] * 8)
b = io.BytesIO()
rgba_t.save(b, "DDS", pixel_format="DXT1")
dump("rgba-t-dxt1", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("rgba-t-dxt1 reopen:", [ro.getpixel((x, y)) for y in range(4) for x in range(4)])

print("== partial block edge (3x2) DXT1 ==")
small = Image.new("RGB", (3, 2))
small.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
b = io.BytesIO()
small.save(b, "DDS", pixel_format="DXT1")
dump("small-dxt1", b.getvalue())
ro = Image.open(io.BytesIO(b.getvalue())); ro.load()
print("small-dxt1 reopen:", ro.mode, ro.size, [ro.getpixel((x, y)) for y in range(2) for x in range(3)])

print("== open errors ==")
blob = b"XXXX" + struct.pack("<I", 100) + b"\x00" * 96
try:
    im = Image.open(io.BytesIO(blob)); im.load()
    print("hdr100 OK")
except Exception as e:
    print("hdr100 ERR", type(e).__name__, str(e))
blob = b"DDS " + struct.pack("<I", 124) + b"\x00" * 100
try:
    im = Image.open(io.BytesIO(blob)); im.load()
    print("short-hdr OK")
except Exception as e:
    print("short-hdr ERR", type(e).__name__, str(e))
blob = b"XXXX"
try:
    im = Image.open(io.BytesIO(blob)); im.load()
    print("badmagic OK")
except Exception as e:
    print("badmagic ERR", type(e).__name__, str(e))
# unknown pixel format flags (no RGB/LUM/PAL8/FOURCC)
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0, 0, 0) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
try:
    im = Image.open(io.BytesIO(hdr)); im.load()
    print("pfflags0 OK")
except Exception as e:
    print("pfflags0 ERR", type(e).__name__, str(e))
# unimplemented fourcc
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DXT9")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
try:
    im = Image.open(io.BytesIO(hdr)); im.load()
    print("fourcc9 OK")
except Exception as e:
    print("fourcc9 ERR", type(e).__name__, str(e))
# unsupported luminance bitcount
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20000, 0, 24) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
try:
    im = Image.open(io.BytesIO(hdr)); im.load()
    print("lum24 OK")
except Exception as e:
    print("lum24 ERR", type(e).__name__, str(e))
# unimplemented DXGI format
dxgi = struct.pack("<I", 999)
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DX10")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + dxgi + b"\x00" * 16
try:
    im = Image.open(io.BytesIO(hdr)); im.load()
    print("dxgi999 OK")
except Exception as e:
    print("dxgi999 ERR", type(e).__name__, str(e))

print("== raw RGB565 mask reopen ==")
# RGB 565: bitcount 16, masks R=0xF800 G=0x7E0 B=0x1F
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 2, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x40, 0, 16) + struct.pack("<4I", 0xF800, 0x7E0, 0x1F, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
data = struct.pack("<HHHH", 0xF800, 0x07E0, 0x001F, 0xFFFF)
im = Image.open(io.BytesIO(hdr + data)); im.load()
print("rgb565:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== L8 luminance reopen ==")
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 2, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20000, 0, 8) + struct.pack("<4I", 0xFF000000, 0xFF000000, 0xFF000000, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + bytes([10, 20, 30, 40]))); im.load()
print("l8:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== LA16 luminance reopen ==")
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 4, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20002, 0, 16) + struct.pack("<4I", 0xFF, 0xFF, 0xFF, 0xFF000000) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + bytes([10, 4, 20, 5, 30, 6, 40, 7]))); im.load()
print("la16:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)])

print("== P8 palette reopen ==")
pal = b"".join(bytes([i, i, i, 255]) for i in range(256))
hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x1007, 2, 2, 2, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x20, 0, 8) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0)
im = Image.open(io.BytesIO(hdr + pal + bytes([0, 1, 2, 3]))); im.load()
print("p8:", im.mode, im.size, [im.getpixel((x, y)) for y in range(2) for x in range(2)], list(im.getpalette("RGBA")[:8]))
