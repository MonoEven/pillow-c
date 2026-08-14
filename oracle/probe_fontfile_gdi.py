"""BEHAV-FONTFILE-001 GDI probe: compare GDI hinted glyph metrics with the
FreeType-hinted values derived from Pillow's getsize, and dump GDI advances."""
import ctypes
import json
from ctypes import wintypes

gdi32 = ctypes.WinDLL("gdi32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

GGO_METRICS = 0
GGO_GRAY8_BITMAP = 6
GGO_GLYPH_INDEX = 0x0080
GGO_BITMAP = 1
ANSI_CHARSET = 0
DEFAULT_CHARSET = 1
OUT_TT_PRECIS = 4
CLIP_DEFAULT_PRECIS = 0
ANTIALIASED_QUALITY = 4
DEFAULT_QUALITY = 0


class GLYPHMETRICS(ctypes.Structure):
    _fields_ = [
        ("gmBlackBoxX", wintypes.UINT),
        ("gmBlackBoxY", wintypes.UINT),
        ("gmptGlyphOriginX", wintypes.LONG),  # POINT as two LONGs (16.16)
        ("gmptGlyphOriginY", wintypes.LONG),
        ("gmCellIncX", wintypes.SHORT),
        ("gmCellIncY", wintypes.SHORT),
    ]


class TEXTMETRICW(ctypes.Structure):
    _fields_ = [
        ("tmHeight", wintypes.LONG),
        ("tmAscent", wintypes.LONG),
        ("tmDescent", wintypes.LONG),
        ("tmInternalLeading", wintypes.LONG),
        ("tmExternalLeading", wintypes.LONG),
        ("tmAveCharWidth", wintypes.LONG),
        ("tmMaxCharWidth", wintypes.LONG),
        ("tmWeight", wintypes.LONG),
        ("tmOverhang", wintypes.LONG),
        ("tmDigitizedAspectX", wintypes.LONG),
        ("tmDigitizedAspectY", wintypes.LONG),
        ("tmFirstChar", wintypes.WCHAR),
        ("tmLastChar", wintypes.WCHAR),
        ("tmDefaultChar", wintypes.WCHAR),
        ("tmBreakChar", wintypes.WCHAR),
        ("tmItalic", wintypes.BYTE),
        ("tmUnderlined", wintypes.BYTE),
        ("tmStruckOut", wintypes.BYTE),
        ("tmPitchAndFamily", wintypes.BYTE),
        ("tmCharSet", wintypes.BYTE),
    ]


out = {}

with open(r"C:\Windows\Fonts\arial.ttf", "rb") as f:
    font_data = f.read()

buf = ctypes.create_string_buffer(font_data)
num_fonts = wintypes.DWORD()
hres = gdi32.AddFontMemResourceEx(buf, len(font_data), None, ctypes.byref(num_fonts))

class MAT2(ctypes.Structure):
    _fields_ = [
        ("eM11", wintypes.LONG),
        ("eM12", wintypes.LONG),
        ("eM21", wintypes.LONG),
        ("eM22", wintypes.LONG),
    ]


identity_mat = MAT2(65536, 0, 0, 65536)
glyph_buf = ctypes.create_string_buffer(4096)

for size in (10, 24, 24.5):
    hdc = gdi32.CreateCompatibleDC(None)
    bmp = gdi32.CreateCompatibleBitmap(hdc, 64, 64)
    gdi32.SelectObject(hdc, bmp)
    hfont = gdi32.CreateFontW(
        -int(size), 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 0, "Arial",
    )
    old = gdi32.SelectObject(hdc, hfont)
    tm = TEXTMETRICW()
    gdi32.GetTextMetricsW(hdc, ctypes.byref(tm))
    d = {
        "size": size,
        "tm_ascent": tm.tmAscent,
        "tm_descent": tm.tmDescent,
        "tm_height": tm.tmHeight,
    }
    for ch in "ABCVgy ":
        gm = GLYPHMETRICS()
        widx = wintypes.WORD()
        gdi32.GetGlyphIndicesW(hdc, ch, 1, ctypes.byref(widx), 1)
        ok = gdi32.GetGlyphOutlineW(hdc, widx.value, GGO_METRICS | GGO_GLYPH_INDEX, ctypes.byref(gm), 4096, glyph_buf, ctypes.byref(identity_mat))
        err = ctypes.get_last_error()
        d[ch] = {
            "gid": widx.value,
            "ok": ok,
            "glerr": err,
            "w": gm.gmBlackBoxX,
            "h": gm.gmBlackBoxY,
            "ox": gm.gmptGlyphOriginX / 65536.0,
            "oy": gm.gmptGlyphOriginY / 65536.0,
            "cell_inc_x": gm.gmCellIncX,
        }
    gdi32.SelectObject(hdc, old)
    gdi32.DeleteObject(hfont)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(hdc)
    out[f"size_{size}"] = d

# GDI text extents (kerning-aware) for comparison
hdc = gdi32.CreateCompatibleDC(None)
hfont = gdi32.CreateFontW(-24, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 0, "Arial")
old = gdi32.SelectObject(hdc, hfont)
ext = {}
for text in ("A", "ABC", "Ag", "AV", "g", "gy"):
    sz = wintypes.SIZE()
    gdi32.GetTextExtentPoint32W(hdc, text, len(text), ctypes.byref(sz))
    ext[text] = [sz.cx, sz.cy]
out["gdi_extent24"] = ext
gdi32.SelectObject(hdc, old)
gdi32.DeleteObject(hfont)
gdi32.DeleteDC(hdc)

# family name via GetTextFace
hdc = gdi32.CreateCompatibleDC(None)
hfont = gdi32.CreateFontW(-24, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET,
                          OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, 0, "Arial")
old = gdi32.SelectObject(hdc, hfont)
name = ctypes.create_unicode_buffer(64)
gdi32.GetTextFaceW(hdc, 64, name)
out["text_face"] = name.value
gdi32.SelectObject(hdc, old)
gdi32.DeleteObject(hfont)
gdi32.DeleteDC(hdc)

gdi32.RemoveFontMemResourceEx(hres)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_gdi.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
