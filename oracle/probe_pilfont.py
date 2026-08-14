"""BEHAV-FONTFILE-002 oracle: pin Pillow 11.3.0's PILfont (bitmap font)
semantics with crafted fixtures: the 256x20-byte metrics layout, the
getsize/getmask semantics and modes, and the error shapes."""
import base64
import json
import os
import struct
import tempfile

from PIL import Image, ImageFont

out = {}
tmp = tempfile.mkdtemp(prefix="pilfont_probe_")


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


# --- crafted glyph image: 16x16 mode-1 PBM (P4), left half a 4x4 block
# pattern per char slot, right half a different pattern ---
w = 16
h = 16
img = Image.new("1", (w, h), 0)
for y in range(h):
    for x in range(w):
        if x < 8:
            # left region: diagonal
            if (x + y) % 2 == 0:
                img.putpixel((x, y), 1)
        else:
            # right region: vertical stripe every 3
            if x % 3 == 0:
                img.putpixel((x, y), 1)

root = os.path.join(tmp, "crafted")
img.save(root + ".pbm")
img.save(root + ".png")

# --- metrics: 256 entries x 10 shorts (LE).
# char 'A' (65): dx=4, dy=1, dst=(0,0,4,4), src=(0,0,4,4)
# char 'B' (66): dx=3, dy=0, dst=(0,1,3,4), src=(8,0,11,3)
# every other char: dx=1, dy=0, dst=(0,0,1,1), src=(8,8,9,9)
metrics = []
for i in range(256):
    if i == 65:
        entry = [4, 1, 0, 0, 4, 4, 0, 0, 4, 4]
    elif i == 66:
        entry = [3, 0, 0, 1, 3, 4, 8, 0, 11, 3]
    else:
        entry = [1, 0, 0, 0, 1, 1, 8, 8, 9, 9]
    metrics.extend(entry)
raw = struct.pack("<2560h", *metrics)

pil_path = root + ".pil"
with open(pil_path, "wb") as f:
    f.write(b"PILfont\n")
    f.write(b"16x16;crafted;\n")
    f.write(b"name=crafted\n")
    f.write(b"DATA\n")
    f.write(raw)

f = ImageFont.load(pil_path)
out["props"] = {
    "file": f.file,
    "info": f.info,
    "repr": repr(f)[:100],
}
for text in ("A", "AB", "B", " ", ""):
    out[f"getsize_{text!r}"] = capture(lambda t=text: f.font.getsize(t))
    out[f"getbbox_{text!r}"] = capture(lambda t=text: f.getbbox(t))
    out[f"getlength_{text!r}"] = capture(lambda t=text: f.getlength(t))
    out[f"getlength_bytes_{text!r}"] = capture(lambda t=text: f.getlength(t.encode()))

for mode in ("", "1", "L", "XX"):
    m = capture(lambda md=mode: f.getmask("A", md))
    if "ok" in m:
        im = m["ok"]
        wrapped = Image.Image()._new(im)
        out[f"mask_A_mode_{mode!r}"] = [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:64]]
    else:
        out[f"mask_A_mode_{mode!r}"] = m
m = capture(lambda: f.getmask("AB"))
if "ok" in m:
    im = m["ok"]
    wrapped = Image.Image()._new(im)
    out["mask_AB"] = [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:96]]
else:
    out["mask_AB"] = m

# --- error shapes ---
out["err_bad_header"] = capture(lambda: ImageFont.load(_make_bad_header()))
out["err_bad_mode"] = capture(lambda: ImageFont.load(_make_rgb_image()))
out["err_missing_glyph"] = capture(lambda: ImageFont.load(os.path.join(tmp, "no-glyph-xyz.pil")))

# --- the bundled default bitmap font (courB08) ---
d = ImageFont.load_default_imagefont()
for text in ("A", "AB", "i", ""):
    out[f"default_getsize_{text!r}"] = capture(lambda t=text: d.font.getsize(t))
    out[f"default_getlength_{text!r}"] = capture(lambda t=text: d.getlength(t))
m = capture(lambda: d.getmask("AB"))
if "ok" in m:
    im = m["ok"]
    wrapped = Image.Image()._new(im)
    out["default_mask_AB"] = [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:96]]
else:
    out["default_mask_AB"] = m
m = capture(lambda: d.getmask("A", "1"))
if "ok" in m:
    im = m["ok"]
    wrapped = Image.Image()._new(im)
    out["default_mask_A_1"] = [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:64]]
else:
    out["default_mask_A_1"] = m


def _make_bad_header():
    p = os.path.join(tmp, "badhdr.pil")
    with open(p, "wb") as fh:
        fh.write(b"NOTPIL\nDATA\n" + b"\x00" * 5120)
    return p


def _make_rgb_image():
    p = os.path.join(tmp, "rgbimg.pil")
    with open(p, "wb") as fh:
        fh.write(b"PILfont\n16x16;x;\nDATA\n" + b"\x00" * 5120)
    Image.new("RGB", (16, 16), (10, 20, 30)).save(os.path.join(tmp, "rgbimg.png"))
    return p


# --- extract the bundled courB08 .pil metrics + glyph PNG from the local
# Pillow source for fixture embedding ---
import re

src = open(ImageFont.__file__, "rb").read()
blobs = re.findall(rb'base64\.b64decode\(\s*b"""\r?\n(.*?)"""\s*\)', src, re.S)
out["courB08_blobs"] = [len(b) for b in blobs]
cour_pil = base64.b64decode(blobs[-2]) if len(blobs) >= 2 else None
cour_png = base64.b64decode(blobs[-1]) if len(blobs) >= 2 else None
out["courB08_sizes"] = [len(cour_pil), len(cour_png)]
if cour_pil:
    with open(os.path.join(tmp, "courB08.pil"), "wb") as fh:
        fh.write(cour_pil)
    with open(os.path.join(tmp, "courB08.png"), "wb") as fh:
        fh.write(cour_png)
    # verify the extracted data loads
    out["courB08_reload"] = capture(
        lambda: (ImageFont.load(os.path.join(tmp, "courB08.pil")).getlength("AB"),
                 ImageFont.load(os.path.join(tmp, "courB08.pil")).getmask("AB").size)
    )

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pilfont.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
