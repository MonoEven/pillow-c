"""BEHAV-FONTFILE-002 oracle #4: exact SystemError boundaries and the
negative dst_y0 clip behavior."""
import json
import os
import struct
import tempfile

from PIL import Image, ImageFont

tmp = tempfile.mkdtemp(prefix="pilfont4_")
out = {}


def be(v):
    return struct.unpack("<h", struct.pack(">h", v))[0]


def make_font(name, entries):
    root = os.path.join(tmp, name)
    img = Image.new("1", (20, 12), 0)
    for y in range(12):
        for x in range(20):
            if (x + y) % 2 == 0:
                img.putpixel((x, y), 1)
    img.save(root + ".png")
    metrics = []
    for i in range(256):
        entry = entries.get(i, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        metrics.extend(be(v) for v in entry)
    raw = struct.pack("<2560h", *metrics)
    with open(root + ".pil", "wb") as f:
        f.write(b"PILfont\n;;;;;;10;\nDATA\n")
        f.write(raw)
    return ImageFont.load(root + ".pil")


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def mask(font, text="A"):
    m = capture(lambda: font.getmask(text))
    if "ok" in m:
        im = m["ok"]
        wrapped = Image.Image()._new(im)
        return [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:48]]
    return m


# dst_x0 boundaries: -1, -2, -3 with matching src
for dx0 in (-1, -2, -3):
    f = make_font(f"x{dx0}", {65: [6, 0, dx0, 0, dx0 + 4, 6, 0, 0, 4, 6]})
    out[f"dst_x0_{dx0}"] = mask(f)

# src > dst in width only, height only
f = make_font("wbig", {65: [6, 0, 0, 0, 4, 6, 0, 0, 6, 6]})
out["src_w_big"] = mask(f)
f = make_font("hbig", {65: [6, 0, 0, 0, 4, 4, 0, 0, 4, 6]})
out["src_h_big"] = mask(f)
# src < dst
f = make_font("wsmall", {65: [6, 0, 0, 0, 6, 6, 0, 0, 3, 6]})
out["src_w_small"] = mask(f)

# dst_y0 below the font min (row negative)
f = make_font("yneg", {65: [6, 0, 0, -3, 6, 3, 0, 0, 6, 6]})
out["dst_y0_neg"] = mask(f)

# src box outside the image bounds
f = make_font("srcbig", {65: [6, 0, 0, 0, 6, 6, 15, 0, 21, 6]})
out["src_out_of_image"] = mask(f)

# glyph image smaller than referenced src
root = os.path.join(tmp, "tinyimg")
img = Image.new("1", (6, 6), 0)
img.putpixel((1, 1), 1)
img.save(root + ".png")
metrics = [be(v) for i in range(256) for v in (
    [6, 0, 0, 0, 6, 6, 0, 0, 6, 6] if i == 65 else [0, 0, 0, 0, 0, 0, 0, 0, 0, 0])]
raw = struct.pack("<2560h", *metrics)
with open(root + ".pil", "wb") as f:
    f.write(b"PILfont\n;;;;;;10;\nDATA\n")
    f.write(raw)
out["tiny_image"] = mask(ImageFont.load(root + ".pil"))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pilfont4.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
