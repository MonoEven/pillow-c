"""BEHAV-SAVEOPTS-001 cross-check #2: PNG P-mode bit depths (auto + bits
override) via the DLL against Pillow 11.3.0."""
import ctypes
import json
import os
import tempfile

from PIL import Image

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_save_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_png_bits.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]

out = {}
tmp = tempfile.mkdtemp(prefix="pngbits_dll_")


def fixture(name, n_colors, n_indices):
    im = Image.new("P", (8, 8))
    im.putpalette([(i * 40) % 256 for i in range(n_colors * 3)])
    for x in range(8):
        for y in range(8):
            im.putpixel((x, y), (x + y) % n_indices)
    path = os.path.join(tmp, name)
    im.save(path)
    return path


for n_colors in (2, 3, 4, 5, 16, 17):
    fx = fixture(f"p{n_colors}.png", n_colors, n_colors)
    handle = ctypes.c_void_p()
    assert dll.pillow_c_image_open_png(fx.encode(), ctypes.byref(handle)) == 0
    out_path = os.path.join(tmp, f"d{n_colors}.png")
    st = dll.pillow_c_image_save_png(handle.value, out_path.encode())
    data = open(out_path, "rb").read()
    dll.pillow_c_image_free(handle.value)
    reopen = Image.open(out_path)
    out[f"auto_{n_colors}"] = [st, data[24], reopen.mode, len(data)]

fx = fixture("p16.png", 16, 16)
handle = ctypes.c_void_p()
dll.pillow_c_image_open_png(fx.encode(), ctypes.byref(handle))
for bits in (1, 2, 4, 3, 9, 0, 16):
    out_path = os.path.join(tmp, f"b{bits}.png")
    st = dll.pillow_c_image_save_png_bits(handle.value, out_path.encode(), bits)
    data = open(out_path, "rb").read()
    reopen = Image.open(out_path)
    ok = reopen.mode == "P"
    out[f"bits_{bits}"] = [st, data[24] if ok else -1, ok, len(data)]
dll.pillow_c_image_free(handle.value)

# Pillow reference depths
out["ref"] = {}
for n_colors in (2, 3, 4, 5, 16, 17):
    im = Image.new("P", (8, 8))
    im.putpalette([(i * 40) % 256 for i in range(n_colors * 3)])
    for x in range(8):
        for y in range(8):
            im.putpixel((x, y), (x + y) % n_colors)
    p = os.path.join(tmp, f"r{n_colors}.png")
    im.save(p)
    out["ref"][f"auto_{n_colors}"] = open(p, "rb").read()[24]

# pixel round-trip: DLL auto-save of the 16-color fixture reopened in Pillow
d_data = open(os.path.join(tmp, "d16.png"), "rb").read()
r_data = open(os.path.join(tmp, "r16.png"), "rb").read()
out["depth16_reopen_pixels_match"] = list(Image.open(os.path.join(tmp, "d16.png")).getdata())[:4]
out["ref16_pixels"] = list(Image.open(os.path.join(tmp, "r16.png")).getdata())[:4]

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pngbits_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
