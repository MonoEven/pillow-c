"""BEHAV-FONTFILE-001 ctypes smoke test: validate the native exports against
the pinned Pillow 11.3.0 values."""
import ctypes
import json

dll = ctypes.CDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")

dll.pillow_c_font_load_file.argtypes = [ctypes.c_char_p, ctypes.c_double, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_font_load_file.restype = ctypes.c_int
dll.pillow_c_font_load_bytes.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_double, ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_font_load_bytes.restype = ctypes.c_int
dll.pillow_c_font_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_font_getlength.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_double)]
dll.pillow_c_font_getlength.restype = ctypes.c_int
dll.pillow_c_font_getmetrics.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_font_getname.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
dll.pillow_c_font_getbbox.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_font_is_variable.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]


def load(path, size=24.0, index=0, encoding=b"", engine=1):
    h = ctypes.c_void_p()
    st = dll.pillow_c_font_load_file(path.encode(), size, index, encoding, engine, ctypes.byref(h))
    return st, h.value


def length(h, text):
    out = ctypes.c_double()
    st = dll.pillow_c_font_getlength(h, text.encode(), ctypes.byref(out))
    return st, out.value


def metrics(h):
    a = ctypes.c_int()
    d = ctypes.c_int()
    st = dll.pillow_c_font_getmetrics(h, ctypes.byref(a), ctypes.byref(d))
    return st, (a.value, d.value)


def name(h):
    fr = ctypes.c_size_t()
    sr = ctypes.c_size_t()
    st = dll.pillow_c_font_getname(h, None, 0, ctypes.byref(fr), None, 0, ctypes.byref(sr))
    fam = ctypes.create_string_buffer(fr.value)
    sty = ctypes.create_string_buffer(sr.value)
    st = dll.pillow_c_font_getname(h, fam, fr.value, ctypes.byref(fr), sty, sr.value, ctypes.byref(sr))
    return st, (fam.value.decode(), sty.value.decode())


def bbox(h, text):
    l = ctypes.c_int(); t = ctypes.c_int(); r = ctypes.c_int(); b = ctypes.c_int()
    st = dll.pillow_c_font_getbbox(h, text.encode(), ctypes.byref(l), ctypes.byref(t), ctypes.byref(r), ctypes.byref(b))
    return st, (l.value, t.value, r.value, b.value)


ARIAL = r"C:\Windows\Fonts\arial.ttf"
CAMB = r"C:\Windows\Fonts\cambria.ttc"

out = {}
st, h = load(ARIAL, 24.0)
out["load_arial_24"] = st
out["name"] = name(h)
out["metrics"] = metrics(h)
for t in ("A", "AB", "ABC", "AV", "VA", " A", "A ", "To", "A\nB", "\n", "  ", "gy", "g", "Ag"):
    out[f"len_{t!r}"] = length(h, t)
for t in ("A", "ABC", " A", "A ", "To", "A\nB", "\n", "  ", "gy", "g", "Ag", "AB", "AV"):
    out[f"bbox_{t!r}"] = bbox(h, t)
var = ctypes.c_int()
out["is_variable"] = (dll.pillow_c_font_is_variable(h, ctypes.byref(var)), var.value)
dll.pillow_c_font_free(h)

st, h = load(ARIAL, 10.0)
out["metrics_10"] = metrics(h)
out["len_A_10"] = length(h, "A")
dll.pillow_c_font_free(h)

st, h = load(ARIAL, 24.5)
out["metrics_24_5"] = metrics(h)
out["len_A_24_5"] = length(h, "A")
dll.pillow_c_font_free(h)

st, h = load(CAMB, 24.0, index=0)
out["cambria0"] = (st, name(h))
dll.pillow_c_font_free(h)
st, h = load(CAMB, 24.0, index=1)
out["cambria1"] = (st, name(h))
dll.pillow_c_font_free(h)
st, h = load(CAMB, 24.0, index=99)
out["cambria99"] = st
st, h = load(ARIAL, 24.0, index=1)
out["arial_index1"] = st
st, h = load(r"C:\Windows\Fonts\no-such-font-xyz.ttf")
out["missing"] = st
st, h = load(ARIAL, 24.0, encoding=b"junk")
out["encoding_junk"] = st
with open(ARIAL, "rb") as f:
    data = f.read()
buf = ctypes.create_string_buffer(data)
st, h = None, ctypes.c_void_p()
st = dll.pillow_c_font_load_bytes(buf, len(data), 24.0, 0, b"", 1, ctypes.byref(h))
out["bytes_load"] = st
out["bytes_name"] = name(h.value)
out["bytes_len_A"] = length(h.value, "A")
dll.pillow_c_font_free(h.value)
garbage = ctypes.create_string_buffer(b"\x00\x01\x02 not a font")
st = dll.pillow_c_font_load_bytes(garbage, len(garbage.raw) - 1, 24.0, 0, b"", 1, ctypes.byref(h))
out["garbage"] = st

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_dll.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
