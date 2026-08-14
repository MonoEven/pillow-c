"""BEHAV-OPEN-002 cross-check: our openers vs the Pillow oracle pins."""
import ctypes
import json
import os
import struct

dll = ctypes.CDLL(r"build\x64\Release\pillow_c.dll")
for name in ["ftex", "sun", "gbr", "fits", "xpm"]:
    fn = getattr(dll, f"pillow_c_image_open_{name}")
    fn.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    fn.restype = ctypes.c_int
dll.pillow_c_image_mode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_mode.restype = ctypes.c_int
dll.pillow_c_image_width.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_width.restype = ctypes.c_int
dll.pillow_c_image_height.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_height.restype = ctypes.c_int
dll.pillow_c_image_get_raw_bytes.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
dll.pillow_c_image_get_raw_bytes.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]

oracle = json.load(open("oracle/probe_open_mid.json"))
tmp = os.environ["TEMP"]

MODES = {1: "L", 2: "LA", 3: "RGB", 4: "RGBA", 5: "1", 6: "P", 7: "CMYK", 11: "I;16", 8: "I", 9: "F"}


def open_raw(path, rawmode):
    handle = ctypes.c_void_p()
    status = getattr(dll, "pillow_c_image_open_" + path.split(".")[0][:4])  # placeholder
    return None


def try_open(fmt, blob, rawmode):
    p = os.path.join(tmp, f"xchk.{fmt.lower()}")
    with open(p, "wb") as f:
        f.write(blob)
    handle = ctypes.c_void_p()
    status = getattr(dll, f"pillow_c_image_open_{fmt.lower()}")(p.encode(), ctypes.byref(handle))
    if status != 0:
        return ("status", status)
    mode_id = ctypes.c_int()
    dll.pillow_c_image_mode(handle, ctypes.byref(mode_id))
    mode_id = mode_id.value
    w = ctypes.c_int()
    h = ctypes.c_int()
    dll.pillow_c_image_width(handle, ctypes.byref(w))
    dll.pillow_c_image_height(handle, ctypes.byref(h))
    w = w.value
    h = h.value
    if mode_id not in MODES:
        dll.pillow_c_image_free(handle)
        return ("mode", mode_id)
    m = MODES[mode_id]
    raw = None
    if m == "P":
        raw = "P"
    elif m == "1":
        raw = "1"
    elif m == "I;16":
        raw = "I;16"
    elif m == "I":
        raw = "I"
    elif m == "F":
        raw = "F"
    else:
        raw = m
    size = w * h * (4 if m == "RGBA" else (3 if m == "RGB" else (2 if m == "I;16" else (4 if m in ("I", "F") else 1))))
    buf = ctypes.create_string_buffer(size)
    required = ctypes.c_size_t()
    st = dll.pillow_c_image_get_raw_bytes(handle, raw.encode(), buf, size, ctypes.byref(required))
    dll.pillow_c_image_free(handle)
    return (st, m, [w, h], bytes(buf).hex())


def check(name, blob, expected_ok, rawmode=None):
    got = try_open(name, blob, rawmode)
    if expected_ok is None:
        print(name, "got", got[:3] if isinstance(got, tuple) and got[0] != "status" else got)
        return
    print(name, "MATCH" if str(got)[:60] == str(expected_ok)[:60] else "DIFF", got)


# FTEX
def make_ftex(w, h, payload, fmt_id):
    return struct.pack("<6i", 0x58455446, 1, w, h, 1, 1) + struct.pack("<2i", fmt_id, 32) + struct.pack("<i", len(payload)) + payload


rgb = bytes([(x * 7 + y) % 256 for y in range(2) for x in range(3) for _ in range(3)])
r = try_open("ftex", make_ftex(3, 2, rgb, 1), None)
print("ftex_rgb", r, "EXPECT RGB [3,2]", rgb.hex())
r = try_open("ftex", make_ftex(3, 2, rgb[:10], 1), None)
print("ftex_truncated", r, "EXPECT status -29")
dxt1 = struct.pack("<HHI", 0x7C00, 0x0000, 0x00000000)
r = try_open("ftex", make_ftex(4, 4, dxt1, 0), None)
print("ftex_dxt1", r, "EXPECT RGBA 7b8200ff...")
r = try_open("ftex", struct.pack("<6i", 0x58455446, 1, 3, 2, 1, 2) + b"\x00" * 64, None)
print("ftex_multi", r, "EXPECT -30")
r = try_open("ftex", make_ftex(3, 2, rgb, 7), None)
print("ftex_bad_format", r, "EXPECT -38")

# SUN
def make_sun(w, h, depth, data, ft=1, pt=0, pl=b""):
    return struct.pack(">8I", 0x59A66A95, w, h, depth, len(data), ft, pt, len(pl)) + pl + data


r = try_open("sun", make_sun(3, 2, 1, b"\x80\x40\xc0\x00"), None)
print("sun_depth1", r, "EXPECT 1 packed 6020")
r = try_open("sun", make_sun(3, 2, 8, bytes(range(8)), 1), None)
print("sun_depth8", r, "EXPECT L 000102030405")
r = try_open("sun", make_sun(3, 2, 24, bytes(range(24)), 1), None)
print("sun_depth24_bgr", r, "EXPECT RGB bgr-order")
r = try_open("sun", make_sun(2, 1, 32, bytes([1, 2, 3, 9, 4, 5, 6, 9]), 1), None)
print("sun_depth32_bgrx", r, "EXPECT RGB 030201060504")
r = try_open("sun", make_sun(3, 2, 8, bytes([0, 1, 0, 1, 0, 1, 0, 1]), 1, 1, bytes([255, 0, 0, 0, 255, 0])), None)
print("sun_palette", r, "EXPECT P")
r = try_open("sun", make_sun(3, 2, 8, bytes([0x80, 0x02, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9]), 2), None)
print("sun_rle", r, "EXPECT L 070707080909")
r = try_open("sun", make_sun(3, 2, 8, b"\x80\x80\x80", 2), None)
print("sun_rle_short", r, "EXPECT L 808080808080")
r = try_open("sun", make_sun(3, 2, 8, b"\x01\x02"), None)
print("sun_truncated", r, "EXPECT -29")
r = try_open("sun", make_sun(3, 2, 6, b"\x00" * 4), None)
print("sun_bad_depth", r, "EXPECT -3")

# GBR
def make_gbr(w, h, depth, data, version=1, comment=b""):
    header_size = 20 + len(comment) + (1 if version == 1 else 9)
    if version == 1:
        head = struct.pack(">5I", header_size, version, w, h, depth) + comment + b"\n"
    else:
        head = struct.pack(">5I", header_size, version, w, h, depth) + b"GIMP" + struct.pack(">I", 25) + comment + b"\n"
    return head + data


r = try_open("gbr", make_gbr(3, 2, 1, bytes(range(6)), 1), None)
print("gbr_v1", r, "EXPECT L 000102030405")
r = try_open("gbr", make_gbr(3, 2, 4, bytes(range(24)), 2, b"hello"), None)
print("gbr_v2", r, "EXPECT RGBA")
r = try_open("gbr", make_gbr(3, 2, 1, b"\x01"), None)
print("gbr_short", r, "EXPECT -31")

# FITS
def make_fits(bitpix, n1, n2, payload):
    lines = ["SIMPLE  =                    T", f"BITPIX  = {bitpix:20d}", f"NAXIS   = {2:20d}", f"NAXIS1  = {n1:20d}", f"NAXIS2  = {n2:20d}", "END"]
    h = b"".join([l.ljust(80)[:80].encode() for l in lines])
    h += b" " * (2880 - len(h) % 2880)
    return h + payload


r = try_open("fits", make_fits(8, 40, 2, bytes(range(80))), None)
print("fits8", r, "EXPECT L row-flipped")
r = try_open("fits", make_fits(16, 10, 2, struct.pack(">20h", *range(20))), None)
print("fits16", r, "EXPECT I;16 BE verbatim flipped")
r = try_open("fits", make_fits(32, 10, 2, struct.pack(">20i", *range(20))), None)
print("fits32", r, "EXPECT I BE flipped")
r = try_open("fits", make_fits(-32, 10, 2, struct.pack(">20f", *[float(i) for i in range(20)])), None)
print("fitsf", r, "EXPECT F BE flipped")
r = try_open("fits", make_fits(8, 0, 0, b""), None)
print("fits_naxis0", r, "EXPECT -33")
r = try_open("fits", b"SIMPLE  =                    T"[:40], None)
print("fits_truncated_header", r, "EXPECT -33")

# XPM
def make_xpm(w, h, colors, pixels, bpp=1):
    lines = ["/* XPM */", f'"{w} {h} {len(colors)} {bpp}"']
    for key, value in colors:
        lines.append(f'"{key} c {value}",')
    lines.append("/* pixels */")
    for row in pixels:
        lines.append('"' + row + '",')
    return ("\n".join(lines) + "\n").encode()


r = try_open("xpm", make_xpm(3, 2, [("a", "#FF0000"), (".", "#00FF00")], ["a.a", ".a."]), None)
print("xpm_p", r, "EXPECT P 000100010001")
r = try_open("xpm", make_xpm(2, 1, [("a", "rgb:1/2/3")], ["aa"]), None)
print("xpm_bad_color", r, "EXPECT -35")
r = try_open("xpm", make_xpm(2, 1, [("a", "#FF0000")], ["ab"]), None)
print("xpm_unknown_key", r, "EXPECT -36")
r = try_open("xpm", make_xpm(3, 2, [("a", "#FF0000")], ["aaa"]), None)
print("xpm_truncated", r, "EXPECT -31")
r = try_open("xpm", make_xpm(3, 2, [(f"{i:03d}", f"#{(i * 3) % 256:02x}{(i * 5) % 256:02x}{(i * 7) % 256:02x}") for i in range(300)], ["000001002", "003004005"], 3), None)
print("xpm_rgb", r, "EXPECT RGB")
