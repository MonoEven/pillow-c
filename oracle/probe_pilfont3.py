"""BEHAV-FONTFILE-002 oracle #3: empty-text masks, L-mode glyph images,
and the courB08 dst_y1 max (baseline rule verification)."""
import json
import os
import struct
import tempfile

from PIL import Image, ImageFont

tmp = tempfile.mkdtemp(prefix="pilfont3_")
out = {}


def be(v):
    return struct.unpack("<h", struct.pack(">h", v))[0]


def make_font(name, image_mode, image_fill, entries):
    root = os.path.join(tmp, name)
    img = Image.new(image_mode, (20, 12), 0)
    image_fill(img)
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


# L-mode glyph image: gray values 0/64/128/200/255 in a stripe pattern
def l_fill(img):
    for y in range(12):
        for x in range(20):
            img.putpixel((x, y), [0, 64, 128, 200, 255][(x + y) % 5])


fL = make_font("fL", "L", l_fill, {65: [6, 0, 0, 0, 6, 6, 0, 0, 6, 6]})
out["L_glyph"] = {
    "mask": capture(lambda: fL.getmask("A")),
}
m = capture(lambda: fL.getmask("A"))
if "ok" in m:
    im = m["ok"]
    wrapped = Image.Image()._new(im)
    out["L_glyph"]["mask_meta"] = [im.mode, im.size]
    out["L_glyph"]["bytes"] = wrapped.convert("L").tobytes().hex()

# empty text mask
fM = make_font("fM", "1", lambda im: None, {65: [6, 0, 0, 0, 6, 6, 0, 0, 6, 6]})
m = capture(lambda: fM.getmask(""))
if "ok" in m:
    im = m["ok"]
    out["empty_mask"] = [im.mode, im.size]
else:
    out["empty_mask"] = m
out["empty_getsize"] = capture(lambda: fM.font.getsize(""))

# the courB08 max dst_y1 over all glyphs + the descriptor fields
data = open("oracle/_courB08.pil", "rb").read().split(b"DATA\n", 1)[1][:5120]
entries = [struct.unpack("<10h", data[i * 20:(i + 1) * 20]) for i in range(256)]
be_entries = [[struct.unpack(">h", struct.pack("<h", v))[0] for v in e] for e in entries]
out["courB08"] = {
    "max_dst_y1": max(e[5] for e in be_entries),
    "min_dst_y0": min(e[3] for e in be_entries),
    "max_src_x1": max(e[9] for e in be_entries),
    "max_dx": max(e[0] for e in be_entries),
    "A_be": be_entries[65],
    "g_be": be_entries[ord("g")],
}
# verify the height rule: height == max(dst_y1) over the font
out["courB08"]["height_rule"] = capture(
    lambda: ImageFont.load("oracle/_courB08.pil" + "x")
)

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pilfont3.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
