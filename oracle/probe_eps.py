"""BEHAV-EPS-001 oracle: Pillow 11.3.0 EPS save bytes and open error shapes."""
import io
import json

from PIL import Image


def save_bytes(im, **kwargs):
    buf = io.BytesIO()
    im.save(buf, format="EPS", **kwargs)
    return buf.getvalue()


out = {}

# 1. L save 4x2 with distinct bytes
l = Image.new("L", (4, 2))
l.putdata([0, 1, 2, 3, 4, 5, 6, 7])
out["save_L"] = save_bytes(l).decode("latin-1")

# 2. RGB save 4x2 with distinct bytes 0..23
rgb = Image.new("RGB", (4, 2))
rgb.putdata([(i * 3, i * 3 + 1, i * 3 + 2) for i in range(8)])
out["save_RGB"] = save_bytes(rgb).decode("latin-1")

# 3. RGB save 3x2 (check line wrapping at 39 bytes)
rgb3 = Image.new("RGB", (3, 2))
rgb3.putdata([(i * 3, i * 3 + 1, i * 3 + 2) for i in range(6)])
out["save_RGB_3x2"] = save_bytes(rgb3).decode("latin-1")

# 3b. L save 40x1 (40 bytes -> 39-byte line + 1 byte) for the wrap rule
l40 = Image.new("L", (40, 1))
l40.putdata(list(range(40)))
out["save_L_40x1"] = save_bytes(l40).decode("latin-1")

# 3c. RGB save 14x1 (42 bytes -> 39 + 3) proves mid-row wrap
rgb14 = Image.new("RGB", (14, 1))
rgb14.putdata([(i * 3, i * 3 + 1, i * 3 + 2) for i in range(14)])
out["save_RGB_14x1"] = save_bytes(rgb14).decode("latin-1")

# 4. CMYK save 2x2
cmyk = Image.new("CMYK", (2, 2))
cmyk.putdata([(0, 1, 2, 3), (4, 5, 6, 7), (8, 9, 10, 11), (12, 13, 14, 15)])
out["save_CMYK"] = save_bytes(cmyk).decode("latin-1")

# 5. mode errors
for mode in ["RGBA", "P", "1", "I;16", "F", "LA"]:
    im = Image.new(mode, (2, 2))
    if mode == "1":
        im.putdata([0, 1, 1, 0])
    if mode == "P":
        im.putdata([0, 1, 2, 3])
    if mode == "I;16":
        im.putdata([0, 1, 2, 3])
    try:
        save_bytes(im)
        out[f"err_{mode}"] = "NO ERROR"
    except Exception as e:
        out[f"err_{mode}"] = f"{type(e).__name__}: {e}"

# 6. open behaviors: valid EPS -> header parse + load error
eps_bytes = save_bytes(l)
im = Image.open(io.BytesIO(eps_bytes))
out["open_valid"] = {"mode": im.mode, "size": list(im.size), "info": dict(im.info)}
try:
    im.load()
    out["open_valid_load"] = "NO ERROR"
except Exception as e:
    out["open_valid_load"] = f"{type(e).__name__}: {e}"

# 7. bad magic -> identification error
out["err_bad_magic"] = None
try:
    Image.open(io.BytesIO(b"notaneps"))
    out["err_bad_magic"] = "NO ERROR"
except Exception as e:
    out["err_bad_magic"] = f"{type(e).__name__}: {e}"

# 8. missing PS-Adobe header
out["err_no_psadobe"] = None
try:
    Image.open(io.BytesIO(b"%!PS\n%%BoundingBox: 0 0 10 10\n%%EndComments\n"))
    out["err_no_psadobe"] = "NO ERROR"
except Exception as e:
    out["err_no_psadobe"] = f"{type(e).__name__}: {e}"

# 9. missing BoundingBox
out["err_no_bbox"] = None
try:
    Image.open(io.BytesIO(b"%!PS-Adobe-3.0 EPSF-3.0\n%%EndComments\n"))
    out["err_no_bbox"] = "NO ERROR"
except Exception as e:
    out["err_no_bbox"] = f"{type(e).__name__}: {e}"

# 10. BoundingBox (atend) without trailer -> cannot determine EPS bounding box
out["err_bbox_atend"] = None
try:
    Image.open(io.BytesIO(b"%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: (atend)\n%%EndComments\n"))
    out["err_bbox_atend"] = "NO ERROR"
except Exception as e:
    out["err_bbox_atend"] = f"{type(e).__name__}: {e}"

# 11. bad EPS header (non-DSC garbage line in comments)
out["err_bad_header"] = None
try:
    Image.open(io.BytesIO(b"%!PS-Adobe-3.0 EPSF-3.0\nTHIS IS GARBAGE\n%%BoundingBox: 0 0 10 10\n%%EndComments\n"))
    out["err_bad_header"] = "NO ERROR"
except Exception as e:
    out["err_bad_header"] = f"{type(e).__name__}: {e}"

# 12. ps extension / format description
out["format_description"] = Image.registered_extensions()
out["extensions"] = sorted(k for k, v in Image.registered_extensions().items() if v == "EPS")

with open(r"oracle/probe_eps.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1)
for key in sorted(out):
    value = out[key]
    if isinstance(value, str) and len(value) > 300:
        print(key, "=", repr(value[:300]), "... total", len(value))
    else:
        print(key, "=", value)
