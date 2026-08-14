"""BEHAV-SAVEOPTS-004 cross-check: TIFF named-tag kwargs and the per-axis
resolution surface via the DLL against Pillow 11.3.0."""
import ctypes
import json
import os
import struct
import tempfile

from PIL import Image

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_save_tiff_named_options.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_double,
    ctypes.c_int, ctypes.c_double, ctypes.c_int, ctypes.c_int,
    ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_void_p),
    ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
]

tmp = tempfile.mkdtemp(prefix="tiffnamed_dll_")
src = os.path.join(tmp, "src.png")
Image.new("RGB", (8, 8), (10, 20, 30)).save(src)
h = ctypes.c_void_p()
dll.pillow_c_image_open_png(src.encode(), ctypes.byref(h))


def tiff_tags(data):
    order = "<" if data[:2] == b"II" else ">"
    ifd = struct.unpack(order + "I", data[4:8])[0]
    n = struct.unpack(order + "H", data[ifd:ifd + 2])[0]
    result = {}
    for i in range(n):
        rec = ifd + 2 + i * 12
        tag, typ, count = struct.unpack(order + "HHI", data[rec:rec + 8])
        val = data[rec + 8:rec + 12]
        if typ == 3 and count == 1:
            result[tag] = ("H", struct.unpack(order + "H", val[:2])[0])
        elif typ == 5 and count == 1:
            off = struct.unpack(order + "I", val)[0]
            result[tag] = ("R", struct.unpack(order + "II", data[off:off + 8]))
        elif typ == 2:
            if count <= 4:
                result[tag] = ("A", data[rec + 8:rec + 8 + count])
            else:
                off = struct.unpack(order + "I", val)[0]
                result[tag] = ("A", data[off:off + count])
        else:
            result[tag] = (typ, count)
    return result


def cbuf(data):
    if data is None:
        return None
    return ctypes.create_string_buffer(bytes(data))


def dll_save(name, has_x, x, has_y, y, has_unit, unit, compression, icc, tags_values):
    p = os.path.join(tmp, name)
    tag_arr = (ctypes.c_int * len(tags_values))(*[t for t, _ in tags_values])
    ptrs = (ctypes.c_void_p * len(tags_values))(
        *[ctypes.cast(cbuf(v), ctypes.c_void_p) for _, v in tags_values])
    sizes = (ctypes.c_size_t * len(tags_values))(*[len(v) for _, v in tags_values])
    icc_buf = cbuf(icc)
    st = dll.pillow_c_image_save_tiff_named_options(
        h.value, p.encode(), has_x, x, has_y, y, has_unit, unit,
        compression, ctypes.cast(icc_buf, ctypes.c_void_p) if icc_buf is not None else None,
        len(icc) if icc is not None else 0,
        tag_arr, ptrs, sizes, len(tags_values))
    if not os.path.exists(p):
        return [st, None]
    return [st, tiff_tags(open(p, "rb").read())]


def pill_tags(**kw):
    import io
    b = io.BytesIO()
    Image.open(src).save(b, "TIFF", **kw)
    return tiff_tags(b.getvalue())


def only_interesting(tags):
    if tags is None:
        return None
    wanted = {259, 270, 282, 283, 296, 305, 306, 315, 320, 33432, 34675}
    return {k: v for k, v in tags.items() if k in wanted}


out = {}
out["dll_all5"] = only_interesting(dll_save("all5.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [
    (270, b"d\0"), (305, b"s\0"), (306, b"2026:01:02 03:04:05\0"), (315, b"a\0"), (33432, b"c\0")])[1])
out["ref_all5"] = only_interesting(pill_tags(description="d", software="s", artist="a", copyright="c", date_time="2026:01:02 03:04:05"))
out["dll_long"] = only_interesting(dll_save("long.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [
    (270, b"a long description longer than four bytes\0")])[1])
out["ref_long"] = only_interesting(pill_tags(description="a long description longer than four bytes"))
out["dll_empty"] = only_interesting(dll_save("empty.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [(270, b"\0")])[1])
out["ref_empty"] = only_interesting(pill_tags(description=""))
out["dll_int"] = only_interesting(dll_save("int.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [(270, b"5\0")])[1])
out["ref_int"] = only_interesting(pill_tags(description=5))
out["dll_bytes"] = only_interesting(dll_save("bytes.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [(270, b"abc\0")])[1])
out["ref_bytes"] = only_interesting(pill_tags(description=b"abc"))
out["dll_nonascii"] = only_interesting(dll_save("nonascii.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [(270, b"h?llo\0")])[1])
out["ref_nonascii"] = only_interesting(pill_tags(description="h\u00e9llo"))
out["dll_trunc"] = only_interesting(dll_save("trunc.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, None, [(305, b"a\0")])[1])
out["ref_trunc"] = only_interesting(pill_tags(software=["a", "b"]))
out["dll_xonly"] = only_interesting(dll_save("xonly.tiff", 1, 300.0, 0, 0.0, 0, 0, 1, None, [])[1])
out["ref_xonly"] = only_interesting(pill_tags(x_resolution=300))
out["dll_yonly"] = only_interesting(dll_save("yonly.tiff", 0, 0.0, 1, 150.0, 0, 0, 1, None, [])[1])
out["ref_yonly"] = only_interesting(pill_tags(y_resolution=150))
out["dll_xy"] = only_interesting(dll_save("xy.tiff", 1, 145.5, 1, 72.5, 0, 0, 1, None, [])[1])
out["ref_xy"] = only_interesting(pill_tags(x_resolution=145.5, y_resolution=72.5))
out["dll_res_plus_x"] = only_interesting(dll_save("rpx.tiff", 1, 111.0, 1, 300.0, 0, 0, 1, None, [])[1])
out["ref_res_plus_x"] = only_interesting(pill_tags(resolution=300, x_resolution=111))
out["dll_x_unit"] = only_interesting(dll_save("xu.tiff", 1, 300.0, 0, 0.0, 1, 3, 1, None, [])[1])
out["ref_x_unit"] = only_interesting(pill_tags(x_resolution=300, resolution_unit=3))
out["dll_desc_res_unit"] = only_interesting(dll_save("dru.tiff", 1, 145.5, 1, 145.5, 1, 3, 1, None, [
    (270, b"longer than four bytes\0")])[1])
out["ref_desc_res_unit"] = only_interesting(pill_tags(resolution=145.5, resolution_unit=3, description="longer than four bytes"))
out["dll_dpi_res"] = only_interesting(dll_save("dpir.tiff", 1, 300.0, 1, 150.0, 1, 2, 1, None, [])[1])
out["ref_dpi_res"] = only_interesting(pill_tags(dpi=(300, 150), resolution=99))
out["dll_desc_dpi"] = only_interesting(dll_save("dd.tiff", 1, 300.0, 1, 150.0, 1, 2, 1, None, [(270, b"d\0")])[1])
out["ref_desc_dpi"] = only_interesting(pill_tags(description="d", dpi=(300, 150)))
out["dll_packbits"] = only_interesting(dll_save("pb.tiff", 0, 0.0, 0, 0.0, 0, 0, 32773, None, [(315, b"artist\0")])[1])
out["ref_packbits"] = only_interesting(pill_tags(artist="artist", compression="packbits"))
out["dll_icc"] = only_interesting(dll_save("icc.tiff", 0, 0.0, 0, 0.0, 0, 0, 1, b"dummyicc\x00\x01\x02", [(33432, b"(c)\0")])[1])
out["ref_icc"] = only_interesting(pill_tags(copyright="(c)", icc_profile=b"dummyicc\x00\x01\x02"))
dll.pillow_c_image_free(h.value)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_tiffnamed_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
