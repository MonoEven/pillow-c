"""BEHAV-SAVEOPTS-003 cross-check: TIFF resolution/resolution_unit tags via
the DLL against Pillow 11.3.0."""
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
dll.pillow_c_image_save_tiff_resolution_options.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_int, ctypes.c_int]

tmp = tempfile.mkdtemp(prefix="tiffres_dll_")
im = Image.new("RGB", (8, 8), (10, 20, 30))
fx = os.path.join(tmp, "fx.png")
im.save(fx)

out = {}
h = ctypes.c_void_p()
dll.pillow_c_image_open_png(fx.encode(), ctypes.byref(h))


def dll_save(name, has_res, x, has_unit, unit):
    p = os.path.join(tmp, name)
    st = dll.pillow_c_image_save_tiff_resolution_options(h.value, p.encode(), has_res, x, x, has_unit, unit)
    if not os.path.exists(p):
        return [st, None]
    data = open(p, "rb").read()
    return [st, tiff_tags(data)]


def tiff_tags(data):
    order = "<" if data[:2] == b"II" else ">"
    n = struct.unpack(order + "H", data[8:10])[0]
    result = {}
    for i in range(n):
        rec = 10 + i * 12
        tag, typ, count = struct.unpack(order + "HHI", data[rec:rec + 8])
        val = data[rec + 8:rec + 12]
        if typ == 3:
            result[tag] = ("H", struct.unpack(order + "H", val[:2])[0])
        elif typ == 5 and count == 1:
            off = struct.unpack(order + "I", val)[0]
            num, den = struct.unpack(order + "II", data[off:off + 8])
            result[tag] = ("R", f"{num}/{den}")
        else:
            result[tag] = (typ, count)
    return result


def pill_tags(**kw):
    import io
    b = io.BytesIO()
    im.save(b, "TIFF", **kw)
    return tiff_tags(b.getvalue())


out["dll_res300"] = dll_save("d300.tiff", 1, 300.0, 0, 0)
out["ref_res300"] = pill_tags(resolution=300)
out["dll_res_unit3"] = dll_save("dunit3.tiff", 1, 300.0, 1, 3)
out["ref_res_unit3"] = pill_tags(resolution=300, resolution_unit=3)
out["dll_unit_only"] = dll_save("dunit.tiff", 0, 0.0, 1, 3)
out["ref_unit_only"] = pill_tags(resolution_unit=3)
out["dll_res_float"] = dll_save("dfloat.tiff", 1, 145.5, 0, 0)
out["ref_res_float"] = pill_tags(resolution=145.5)
out["dll_res_zero"] = dll_save("dzero.tiff", 1, 0.0, 0, 0)
out["ref_res_zero"] = pill_tags(resolution=0)
out["dll_res_1451"] = dll_save("d1451.tiff", 1, 145.1, 0, 0)
out["ref_res_1451"] = pill_tags(resolution=145.1)
out["dll_res_half"] = dll_save("dhalf.tiff", 1, 0.5, 0, 0)
out["ref_res_half"] = pill_tags(resolution=0.5)
out["dll_res_5000"] = dll_save("d5000.tiff", 1, 5000.0, 0, 0)
out["ref_res_5000"] = pill_tags(resolution=5000)
out["dll_res_9600_5"] = dll_save("d9600.tiff", 1, 9600.5, 0, 0)
out["ref_res_9600_5"] = pill_tags(resolution=9600.5)
out["dll_unit_zero"] = dll_save("du0.tiff", 0, 0.0, 1, 0)
out["ref_unit_zero"] = pill_tags(resolution_unit=0)
out["dll_unit_65535"] = dll_save("du65535.tiff", 0, 0.0, 1, 65535)
out["ref_unit_65535"] = pill_tags(resolution_unit=65535)
out["dll_res_neg_status"] = dll_save("dneg.tiff", 1, -300.0, 0, 0)[0]
dll.pillow_c_image_free(h.value)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_tiffres_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
