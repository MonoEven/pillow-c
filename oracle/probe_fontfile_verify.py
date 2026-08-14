"""BEHAV-FONTFILE-001 rule verification: confirm the extents assembly rule
predictions for texts with leading/trailing spaces and non-exact kern pairs,
and pin the RGBA mask bytes / mode-1 sizes / notdef metrics."""
import json

from PIL import Image, ImageFont

out = {}
ARIAL = r"C:\Windows\Fonts\arial.ttf"
f = ImageFont.truetype(ARIAL, 24)


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


for text in (" A", "A ", "To", "Ta", "VV", "VA", "AV", "\n", "A\n", "\nA", "A\nB", "AB"):
    out[repr(text)] = {
        "bbox": capture(lambda t=text: f.getbbox(t)),
        "getsize": capture(lambda t=text: f.font.getsize(t)),
        "mask_size": capture(lambda t=text: f.getmask(t).size),
        "len": capture(lambda t=text: f.getlength(t)),
    }

im = f.getmask("A", "RGBA")
wrapped = Image.Image()._new(im)
wrapped.load()
out["rgba_bytes"] = wrapped.convert("RGB").tobytes().hex()[:80]
out["rgba_alpha"] = wrapped.convert("L").tobytes().hex()[:80]
im2 = f.getmask("A", "RGBA", ink=0)
wrapped2 = Image.Image()._new(im2)
wrapped2.load()
out["rgba_ink0"] = wrapped2.convert("RGB").tobytes().hex()[:40]
out["mask_mode_1_size"] = capture(lambda: f.getmask("A", "1").size)
out["mask_mode_1_bytes"] = capture(
    lambda: f.getmask("A", "1").convert("L").tobytes().hex()[:40]
)
out["mask_mode_L_bytes"] = capture(
    lambda: f.getmask("A", "L").convert("L").tobytes().hex()[:40]
)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_fontfile_verify.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
