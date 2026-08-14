"""Generate BEHAV-DDS-001 fixtures with local Pillow 11.3.0 and patch them
into the AHK test placeholders.

Save fixtures come straight from Pillow; crafted open fixtures (masks,
luminance, P8, BC4, BC5S, BC7, error shapes) are built here and
their Pillow decode outputs are recorded back into the test value
placeholders. (BC6H/BC6HS open is a documented deferred child.)
"""

import io
import struct

from PIL import Image

fixes = {}
values = {}


def save_fix(key, im, **opts):
    buf = io.BytesIO()
    im.save(buf, "DDS", **opts)
    fixes[key] = buf.getvalue().hex()


def hdr(flags, w, h, pitch, pfflags, fourcc, bitcount, masks, dxgi=None):
    out = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, flags, h, w, pitch, 0, 0)
    out += struct.pack("11I", *((0,) * 11))
    out += struct.pack("<4I", 32, pfflags, fourcc, bitcount)
    out += struct.pack("<4I", *masks)
    out += struct.pack("<5I", 0x1000, 0, 0, 0, 0)
    if dxgi is not None:
        out += struct.pack("<5I", dxgi, 3, 0, 0, 1)
    return out


im = Image.new("RGB", (2, 2))
im.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12)])
save_fix("DDS_RGB_RAW_FIX", im)

im = Image.new("RGBA", (2, 2))
im.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16)])
save_fix("DDS_RGBA_RAW_FIX", im)

im = Image.new("L", (2, 2))
im.putdata([10, 20, 30, 40])
save_fix("DDS_L_RAW_FIX", im)

im = Image.new("LA", (2, 2))
im.putdata([(10, 4), (20, 5), (30, 6), (40, 7)])
save_fix("DDS_LA_RAW_FIX", im)

rgb4 = Image.new("RGB", (4, 4))
rgb4.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12),
              (13, 14, 15), (16, 17, 18), (19, 20, 21), (22, 23, 24),
              (25, 26, 27), (28, 29, 30), (31, 32, 33), (34, 35, 36),
              (37, 38, 39), (40, 41, 42), (43, 44, 45), (46, 47, 48)])
rgba4 = Image.new("RGBA", (4, 4))
rgba4.putdata([(1, 2, 3, 4), (5, 6, 7, 8), (9, 10, 11, 12), (13, 14, 15, 16),
               (17, 18, 19, 20), (21, 22, 23, 24), (25, 26, 27, 28), (29, 30, 31, 32),
               (33, 34, 35, 36), (37, 38, 39, 40), (41, 42, 43, 44), (45, 46, 47, 48),
               (49, 50, 51, 52), (53, 54, 55, 56), (57, 58, 59, 60), (61, 62, 63, 64)])
l4 = Image.new("L", (4, 4))
l4.putdata(list(range(16)))

save_fix("DDS_RGB_DXT1_FIX", rgb4, pixel_format="DXT1")
save_fix("DDS_RGBA_DXT1_FIX", rgba4, pixel_format="DXT1")
save_fix("DDS_L_DXT1_FIX", l4, pixel_format="DXT1")
save_fix("DDS_RGB_DXT3_FIX", rgb4, pixel_format="DXT3")
save_fix("DDS_RGBA_DXT3_FIX", rgba4, pixel_format="DXT3")
save_fix("DDS_RGB_DXT5_FIX", rgb4, pixel_format="DXT5")
save_fix("DDS_RGBA_DXT5_FIX", rgba4, pixel_format="DXT5")
save_fix("DDS_RGBA_BC2_FIX", rgba4, pixel_format="BC2")
save_fix("DDS_RGBA_BC3_FIX", rgba4, pixel_format="BC3")
save_fix("DDS_RGB_BC5_FIX", rgb4, pixel_format="BC5")

rgba_t = Image.new("RGBA", (4, 4))
rgba_t.putdata([(10, 20, 30, 0), (10, 20, 30, 255)] * 8)
save_fix("DDS_RGBA_T_DXT1_FIX", rgba_t, pixel_format="DXT1")

small = Image.new("RGB", (3, 2))
small.putdata([(1, 2, 3), (4, 5, 6), (7, 8, 9), (10, 11, 12), (13, 14, 15), (16, 17, 18)])
save_fix("DDS_SMALL_DXT1_FIX", small, pixel_format="DXT1")

# ---- crafted open fixtures ----

# rgb565 masks
fixes["DDS_RGB565_FIX"] = (
    hdr(0x1007, 2, 2, 4, 0x40, 0, 16, [0xF800, 0x7E0, 0x1F, 0])
    + struct.pack("<HHHH", 0xF800, 0x07E0, 0x001F, 0xFFFF)
).hex()

# L8 luminance
fixes["DDS_L8_FIX"] = (
    hdr(0x100F, 2, 2, 2, 0x20000, 0, 8, [0xFF000000] * 3 + [0])
    + bytes([10, 20, 30, 40])
).hex()

# LA16 luminance
fixes["DDS_LA16_FIX"] = (
    hdr(0x100F, 2, 2, 4, 0x20001, 0, 16, [0xFF, 0xFF, 0xFF, 0xFF000000])
    + bytes([10, 4, 20, 5, 30, 6, 40, 7])
).hex()

# P8 palette
pal = b"".join(bytes([i, i, i, 255]) for i in range(256))
fixes["DDS_P8_FIX"] = (
    hdr(0x1007, 2, 2, 2, 0x20, 0, 8, [0] * 4) + pal + bytes([0, 1, 2, 3])
).hex()


def alpha_block(a0, a1, inds):
    lut1 = sum(v << (3 * n) for n, v in enumerate(inds[:8]))
    lut2 = sum(v << (3 * n) for n, v in enumerate(inds[8:]))
    return bytes([a0, a1]) + bytes([
        lut1 & 0xFF, (lut1 >> 8) & 0xFF, (lut1 >> 16) & 0xFF,
        lut2 & 0xFF, (lut2 >> 8) & 0xFF, (lut2 >> 16) & 0xFF,
    ])


# BC4
fixes["DDS_BC4_FIX"] = (
    hdr(0x81007, 4, 4, 16, 0x4, struct.unpack("<I", b"BC4U")[0], 8, [0] * 4)
    + alpha_block(16, 240, [1] * 8 + [4] * 4 + [0] + [2] * 3)
).hex()

# BC5S (signed alpha blocks)
b0 = alpha_block(200, 100, [1] * 16)   # int8: -56, +100
b1 = alpha_block(128, 64, [0] * 16)    # int8: -128, +64
fixes["DDS_BC5S_FIX"] = (
    hdr(0x81007, 4, 4, 16, 0x4, struct.unpack("<I", b"BC5S")[0], 24, [0] * 4) + b0 + b1
).hex()
im = Image.open(io.BytesIO(bytes.fromhex(fixes["DDS_BC5S_FIX"]))); im.load()
values["DDS_BC5S_P0_R"] = im.getpixel((0, 0))[0]
values["DDS_BC5S_P0_G"] = im.getpixel((0, 0))[1]
values["DDS_BC5S_P1_R"] = im.getpixel((3, 3))[0]
values["DDS_BC5S_P1_G"] = im.getpixel((3, 3))[1]
print("bc5s pixels:", im.getpixel((0, 0)), im.getpixel((3, 3)))


def put_bits(bits, pos, value, count):
    for i in range(count):
        bits[pos + i] = (value >> i) & 1


def bc7_mode5_block(ep0, ep1, color_idx, alpha_idx):
    # mode 5: ns=1, pb=0, rb=2, isb=0, cb=7, ab=8, ib=2, ib2=2.
    # The mode field is the first-set-bit pattern in byte 0; the C
    # decoder reads the data fields starting at bit (mode + 1) = 6.
    bits = [0] * 128
    pos = 6
    put_bits(bits, pos, 0, 2)  # rotation
    pos += 2
    for ch in range(3):
        put_bits(bits, pos, ep0[ch], 7)
        put_bits(bits, pos + 7, ep1[ch], 7)
        pos += 14
    put_bits(bits, pos, ep0[3], 8)
    put_bits(bits, pos + 8, ep1[3], 8)
    pos += 16
    for i in range(16):
        put_bits(bits, pos, color_idx[i], 2 if i > 0 else 1)
        pos += 2 if i > 0 else 1
    for i in range(16):
        put_bits(bits, pos, alpha_idx[i], 2 if i > 0 else 1)
        pos += 2 if i > 0 else 1
    data = bytearray(16)
    for b, v in enumerate(bits):
        data[b >> 3] |= v << (b & 7)
    data[0] |= 0x20
    return bytes(data)


# BC7 mode 5: distinct endpoints and a gradient of indices
block = bc7_mode5_block(
    (0, 0, 0, 255), (120, 60, 30, 16),
    [0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3],
    [3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0, 3, 2, 1, 0],
)
fixes["DDS_BC7_FIX"] = (
    hdr(0x81007, 4, 4, 16, 0x4, struct.unpack("<I", b"DX10")[0], 32, [0] * 4, dxgi=98)
    + block
).hex()
im = Image.open(io.BytesIO(bytes.fromhex(fixes["DDS_BC7_FIX"]))); im.load()
for key, xy in [("P0", (0, 0)), ("P1", (3, 3))]:
    px = im.getpixel(xy)
    for ch_i, ch in enumerate(["R", "G", "B", "A"]):
        values[f"DDS_BC7_{key}_{ch}"] = px[ch_i]
print("bc7 pixels:", im.getpixel((0, 0)), im.getpixel((3, 3)))

# ---- error fixtures ----
fixes["DDS_HDR100_FIX"] = (b"DDS " + struct.pack("<I", 100) + b"\x00" * 120).hex()
fixes["DDS_SHORTHDR_FIX"] = (b"DDS " + struct.pack("<I", 124) + b"\x00" * 100).hex()
fixes["DDS_PFFLAGS0_FIX"] = hdr(0x1007, 2, 2, 4, 0, 0, 0, [0] * 4).hex()
fixes["DDS_FOURCC9_FIX"] = hdr(0x1007, 2, 2, 4, 0x4, struct.unpack("<I", b"DXT9")[0], 32, [0] * 4).hex()
fixes["DDS_LUM24_FIX"] = hdr(0x1007, 2, 2, 4, 0x20000, 0, 24, [0] * 4).hex()
fixes["DDS_DXGI999_FIX"] = hdr(0x1007, 2, 2, 4, 0x4, struct.unpack("<I", b"DX10")[0], 32, [0] * 4, dxgi=999).hex()
# BC6H open is a documented deferred child (Pillow decodes it; the
# facade fails loudly with the NotImplementedError shape).
fixes["DDS_DXGI95_FIX"] = (hdr(0x1007, 4, 4, 16, 0x4, struct.unpack("<I", b"DX10")[0], 32, [0] * 4, dxgi=95) + b"\x00" * 16).hex()
fixes["DDS_TRUNC_FIX"] = (hdr(0x81007, 4, 8, 28, 0x4, struct.unpack("<I", b"DXT1")[0], 32, [0] * 4) + bytes(range(12))).hex()
fixes["DDS_TRUNCL8_FIX"] = (hdr(0x100F, 2, 2, 2, 0x20000, 0, 8, [0xFF000000] * 3 + [0]) + bytes([10, 20])).hex()
fixes["DDS_ZEROMASK_FIX"] = (hdr(0x1007, 2, 2, 4, 0x41, 0, 16, [0, 0xF00, 0xF0, 0xF]) + b"\x00" * 8).hex()
fixes["DDS_BADMAGIC_FIX"] = (b"XXXX" + b"\x00" * 124).hex()

for name, hexed in sorted(fixes.items()):
    print(name, len(hexed) // 2)

src = open(r"ahk\pillow.test.ahk", encoding="utf-8").read()
for name, hexed in fixes.items():
    if name in src:
        src = src.replace(name, hexed)
for name, value in values.items():
    if name in src:
        src = src.replace(name, str(value))
open(r"ahk\pillow.test.ahk", "w", encoding="utf-8", newline="\n").write(src)
print("patched")
