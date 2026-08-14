"""BEHAV-FONTFILE-002 ctypes cross-check: the native PILfont surface against
the Pillow 11.3.0 oracle values (crafted fixtures + courB08)."""
import ctypes
import json
import os
import struct
import tempfile

from PIL import Image, ImageFont

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_png.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_png.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_font_load_pil.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_font_load_pil.restype = ctypes.c_int
dll.pillow_c_font_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_font_getlength.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)]
dll.pillow_c_font_getbbox.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_font_getmask.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]


class PillowCImage(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_int),
        ("height", ctypes.c_int),
        ("mode", ctypes.c_int),
        ("channels", ctypes.c_int),
        ("stride", ctypes.c_size_t),
        ("_v_start", ctypes.c_size_t),
        ("_v_finish", ctypes.c_size_t),
        ("_v_end", ctypes.c_size_t),
    ]


def be(v):
    return struct.unpack("<h", struct.pack(">h", v))[0]


def metrics_bytes(entries):
    data = []
    for i in range(256):
        entry = entries.get(i, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        data.extend(be(v) for v in entry)
    return struct.pack("<2560h", *data)


tmp = tempfile.mkdtemp(prefix="pilfont_dll_")
out = {}


def native_font(png_path, metrics):
    img = ctypes.c_void_p()
    st = dll.pillow_c_image_open_png(png_path.encode(), ctypes.byref(img))
    assert st == 0, f"png open {st}"
    font = ctypes.c_void_p()
    st = dll.pillow_c_font_load_pil(metrics, len(metrics), img.value, ctypes.byref(font))
    dll.pillow_c_image_free(img.value)
    return st, font.value


def native_length(font, text):
    v = ctypes.c_double()
    st = dll.pillow_c_font_getlength(font, text.encode(), ctypes.byref(v))
    return st, v.value


def native_bbox(font, text):
    l = ctypes.c_int(); t = ctypes.c_int(); r = ctypes.c_int(); b = ctypes.c_int()
    st = dll.pillow_c_font_getbbox(font, text.encode(), ctypes.byref(l), ctypes.byref(t), ctypes.byref(r), ctypes.byref(b))
    return st, (l.value, t.value, r.value, b.value)


def native_mask(font, text, mode=b""):
    img = ctypes.c_void_p()
    st = dll.pillow_c_font_getmask(font, text.encode(), mode, 0, ctypes.byref(img))
    if st != 0:
        return [st, None]
    raw = ctypes.string_at(img.value, 48)
    width, height, image_mode, channels = struct.unpack("<iiii", raw[:16])
    stride, v_begin = struct.unpack("<QQ", raw[16:32])
    n = stride * max(height, 0)
    data = ctypes.string_at(v_begin, n) if n else b""
    dll.pillow_c_image_free(img.value)
    if image_mode == 5:
        # DLL stores mode 1 byte-per-pixel; pack MSB-first for comparison
        packed = bytearray((width + 7) // 8 * height)
        for y in range(height):
            for x in range(width):
                if data[y * width + x]:
                    packed[y * ((width + 7) // 8) + (x >> 3)] |= 1 << (7 - (x & 7))
        data = bytes(packed)
    return [st, [width, height, image_mode, data.hex()[:64]]]


def pack_l_bytes(width, height, ldata):
    packed = bytearray((width + 7) // 8 * height)
    for y in range(height):
        for x in range(width):
            if ldata[y * width + x]:
                packed[y * ((width + 7) // 8) + (x >> 3)] |= 1 << (7 - (x & 7))
    return bytes(packed)


def diagonal(img):
    for y in range(img.height):
        for x in range(img.width):
            if (x + y) % 2 == 0:
                img.putpixel((x, y), 1)


# fixture f5-like: A 6x4 at origin, B 4x4 shifted, diagonal image
img = Image.new("1", (20, 12), 0)
diagonal(img)
png = os.path.join(tmp, "fx.png")
img.save(png)
entries = {65: [6, 0, 0, 0, 6, 4, 0, 0, 6, 4], 66: [4, 0, 0, 0, 4, 4, 6, 0, 10, 4]}
m = metrics_bytes(entries)
st, font = native_font(png, m)
out["load"] = st
out["len_A"] = native_length(font, "A")
out["len_AB"] = native_length(font, "AB")
out["bbox_A"] = native_bbox(font, "A")
out["bbox_AB"] = native_bbox(font, "AB")
out["mask_A"] = native_mask(font, "A")
out["mask_AB"] = native_mask(font, "AB")
out["mask_A_mode1"] = native_mask(font, "A", b"1")
out["mask_empty"] = native_mask(font, "")
dll.pillow_c_font_free(font)

# mismatch -> SystemError status
entries2 = {65: [4, 0, 0, 0, 2, 2, 0, 0, 6, 6]}
m2 = metrics_bytes(entries2)
st, font2 = native_font(png, m2)
out["mismatch_mask"] = native_mask(font2, "A")
dll.pillow_c_font_free(font2)

# L glyph image
imgL = Image.new("L", (20, 12), 0)
for y in range(12):
    for x in range(20):
        imgL.putpixel((x, y), [0, 64, 128, 200, 255][(x + y) % 5])
pngL = os.path.join(tmp, "fl.png")
imgL.save(pngL)
entriesL = {65: [6, 0, 0, 0, 6, 6, 0, 0, 6, 6]}
mL = metrics_bytes(entriesL)
st, fontL = native_font(pngL, mL)
out["L_mask"] = native_mask(fontL, "A")
dll.pillow_c_font_free(fontL)

# courB08 via the shipped fixtures
cour_pil = open("ahk/fonts/courB08.pil", "rb").read()
cour_png = "ahk/fonts/courB08.png"
data = cour_pil.split(b"DATA\n", 1)[1][:5120]
st, cour = native_font(cour_png, data)
out["cour_load"] = st
out["cour_len_AB"] = native_length(cour, "AB")
out["cour_bbox_A"] = native_bbox(cour, "A")
out["cour_mask_A"] = native_mask(cour, "A")
out["cour_mask_AB"] = native_mask(cour, "AB")
dll.pillow_c_font_free(cour)

# Pillow reference values for the same texts
with open(os.path.join(tmp, "fx.pil"), "wb") as f:
    f.write(b"PILfont\n;;;;;;10;\nDATA\n" + m)
ref = ImageFont.load(os.path.join(tmp, "fx.pil"))
out["ref_len_AB"] = ref.getlength("AB")
out["ref_bbox_AB"] = list(ref.getbbox("AB"))
w = Image.Image()._new(ref.getmask("AB"))
out["ref_mask_AB"] = [w.mode, w.size, pack_l_bytes(w.size[0], w.size[1], w.convert("L").tobytes()).hex()[:64]]
cour_ref = ImageFont.load_default_imagefont()
wc = Image.Image()._new(cour_ref.getmask("AB"))
out["cour_ref_mask_AB"] = [wc.mode, wc.size, pack_l_bytes(wc.size[0], wc.size[1], wc.convert("L").tobytes()).hex()[:64]]
wc2 = Image.Image()._new(cour_ref.getmask("A"))
out["cour_ref_mask_A"] = [wc2.mode, wc2.size, pack_l_bytes(wc2.size[0], wc2.size[1], wc2.convert("L").tobytes()).hex()[:64]]
with open(os.path.join(tmp, "fl.pil"), "wb") as f:
    f.write(b"PILfont\n;;;;;;10;\nDATA\n" + mL)
refL = ImageFont.load(os.path.join(tmp, "fl.pil"))
wL = Image.Image()._new(refL.getmask("A"))
out["ref_L_mask"] = [wL.mode, wL.size, wL.convert("L").tobytes().hex()[:64]]

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pilfont_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
