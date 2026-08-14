"""BEHAV-FONTFILE-001 ctypes smoke test #2: mask surface via pillow_c_font_getmask."""
import ctypes
import json

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")

dll.pillow_c_font_load_file.argtypes = [ctypes.c_char_p, ctypes.c_double, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_font_load_file.restype = ctypes.c_int
dll.pillow_c_font_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_font_getmask.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_font_getmask.restype = ctypes.c_int
dll.pillow_c_font_getbbox_anchor.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_font_getbbox_anchor.restype = ctypes.c_int

# PillowCImage layout: width, height, mode, channels, stride, pixels(vector)
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
        ("pad", ctypes.c_uint8 * 8),
    ]


out = {}
h = ctypes.c_void_p()
dll.pillow_c_font_load_file(r"C:\Windows\Fonts\arial.ttf".encode(), 24.0, 0, b"", 1, ctypes.byref(h))


def mask(text, mode=b"", ink=0):
    img = ctypes.c_void_p()
    st = dll.pillow_c_font_getmask(h, text.encode(), mode, ink, ctypes.byref(img))
    if st != 0:
        return [st, None]
    core = ctypes.cast(img.value, ctypes.POINTER(PillowCImage)).contents
    # pixel data follows the vector bookkeeping: find nonzero count heuristically
    return [st, [core.width, core.height, core.mode, core.channels]]


def anchor(text, a):
    l = ctypes.c_int(); t = ctypes.c_int(); r = ctypes.c_int(); b = ctypes.c_int()
    st = dll.pillow_c_font_getbbox_anchor(h, text.encode(), a.encode(), ctypes.byref(l), ctypes.byref(t), ctypes.byref(r), ctypes.byref(b))
    return [st, (l.value, t.value, r.value, b.value)]


out["mask_A"] = mask("A")
out["mask_AB"] = mask("AB")
out["mask_spaces"] = mask("  ")
out["mask_empty"] = mask("")
out["mask_A_RGBA"] = mask("A", b"RGBA", 255)
out["mask_A_RGBA_ink0"] = mask("A", b"RGBA", 0)
out["mask_A_mode1"] = mask("A", b"1")
out["anchor_mm_AB"] = anchor("AB", "mm")
out["anchor_rs_AB"] = anchor("AB", "rs")
out["anchor_la_AB"] = anchor("AB", "la")
out["anchor_bad"] = anchor("AB", "ts")
dll.pillow_c_font_free(h)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_mask.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
