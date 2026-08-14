"""BEHAV-FONTFILE-001 oracle: ImageFont.truetype surface and errors in Pillow 11.3.0."""
import json
import os
import tempfile

from PIL import ImageFont

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


ARIAL = r"C:\Windows\Fonts\arial.ttf"


def font_surface(path, size, index=0):
    f = ImageFont.truetype(path, size, index)
    return (
        f.getname(),
        f.getlength("A"),
        f.getlength("ABC"),
        f.getbbox("A"),
        f.getbbox("ABC"),
        f.getmetrics(),
        f.size,
    )


out["truetype"] = capture(lambda: font_surface(ARIAL, 24))
out["truetype_small"] = capture(lambda: font_surface(ARIAL, 10))
out["missing"] = capture(lambda: ImageFont.truetype(os.path.join(tempfile.gettempdir(), "no-such-font-xyz.ttf"), 24))
garbage = os.path.join(tempfile.gettempdir(), "font_garbage.ttf")
with open(garbage, "wb") as f:
    f.write(b"\x00\x01\x02 not a font")
out["garbage"] = capture(lambda: ImageFont.truetype(garbage, 24))
out["zero_size"] = capture(lambda: ImageFont.truetype(ARIAL, 0))
out["neg_size"] = capture(lambda: ImageFont.truetype(ARIAL, -5))
out["index_oob"] = capture(lambda: ImageFont.truetype(ARIAL, 24, index=99))
out["load"] = capture(lambda: font_surface(ARIAL, 24))
out["load_missing"] = capture(lambda: ImageFont.load(os.path.join(tempfile.gettempdir(), "no-such-font-xyz.ttf")))
out["load_default"] = capture(lambda: (ImageFont.load_default().getname(), ImageFont.load_default().size))
# mask modes for a truetype font
f = ImageFont.truetype(ARIAL, 24)
out["mask_modes"] = capture(lambda: (
    bytes(f.getmask("A")).hex()[:32],
    bytes(f.getmask("A", "1")).hex()[:16],
    bytes(f.getmask("A", "L")).hex()[:32],
    bytes(f.getmask("A", "XX")).hex()[:32],
))
out["mask_empty"] = capture(lambda: (f.getmask("").size, bytes(f.getmask("")).hex()))
out["mask_bad_mode"] = capture(lambda: f.getmask("A", "RGBA"))
out["mask_multiline"] = capture(lambda: f.getmask("A\nB").size)
# variation surface (arial has no fvar; the error shapes)
out["variations"] = capture(lambda: f.get_variation_axes())
out["variation_names"] = capture(lambda: f.get_variation_names())
out["set_variation_bad"] = capture(lambda: f.set_variation_by_axes([100]))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
