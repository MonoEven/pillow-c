"""BEHAV-SAVEOPTS-001 ctypes cross-check: QOI colorspace + TGA options
against Pillow 11.3.0."""
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
dll.pillow_c_image_save_qoi.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_qoi_options.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
dll.pillow_c_image_save_tga.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_tga_full_options.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]

out = {}
tmp = tempfile.mkdtemp(prefix="saveopts_dll_")

# fixture: 2x2 RGB with distinct rows to expose orientation flips
im = Image.new("RGB", (2, 2))
im.putpixel((0, 0), (10, 0, 0))
im.putpixel((1, 0), (20, 0, 0))
im.putpixel((0, 1), (30, 0, 0))
im.putpixel((1, 1), (40, 0, 0))
png = os.path.join(tmp, "fx.png")
im.save(png)

handle = ctypes.c_void_p()
dll.pillow_c_image_open_png(png.encode(), ctypes.byref(handle))


def dll_save(name, export, *args):
    path = os.path.join(tmp, name)
    st = export(handle.value, path.encode(), *args)
    data = open(path, "rb").read()
    return [st, data.hex()[:80], len(data)]


out["qoi_default"] = dll_save("d_default.qoi", dll.pillow_c_image_save_qoi)
out["qoi_srgb"] = dll_save("d_srgb.qoi", dll.pillow_c_image_save_qoi_options, 0)
out["qoi_linear"] = dll_save("d_linear.qoi", dll.pillow_c_image_save_qoi_options, 1)
out["tga_default"] = dll_save("d_default.tga", dll.pillow_c_image_save_tga)
out["tga_orient2"] = dll_save("d_o2.tga", dll.pillow_c_image_save_tga_full_options, 0, None, 0, 2)
idbuf = ctypes.create_string_buffer(b"abc")
out["tga_id"] = dll_save("d_id.tga", dll.pillow_c_image_save_tga_full_options, 0, idbuf, 3, -1)
out["tga_id_orient2"] = dll_save("d_ido2.tga", dll.pillow_c_image_save_tga_full_options, 0, idbuf, 3, 2)
out["tga_rle"] = dll_save("d_rle.tga", dll.pillow_c_image_save_tga_full_options, 1, None, 0, -1)
dll.pillow_c_image_free(handle.value)

# Pillow references
def pill_save(name, fmt, **kw):
    path = os.path.join(tmp, name)
    im.save(path, fmt, **kw)
    data = open(path, "rb").read()
    return [data.hex()[:80], len(data)]


out["ref_qoi_default"] = pill_save("p_default.qoi", "QOI")
out["ref_qoi_srgb"] = pill_save("p_srgb.qoi", "QOI", colorspace="sRGB")
out["ref_qoi_linear"] = pill_save("p_linear.qoi", "QOI", colorspace="linear")
out["ref_tga_default"] = pill_save("p_default.tga", "TGA")
out["ref_tga_orient2"] = pill_save("p_o2.tga", "TGA", orientation=2)
out["ref_tga_id"] = pill_save("p_id.tga", "TGA", id_section=b"abc")
out["ref_tga_id_orient2"] = pill_save("p_ido2.tga", "TGA", id_section=b"abc", orientation=2)
out["ref_tga_rle"] = pill_save("p_rle.tga", "TGA", rle=True)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_saveopts_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
