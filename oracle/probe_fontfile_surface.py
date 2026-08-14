"""BEHAV-FONTFILE-001 surface oracle #2: pin every FreeTypeFont behavior needed
for the bounded native slice (metrics arithmetic, kern source, mask modes,
TTC index, error shapes, boundary behaviors)."""
import io
import json
import os
import tempfile

from fontTools.ttLib import TTFont
from PIL import Image, ImageFont

out = {}
ARIAL = r"C:\Windows\Fonts\arial.ttf"


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


# --- table ground truth ---
tt = TTFont(ARIAL, fontNumber=0)
cmap = tt.getBestCmap()
glyf = tt["glyf"]
out["tables"] = {
    "sfnt_version": tt.sfntVersion,
    "has_GPOS": "GPOS" in tt,
    "has_kern": "kern" in tt,
    "kern_formats": [t.format for t in tt["kern"].kernTables] if "kern" in tt else None,
    "has_gvar": "gvar" in tt,
    "advances": {ch: tt["hmtx"][cmap[ord(ch)]][0] for ch in "ABCVgy "},
    "glyph_bbox": {
        ch: [getattr(glyf[cmap[ord(ch)]], a) for a in ("xMin", "yMin", "xMax", "yMax")]
        for ch in "ABCVgy"
    },
    "kern_pairs": {},
}
if "kern" in tt:
    pairs = {}
    for table in tt["kern"].kernTables:
        if not (table.coverage & 0x0001):
            continue  # horizontal only
        for (l, r), v in table.kernTable.items():
            la = chr(l) if isinstance(l, int) and 32 <= l < 0x110000 else str(l)
            lb = chr(r) if isinstance(r, int) and 32 <= r < 0x110000 else str(r)
            pairs[f"{la}{lb}"] = v
    out["tables"]["kern_pairs"] = pairs

# --- per-char lengths, both engines ---
for engine_name, kw in (("raqm", {}), ("basic", {"layout_engine": ImageFont.Layout.BASIC})):
    f = ImageFont.truetype(ARIAL, 24, **kw)
    out[f"len_{engine_name}"] = {ch: f.getlength(ch) for ch in "ABCVgy "}
    out[f"len_{engine_name}"]["pairs"] = {
        "AB": f.getlength("AB"),
        "AV": f.getlength("AV"),
        "VA": f.getlength("VA"),
        "AV_-kern": None,
    }
f = ImageFont.truetype(ARIAL, 24)
out["len_raqm"]["pairs"]["AV_-kern"] = f.getlength("AV", features=["-kern"])
out["len_raqm"]["pairs"]["AV_kern"] = f.getlength("AV", features=["kern"])
out["len_raqm"]["pairs"]["AV_rtl"] = f.getlength("AV", direction="rtl")
out["len_raqm"]["pairs"]["AV_ttb"] = f.getlength("AV", direction="ttb")
out["len_raqm"]["pairs"]["AV_lang"] = f.getlength("AV", language="en")

# --- bboxes / core getsize ---
out["bboxes"] = {}
for text in ("g", "Ag", "gy", "A\nB", "\nA", "  ", ""):
    out["bboxes"][repr(text)] = {
        "bbox": capture(lambda t=text: f.getbbox(t)),
        "getsize": capture(lambda t=text: f.font.getsize(t)),
        "mask_size": capture(lambda t=text: f.getmask(t).size),
        "len": capture(lambda t=text: f.getlength(t)),
    }

# --- mask modes ---
out["mask_modes"] = {}
for mode in ("", "1", "L", "RGBA", "RGB", "XX"):
    m = capture(lambda md=mode: f.getmask("A", md))
    if "ok" in m:
        im = m["ok"]
        wrapped = Image.Image()
        wrapped._new(im)
        out["mask_modes"][mode] = [im.mode, im.size, wrapped.tobytes().hex()[:48]]
    else:
        out["mask_modes"][mode] = m
out["mask2"] = capture(lambda: (f.getmask2("A")[0].size, f.getmask2("A")[1]))
out["mask_empty"] = capture(lambda: (f.getmask("").size, f.getmask2("")[1]))
out["mask_start"] = capture(lambda: (f.getmask2("A", start=(0.5, 0.5))[0].size, f.getmask2("A", start=(0.5, 0.5))[1]))
out["bbox_stroke"] = capture(lambda: f.getbbox("A", stroke_width=2))
out["mask_stroke"] = capture(lambda: (f.getmask("A", stroke_width=2).size,))

# --- float / odd sizes ---
out["size_24_5"] = {
    "len_A": capture(lambda: ImageFont.truetype(ARIAL, 24.5).getlength("A")),
    "metrics": capture(lambda: ImageFont.truetype(ARIAL, 24.5).getmetrics()),
    "bbox_A": capture(lambda: ImageFont.truetype(ARIAL, 24.5).getbbox("A")),
}

# --- TTC faces ---
CAMB = r"C:\Windows\Fonts\cambria.ttc"
out["ttc"] = {
    "face0": capture(lambda: ImageFont.truetype(CAMB, 24, index=0).getname()),
    "face1": capture(lambda: ImageFont.truetype(CAMB, 24, index=1).getname()),
    "face99": capture(lambda: ImageFont.truetype(CAMB, 24, index=99)),
    "ttc_ttf_index1": capture(lambda: ImageFont.truetype(ARIAL, 24, index=1)),
}

# --- file-like source / encoding ---
with open(ARIAL, "rb") as fh:
    arial_bytes = fh.read()
out["bytesio"] = {
    "name": capture(lambda: ImageFont.truetype(io.BytesIO(arial_bytes), 24).getname()),
    "len_A": capture(lambda: ImageFont.truetype(io.BytesIO(arial_bytes), 24).getlength("A")),
}
out["encoding_unic"] = capture(lambda: ImageFont.truetype(ARIAL, 24, encoding="unic").getlength("A"))
out["encoding_junk"] = capture(lambda: ImageFont.truetype(ARIAL, 24, encoding="junk").getlength("A"))

# --- load / load_path / load_default ---
out["load_ttf"] = capture(lambda: ImageFont.load(ARIAL))
garbage = os.path.join(tempfile.gettempdir(), "font_garbage.ttf")
out["load_garbage"] = capture(lambda: ImageFont.load(garbage))
out["load_path_missing"] = capture(lambda: ImageFont.load_path("no-such-font-xyz"))
out["load_path_arial"] = capture(lambda: ImageFont.load_path(os.path.basename(ARIAL)))
out["load_default_size16"] = capture(lambda: (ImageFont.load_default(16).size, ImageFont.load_default(16).getname()))
out["load_default_metrics"] = capture(lambda: ImageFont.load_default().getmetrics())

# --- string length check ---
out["too_long"] = capture(lambda: f.getlength("A" * 1_000_001))
out["too_long_mask"] = capture(lambda: ImageFont.load_default().getmask("A" * 1_000_001))

# --- properties ---
out["props"] = {
    "path": f.path,
    "size": f.size,
    "index": f.index,
    "encoding": f.encoding,
    "layout_engine": f.layout_engine,
    "repr": repr(f)[:120],
}
out["variant"] = {
    "name": capture(lambda: f.font_variant().getname()),
    "len": capture(lambda: f.font_variant(size=12).getlength("A")),
    "variant_size": capture(lambda: f.font_variant(size=12).size),
}
out["multi_line_mask"] = capture(lambda: (f.getmask("A\nB").size, f.getlength("A\nB")))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_surface.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
