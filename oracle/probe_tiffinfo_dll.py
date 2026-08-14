"""BEHAV-SAVEOPTS-006 cross-check: TIFF tiffinfo arbitrary tags via the DLL
raw-entries patch against Pillow 11.3.0."""
import ctypes
import io
import json
import math
import os
import struct
import tempfile

from PIL import Image

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_save_tiff.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_tiff.restype = ctypes.c_int
dll.pillow_c_image_patch_tiff_raw_entries.argtypes = [
    ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int),
    ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_size_t),
    ctypes.c_void_p, ctypes.c_size_t, ctypes.c_size_t,
]
dll.pillow_c_tiff_rational_from_double.argtypes = [
    ctypes.c_double, ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32),
]

tmp = tempfile.mkdtemp(prefix="tiffinfo_dll_")
src = os.path.join(tmp, "src.png")
Image.new("RGB", (8, 8), (10, 20, 30)).save(src)
h = ctypes.c_void_p()
dll.pillow_c_image_open_png(src.encode(), ctypes.byref(h))


def rational_from_double(v):
    n = ctypes.c_uint32(0)
    d = ctypes.c_uint32(1)
    dll.pillow_c_tiff_rational_from_double(v, ctypes.byref(n), ctypes.byref(d))
    return n.value, d.value


def classify(tag, value):
    """Mirrors the facade's SaveTiffInfoEntries classification."""
    if tag in (282, 283):
        first = value[0] if isinstance(value, (list, tuple)) else value
        if isinstance(first, int):
            num, den = first, 1
        else:
            num, den = rational_from_double(float(first))
        return (5, 1, struct.pack("<II", num, den))
    if tag in (270, 305, 306, 315, 33432):
        if isinstance(value, (list, tuple)):
            value = value[0]
        if isinstance(value, int):
            value = str(value)
        if isinstance(value, bytes):
            raw = value + b"\0"
        else:
            raw = value.encode("ascii", "replace") + b"\0"
        return (2, len(raw), raw)
    if tag == 296:
        if not isinstance(value, int):
            raise TypeError("required argument is not an integer")
        if not 0 <= value <= 65535:
            raise struct.error("ushort format requires 0 <= number <= 0xffff")
        return (3, 1, struct.pack("<H", value))
    if isinstance(value, (list, tuple)):
        if len(value) == 0:
            return (5, 0, b"")
        if all(isinstance(v, str) for v in value):
            raise TypeError("ImageFileDirectory_v2.write_string() takes 2 positional arguments but %d were given" % (len(value) + 1))
        if all(isinstance(v, bytes) for v in value):
            value = value[0]
        elif all(isinstance(v, int) for v in value):
            if all(0 <= v < 2**16 for v in value):
                return (3, len(value), b"".join(struct.pack("<H", v) for v in value))
            if all(-(2**15) < v < 2**15 for v in value):
                return (8, len(value), b"".join(struct.pack("<h", v) for v in value))
            if all(v >= 0 for v in value):
                return (4, len(value), b"".join(struct.pack("<I", v) for v in value))
            return (9, len(value), b"".join(struct.pack("<i", v) for v in value))
        elif all(isinstance(v, float) for v in value):
            return (12, len(value), b"".join(struct.pack("<d", v) for v in value))
        else:
            raise TypeError("ImageFileDirectory_v2.write_undefined() takes 2 positional arguments but %d were given" % (len(value) + 1))
    if isinstance(value, int):
        if not -(2**31) <= value < 2**32:
            raise ValueError("argument out of range")
        if 0 <= value < 2**16:
            return (3, 1, struct.pack("<H", value))
        if -(2**15) < value < 2**15:
            return (8, 1, struct.pack("<h", value))
        if value >= 0:
            return (4, 1, struct.pack("<I", value))
        return (9, 1, struct.pack("<i", value))
    if isinstance(value, float):
        return (12, 1, struct.pack("<d", value))
    if isinstance(value, str):
        raw = value.encode("ascii", "replace") + b"\0"
        return (2, len(raw), raw)
    if isinstance(value, bytes):
        return (1, len(value), value)
    raise ValueError("unsupported")


def dll_save(name, tiffinfo):
    p = os.path.join(tmp, name)
    dll.pillow_c_image_save_tiff(h.value, p.encode())
    entries = []
    for tag, value in tiffinfo.items():
        typ, cnt, raw = classify(tag, value)
        entries.append((tag, typ, cnt, raw))
    tag_arr = (ctypes.c_int * len(entries))(*[e[0] for e in entries])
    type_arr = (ctypes.c_int * len(entries))(*[e[1] for e in entries])
    cnt_arr = (ctypes.c_uint32 * len(entries))(*[e[2] for e in entries])
    blob = b"".join(e[3] for e in entries)
    offsets = []
    cursor = 0
    for e in entries:
        offsets.append(cursor)
        cursor += len(e[3])
    off_arr = (ctypes.c_size_t * len(entries))(*offsets)
    buf = ctypes.create_string_buffer(blob if blob else b"\0")
    st = dll.pillow_c_image_patch_tiff_raw_entries(
        p.encode(), tag_arr, type_arr, cnt_arr, off_arr,
        ctypes.cast(buf, ctypes.c_void_p),
        len(blob), len(entries))
    if not os.path.exists(p):
        return [st, None]
    return [st, open(p, "rb").read()]


def tags(data):
    n = struct.unpack("<H", data[8:10])[0]
    out = {}
    for i in range(n):
        rec = 10 + i * 12
        tag, typ, cnt = struct.unpack("<HHI", data[rec:rec + 8])
        val = data[rec + 8:rec + 12]
        if typ == 2:
            raw = val if cnt <= 4 else data[struct.unpack("<I", val)[0]:][:cnt]
            out[tag] = (typ, cnt, raw.rstrip(b"\0"))
        elif typ in (3, 8) and cnt <= 2:
            fmt = "<" + ("H" if typ == 3 else "h") * cnt
            out[tag] = (typ, cnt, struct.unpack(fmt, val[:2 * cnt]))
        elif typ in (4, 9) and cnt == 1:
            fmt = "<I" if typ == 4 else "<i"
            out[tag] = (typ, cnt, struct.unpack(fmt, val)[0])
        elif typ == 12 and cnt == 1:
            off = struct.unpack("<I", val)[0]
            out[tag] = (typ, cnt, struct.unpack("<d", data[off:off + 8])[0])
        elif typ == 5 and cnt == 1:
            off = struct.unpack("<I", val)[0]
            out[tag] = (typ, cnt, struct.unpack("<II", data[off:off + 8]))
        elif typ == 1:
            raw = val if cnt <= 4 else data[struct.unpack("<I", val)[0]:][:cnt]
            out[tag] = (typ, cnt, raw)
        elif typ == 12:
            off = struct.unpack("<I", val)[0]
            out[tag] = (typ, cnt, struct.unpack("<" + "d" * cnt, data[off:off + 8 * cnt]))
        elif typ in (3, 8) and cnt > 2:
            off = struct.unpack("<I", val)[0]
            fmt = "<" + ("H" if typ == 3 else "h") * cnt
            out[tag] = (typ, cnt, struct.unpack(fmt, data[off:off + 2 * cnt]))
        elif typ in (4, 9) and cnt > 1:
            off = struct.unpack("<I", val)[0]
            fmt = "<" + ("I" if typ == 4 else "i") * cnt
            out[tag] = (typ, cnt, struct.unpack(fmt, data[off:off + 4 * cnt]))
        else:
            out[tag] = (typ, cnt)
    return out


def pill_tags(tiffinfo):
    b = io.BytesIO()
    Image.open(src).save(b, "TIFF", tiffinfo=tiffinfo)
    return tags(b.getvalue())


def interesting(d):
    skip = {256, 257, 258, 259, 262, 273, 277, 278, 279, 284}
    return {k: v for k, v in d.items() if k not in skip}


cases = {
    "int_short": {65000: 300},
    "int_long": {65000: 70000},
    "int_neg_short": {65000: -5},
    "int_neg_long": {65000: -70000},
    "float": {65000: 1.5},
    "str": {65000: "hello"},
    "str_long": {65000: "a longer string value"},
    "bytes": {65000: b"abc"},
    "int_array": {65000: [1, 2, 3]},
    "int_array_long": {65000: [70000, 2]},
    "int_array_mixed": {65000: [70000, -5]},
    "int_array_neg": {65000: [300, -5]},
    "tuple2": {65000: (300, 1)},
    "empty_str": {65000: ""},
    "neg_array": {65001: [-300, -70000]},
    "float_array": {65000: [1.5, 2.5]},
    "reg_rat_int": {282: 300},
    "reg_rat_float": {282: 145.5},
    "reg_rat_tuple": {282: (300, 1)},
    "reg_ascii": {270: "hello"},
    "reg_unit": {296: 3},
    "empty_array": {65000: []},
    "two_tags": {65000: 300, 65001: "hello"},
    "bool": {65000: True},
    "bytes_array": {65000: [b"a", b"b"]},
}
out = {}
for name, kw in cases.items():
    ours = dll_save(name + ".tiff", kw)
    out["dll_" + name] = [ours[0], interesting(tags(ours[1])) if ours[1] else None]
    try:
        out["ref_" + name] = interesting(pill_tags(kw))
    except Exception as e:
        out["ref_" + name] = ["ERROR", type(e).__name__, str(e)[:90]]
dll.pillow_c_image_free(h.value)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_tiffinfo_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
