"""BEHAV-OPEN-008 oracle: MPEG open behaviors in Pillow 11.3.0."""
import io
import json

from PIL import Image

out = {}


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


def mpeg_header(w, h):
    # sequence header: 0x000001B3 + 12-bit w + 12-bit h (contiguous bit
    # fields) + filler
    body = ((w & 0xFFF) << 12) | (h & 0xFFF)
    return b"\x00\x00\x01\xb3" + body.to_bytes(3, "big") + b"\x10"


def open_meta(blob):
    im = Image.open(io.BytesIO(blob), formats=["MPEG"])
    return (im.format, im.mode, list(im.size), getattr(im, "n_frames", None), getattr(im, "is_animated", None))


def open_load(blob):
    im = Image.open(io.BytesIO(blob), formats=["MPEG"])
    try:
        im.load()
        return ("loaded", im.tobytes()[:16].hex())
    finally:
        im.close()


out["mpeg_open"] = capture(lambda: open_meta(mpeg_header(352, 288)))
out["mpeg_open2"] = capture(lambda: open_meta(mpeg_header(0xABC, 0x123)))
out["mpeg_load"] = capture(lambda: open_load(mpeg_header(352, 288)))
out["mpeg_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00\x00\x01\x00" * 8), formats=["MPEG"]))
out["mpeg_truncated"] = capture(lambda: Image.open(io.BytesIO(b"\x00\x00\x01\xb3"), formats=["MPEG"]))
out["mpeg_truncated2"] = capture(lambda: open_meta(b"\x00\x00\x01\xb3" + b"\x00\x00"))
out["mpeg_save"] = capture(lambda: (lambda im: im.save(io.BytesIO(), "MPEG"))(Image.new("L", (1, 1))))
out["mpeg_save_mpeg_ext"] = capture(lambda: (lambda im: im.save(io.BytesIO(), "x.mpeg"))(Image.new("L", (1, 1))))
out["mpeg_description"] = Image.open(io.BytesIO(mpeg_header(16, 16)), formats=["MPEG"]).format_description

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_8.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
