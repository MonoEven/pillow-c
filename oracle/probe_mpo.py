"""BEHAV-MPO-001 oracle: Pillow 11.3.0 MPO save layout and open behavior."""
import io
import struct

from PIL import Image

out = {}


def save_mpo(im, append_images=None):
    buf = io.BytesIO()
    kwargs = {}
    if append_images is not None:
        kwargs["save_all"] = True
        kwargs["append_images"] = append_images
    im.save(buf, format="MPO", **kwargs)
    return buf.getvalue()


def make_l(w, h, fill):
    im = Image.new("L", (w, h))
    im.putdata([(fill + x + y) % 256 for y in range(h) for x in range(w)])
    return im


def make_rgb(w, h, base):
    im = Image.new("RGB", (w, h))
    im.putdata([((base + x + y) % 256, (base * 2 + x) % 256, (base * 3 + y) % 256) for y in range(h) for x in range(w)])
    return im


# single-image save (no append_images) -> plain JPEG
single = save_mpo(make_rgb(4, 2, 10))
out["single_head"] = single[:64].hex()
out["single_len"] = len(single)

# two-frame save
a = make_rgb(4, 2, 10)
b = make_rgb(4, 2, 60)
multi = save_mpo(a, [b])
out["multi_len"] = len(multi)
out["multi_head64"] = multi[:64].hex()
# find the APP2 MPF marker
idx = multi.find(b"MPF\x00")
out["mpf_marker_pos"] = idx
out["mpf_context"] = multi[max(0, idx - 8):idx + 80].hex() if idx >= 0 else None
out["bytes_at_28"] = multi[28:80].hex()
# where does the second JPEG start (SOI)?
soi2 = multi.find(b"\xff\xd8", 2)
out["soi2_pos"] = soi2
out["tail"] = multi[-48:].hex()

# reopen behavior
im = Image.open(io.BytesIO(multi))
out["open"] = {"format": im.format, "mode": im.mode, "size": list(im.size), "n_frames": getattr(im, "n_frames", None), "is_animated": getattr(im, "is_animated", None)}
out["frame0_head"] = im.tobytes()[:24].hex()
im.seek(1)
out["frame1"] = {"size": list(im.size), "mode": im.mode, "tobytes_head": im.tobytes()[:24].hex()}

# open a single-frame MPO (plain JPEG saved as MPO) -> malformed error?
single_open = None
try:
    im2 = Image.open(io.BytesIO(single))
    single_open = {"format": im2.format, "mode": im2.mode, "size": list(im2.size)}
except Exception as e:
    single_open = f"{type(e).__name__}: {e}"
out["open_single"] = single_open

# mode errors
for mode in ["RGBA", "P", "1", "I;16", "CMYK"]:
    im = Image.new(mode, (2, 2))
    if mode == "1":
        im.putdata([0, 1, 1, 0])
    if mode == "P":
        im.putdata([0, 1, 2, 3])
    if mode == "I;16":
        im.putdata([0, 1, 2, 3])
    try:
        save_mpo(im)
        out[f"err_{mode}"] = "NO ERROR"
    except Exception as e:
        out[f"err_{mode}"] = f"{type(e).__name__}: {e}"

# CMYK single save works?
cmyk = Image.new("CMYK", (2, 2))
cmyk.putdata([(0, 1, 2, 3), (4, 5, 6, 7), (8, 9, 10, 11), (12, 13, 14, 15)])
out["cmyk_save"] = save_mpo(cmyk)[:64].hex()

# format description
out["format_description_mpo"] = None
try:
    out["format_description_mpo"] = Image.registered_extensions() and [k for k, v in Image.registered_extensions().items() if v == "MPO"]
except Exception:
    pass

for key in sorted(out):
    print(key, "=", out[key])
