"""BEHAV-SAVEOPTS-001 cross-check #3b: GIF interlace/palette (16x16) via the
DLL against Pillow 11.3.0."""
import ctypes
import json
import os
import tempfile

from PIL import Image

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_save_gif_interlace_palette_options.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t]

out = {}
tmp = tempfile.mkdtemp(prefix="gifopts2_dll_")

im = Image.new("P", (16, 16))
im.putpalette([(i * 32) % 256 for i in range(24)])
for x in range(16):
    for y in range(16):
        im.putpixel((x, y), (x + y) % 4)
fixture = os.path.join(tmp, "fx.png")
im.save(fixture)

handle = ctypes.c_void_p()
assert dll.pillow_c_image_open_png(fixture.encode(), ctypes.byref(handle)) == 0


def dll_save(name, interlace, palette=None):
    path = os.path.join(tmp, name)
    pal = None
    pal_size = 0
    if palette is not None:
        pal = ctypes.create_string_buffer(bytes(palette))
        pal_size = len(palette)
    st = dll.pillow_c_image_save_gif_interlace_palette_options(handle.value, path.encode(), 0, 0, interlace, pal, pal_size)
    data = open(path, "rb").read()
    # locate the image descriptor flags byte (after header/table/GCE)
    flags = None
    idx = data.find(b"\x2c")
    if idx >= 0:
        flags = data[idx + 9]
    return [st, flags, len(data)]


def pill_flags(**kw):
    path = os.path.join(tmp, "r.gif")
    im.save(path, "GIF", **kw)
    data = open(path, "rb").read()
    idx = data.find(b"\x2c")
    return [data[idx + 9] if idx >= 0 else None, len(data)]


out["dll_default"] = dll_save("d_default.gif", 1)
out["ref_default"] = pill_flags()
out["dll_nointerlace"] = dll_save("d_noint.gif", 0)
out["ref_nointerlace"] = pill_flags(interlace=False)
out["dll_interlace"] = dll_save("d_i.gif", 1)
out["ref_interlace"] = pill_flags(interlace=True)
out["dll_palette"] = dll_save("d_p.gif", 1, [0, 0, 0, 255, 255, 255])
out["ref_palette"] = pill_flags(palette=[0, 0, 0, 255, 255, 255])
for name in ("d_default.gif", "d_noint.gif", "d_i.gif", "d_p.gif", "r.gif"):
    reopened = Image.open(os.path.join(tmp, name))
    out[f"reopen_{name}"] = [reopened.mode, reopened.size, list(reopened.getdata())[:4]]

dll.pillow_c_image_free(handle.value)
print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_gifopts2_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
