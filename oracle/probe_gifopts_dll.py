"""BEHAV-SAVEOPTS-001 cross-check #3: GIF interlace/palette via the DLL
against Pillow 11.3.0 (structure + reopen pixels)."""
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
tmp = tempfile.mkdtemp(prefix="gifopts_dll_")

im = Image.new("P", (8, 8))
im.putpalette([(i * 32) % 256 for i in range(8)])
for x in range(8):
    for y in range(8):
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
    return [st, data.hex(), len(data)]


def pill_save(name, **kw):
    path = os.path.join(tmp, name)
    im.save(path, "GIF", **kw)
    data = open(path, "rb").read()
    return [data.hex(), len(data)]


out["dll_interlace"] = dll_save("d_i.gif", 1)
out["ref_interlace"] = pill_save("r_i.gif", interlace=True)
out["dll_palette"] = dll_save("d_p.gif", 0, [0, 0, 0, 255, 255, 255])
out["ref_palette"] = pill_save("r_p.gif", palette=[0, 0, 0, 255, 255, 255])
out["dll_plain"] = dll_save("d_plain.gif", 0)
out["ref_plain"] = pill_save("r_plain.gif")
# reopen pixels: interlaced files must decode identically
for name in ("d_i.gif", "r_i.gif", "d_p.gif", "r_p.gif", "d_plain.gif"):
    reopened = Image.open(os.path.join(tmp, name))
    out[f"reopen_{name}"] = [reopened.mode, reopened.size, list(reopened.getdata())[:4]]

dll.pillow_c_image_free(handle.value)
print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_gifopts_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
