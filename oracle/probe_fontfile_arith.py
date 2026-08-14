"""BEHAV-FONTFILE-001 arithmetic oracle: correlate Pillow's FreeType+RAQM values
with the raw Arial table data (fontTools) to derive the exact 26.6 arithmetic."""
import json

from fontTools.ttLib import TTFont
from PIL import ImageFont

out = {}

ARIAL = r"C:\Windows\Fonts\arial.ttf"

# --- raw table data ---
tt = TTFont(ARIAL, fontNumber=0)
upem = tt["head"].unitsPerEm
hhea = tt["hhea"]
os2 = tt["OS/2"]
hmtx = tt["hmtx"]
kern = tt["kern"] if "kern" in tt else None
cmap = tt.getBestCmap()
out["tables"] = {
    "upem": upem,
    "hhea_ascender": hhea.ascent,
    "hhea_descender": hhea.descent,
    "hhea_linegap": hhea.lineGap,
    "os2_typo_asc": getattr(os2, "sTypoAscender", None),
    "os2_typo_desc": getattr(os2, "sTypoDescender", None),
    "os2_win_asc": getattr(os2, "usWinAscent", None),
    "os2_win_desc": getattr(os2, "usWinDescent", None),
    "os2_cap_height": getattr(os2, "sCapHeight", None),
    "os2_x_height": getattr(os2, "sxHeight", None),
    "glyph_names": {ch: cmap[ord(ch)] for ch in "ABC"},
    "advances": {ch: hmtx[cmap[ord(ch)]][0] for ch in "ABC"},
    "kern_pairs": None,
}
if kern:
    pairs = {}
    for table in kern.kernTables:
        for (l, r), v in table.kernTable.items():
            if l in "ABC" and r in "ABC":
                pairs[f"{chr(l) if l >= 32 else l}{chr(r) if r >= 32 else r}"] = v
    out["tables"]["kern_pairs"] = pairs

# --- Pillow values ---
for size in (10, 24):
    f = ImageFont.truetype(ARIAL, size)
    corefont = f.font
    d = {
        "size": size,
        "layout_engine": f.layout_engine,
        "family": corefont.family,
        "style": corefont.style,
        "ascent": corefont.ascent,
        "descent": corefont.descent,
        "x_height": getattr(corefont, "x_height", None),
        "cap_height": getattr(corefont, "cap_height", None),
        "getmetrics": f.getmetrics(),
        "len_A": f.getlength("A"),
        "len_B": f.getlength("B"),
        "len_C": f.getlength("C"),
        "len_AB": f.getlength("AB"),
        "len_BC": f.getlength("BC"),
        "len_ABC": f.getlength("ABC"),
        "len_AV": f.getlength("AV"),
        "len_A": f.getlength("A"),
        "bbox_A": f.getbbox("A"),
        "bbox_AB": f.getbbox("AB"),
        "bbox_ABC": f.getbbox("ABC"),
        "bbox_empty": f.getbbox(""),
        "len_empty": f.getlength(""),
    }
    out[f"size_{size}"] = d

# raw 26.6 values (the core getlength without the /64)
f = ImageFont.truetype(ARIAL, 24)
out["core26_6"] = {
    "len_A": f.font.getlength("A"),
    "len_AB": f.font.getlength("AB"),
    "len_ABC": f.font.getlength("ABC"),
    "getsize_A": f.font.getsize("A"),
    "getsize_ABC": f.font.getsize("ABC"),
}

# BASIC layout (no raqm kerning/shaping)
fb = ImageFont.truetype(ARIAL, 24, layout_engine=ImageFont.Layout.BASIC)
out["basic"] = {
    "len_A": fb.getlength("A"),
    "len_AB": fb.getlength("AB"),
    "len_ABC": fb.getlength("ABC"),
    "len_AV": fb.getlength("AV"),
    "bbox_A": fb.getbbox("A"),
    "bbox_AB": fb.getbbox("AB"),
    "bbox_ABC": fb.getbbox("ABC"),
}

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_arith.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
