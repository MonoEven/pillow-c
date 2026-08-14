"""BEHAV-FONTFILE-002 oracle #2: systematic PILfont layout decode.
The 256x20-byte metrics are 10 big-endian int16s per glyph
[dx, dy, dst_x0, dst_y0, dst_x1, dst_y1, src_x0, src_y0, src_x1, src_y1].
Pin the descriptor size field, the getsize height rule, and the mask copy
semantics (1:1 blit vs scaling, multiline, mode packing)."""
import json
import os
import struct
import tempfile

from PIL import Image, ImageFont

tmp = tempfile.mkdtemp(prefix="pilfont2_")
out = {}


def be(v):
    # pack an int16 big-endian into the little-endian short slot
    return struct.unpack("<h", struct.pack(">h", v))[0]


def make_font(name, descriptor, image_w, image_h, entries, image_fill=None):
    root = os.path.join(tmp, name)
    img = Image.new("1", (image_w, image_h), 0)
    if image_fill:
        image_fill(img)
    img.save(root + ".png")
    img.save(root + ".pbm")
    metrics = []
    for i in range(256):
        entry = entries.get(i, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        metrics.extend(be(v) for v in entry)
    raw = struct.pack("<2560h", *metrics)
    with open(root + ".pil", "wb") as f:
        f.write(b"PILfont\n")
        f.write(descriptor.encode())
        f.write(b"\n")
        f.write(b"DATA\n")
        f.write(raw)
    return ImageFont.load(root + ".pil")


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def probe(font, text="A"):
    d = {
        "getsize": capture(lambda: font.font.getsize(text)),
        "getbbox": capture(lambda: font.getbbox(text)),
        "getlength": capture(lambda: font.getlength(text)),
    }
    for mode in ("", "1", "L"):
        m = capture(lambda md=mode: font.getmask(text, md))
        if "ok" in m:
            im = m["ok"]
            wrapped = Image.Image()._new(im)
            d[f"mask_{mode!r}"] = [im.mode, im.size, wrapped.convert("L").tobytes().hex()[:120]]
        else:
            d[f"mask_{mode!r}"] = m
    return d


# diagonal fill helper
def diagonal(img):
    for y in range(img.height):
        for x in range(img.width):
            if (x + y) % 2 == 0:
                img.putpixel((x, y), 1)


# Font 1: full-box A (6x10 at origin), size field 10, image 20x12
out["f1_fullbox_size10"] = probe(make_font(
    "f1", ";;;;;;10;", 20, 12, {65: [6, 0, 0, 0, 6, 10, 0, 0, 6, 10]}, diagonal))
# Font 2: same but descriptor size 20
out["f2_fullbox_size20"] = probe(make_font(
    "f2", ";;;;;;20;", 20, 12, {65: [6, 0, 0, 0, 6, 10, 0, 0, 6, 10]}, diagonal))
# Font 3: offset dst/src boxes
out["f3_offset"] = probe(make_font(
    "f3", ";;;;;;10;", 20, 12, {65: [6, 0, 0, 2, 6, 8, 2, 2, 8, 8]}, diagonal))
# Font 4: smaller dst than src (scaling or clip?)
out["f4_scaledown"] = probe(make_font(
    "f4", ";;;;;;10;", 20, 12, {65: [4, 0, 0, 0, 2, 2, 0, 0, 6, 6]}, diagonal))
# Font 5: dy nonzero + B entry, multiline
f5 = make_font(
    "f5", ";;;;;;10;", 20, 12,
    {65: [6, 0, 0, 0, 6, 4, 0, 0, 6, 4], 66: [4, 0, 0, 0, 4, 4, 6, 0, 10, 4]}, diagonal)
out["f5_multiline"] = {
    "A": probe(f5, "A"),
    "AB": probe(f5, "AB"),
    "A_newline_B": probe(f5, "A\nB"),
}
# Font 6: horizontal overlap (B dst overlaps A's advance)
out["f6_overlap"] = probe(make_font(
    "f6", ";;;;;;10;", 20, 12,
    {65: [6, 0, 0, 0, 6, 4, 0, 0, 6, 4], 66: [2, 0, -4, 0, 2, 4, 8, 0, 12, 4]}, diagonal), "AB")

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_pilfont2.json", "w") as fh:
    json.dump(out, fh, indent=1, default=str)
print("TMP", tmp)
