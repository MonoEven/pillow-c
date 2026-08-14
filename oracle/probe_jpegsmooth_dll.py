"""BEHAV-SAVEOPTS-005 cross-check: JPEG smooth/streamtype via the DLL against
Pillow 11.3.0 at the marker-structure and decode level (JPEG payload bytes
are not byte-identical to libjpeg by design; see the gap ledger)."""
import ctypes
import io
import json
import os
import struct
import tempfile

from PIL import Image

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_save_jpeg_smooth_streamtype_options.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int,
    ctypes.c_double, ctypes.c_double, ctypes.c_int, ctypes.c_int,
    ctypes.c_int, ctypes.c_int, ctypes.c_int,
]

tmp = tempfile.mkdtemp(prefix="jpegsmooth_dll_")
im = Image.new("RGB", (32, 24))
px = im.load()
for y in range(24):
    for x in range(32):
        px[x, y] = ((x * 7 + y * 13) % 256, (x * 3 + y * 29) % 256,
                    (x * 11 + y * 5) % 256)
src = os.path.join(tmp, "src.png")
im.save(src)
h = ctypes.c_void_p()
dll.pillow_c_image_open_png(src.encode(), ctypes.byref(h))


def marker_list(data):
    out = []
    i = 0
    n = len(data)
    while i < n:
        if data[i] != 0xFF:
            i += 1
            continue
        j = i
        while j < n and data[j] == 0xFF:
            j += 1
        if j >= n:
            break
        m = data[j]
        if m == 0x00 or (0xD0 <= m <= 0xD7) or m == 0x01:
            i = j + 1
            continue
        out.append(m)
        if m in (0xD8, 0xD9):
            i = j + 1
            continue
        if j + 2 >= n:
            break
        length = (data[j + 1] << 8) | data[j + 2]
        i = j + 1 + length
    return out


def dll_save(name, quality, subsampling, progressive, optimize, smooth, streamtype):
    p = os.path.join(tmp, name)
    st = dll.pillow_c_image_save_jpeg_smooth_streamtype_options(
        h.value, p.encode(), quality, 0, 0.0, 0.0, subsampling, progressive,
        optimize, smooth, streamtype)
    if not os.path.exists(p):
        return [st, None]
    return [st, open(p, "rb").read()]


def pill_bytes(**kw):
    b = io.BytesIO()
    im.save(b, "JPEG", **kw)
    return b.getvalue()


def decode_ok(data):
    try:
        r = Image.open(io.BytesIO(data))
        r.load()
        return [r.size, r.mode]
    except Exception as e:
        return ["ERROR", type(e).__name__, str(e)[:80]]


out = {}
# streamtype marker structures (0/1/2) against Pillow
for st in [0, 1, 2]:
    ours = dll_save(f"st{st}.jpg", 75, -1, -1, -1, 0, st)[1]
    theirs = pill_bytes(streamtype=st)
    out[f"dll_st{st}"] = marker_list(ours)
    out[f"ref_st{st}"] = marker_list(theirs)
    out[f"dll_st{st}_decode"] = decode_ok(ours)
    out[f"ref_st{st}_decode"] = decode_ok(theirs)
# streamtype with metadata (exif APP1/COM survive, ICC/XMP suppressed in
# streamtype 2) is a facade-level concern: the DLL smooth/streamtype export
# takes no metadata, and the facade routes metadata combinations through
# the older patch exports (documented bounded divergence in the ledger).

# smooth sanity: changes bytes, decodes, and differs from smooth=0
plain = dll_save("plain.jpg", 75, -1, -1, -1, 0, 0)[1]
s50 = dll_save("s50.jpg", 75, -1, -1, -1, 50, 0)[1]
s100 = dll_save("s100.jpg", 75, -1, -1, -1, 100, 0)[1]
sneg = dll_save("sneg.jpg", 75, -1, -1, -1, -1, 0)[1]
out["dll_smooth_plain_decode"] = decode_ok(plain)
out["dll_smooth_50_differs"] = plain != s50
out["dll_smooth_50_decode"] = decode_ok(s50)
out["dll_smooth_100_differs_from_50"] = s50 != s100
out["dll_smooth_neg_decode"] = decode_ok(sneg)
out["dll_smooth_neg_differs"] = plain != sneg
# 4:4:4 and L smooth still decode
s444 = dll_save("s444.jpg", 75, 0, -1, -1, 50, 0)[1]
out["dll_smooth_444_decode"] = decode_ok(s444)
# smooth out-of-range values are accepted (no clamp at the encoder level)
s101 = dll_save("s101.jpg", 75, -1, -1, -1, 101, 0)[1]
out["dll_smooth_101_ok"] = len(s101) > 0 and decode_ok(s101)[0] != "ERROR"
# tables + image concatenation decodes to the same size as the plain save
tables = dll_save("tables.jpg", 75, -1, -1, -1, 0, 1)[1]
image_only = dll_save("imageonly.jpg", 75, -1, -1, -1, 0, 2)[1]
out["dll_concat_decode"] = decode_ok(tables + image_only)
# streamtype values outside 1/2 keep the interchange stream
st5 = dll_save("st5.jpg", 75, -1, -1, -1, 0, 5)[1]
out["dll_st5_markers"] = marker_list(st5)
out["ref_st0_markers"] = marker_list(pill_bytes())
dll.pillow_c_image_free(h.value)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_jpegsmooth_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
