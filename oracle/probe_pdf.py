"""BEHAV-PDF-001 oracle: Pillow 11.3.0 PDF save layout and open behavior."""
import io
import json
import os
import tempfile

from PIL import Image

out = {}

tmp = tempfile.mkdtemp(prefix="pillow-pdf-probe-")
single_path = os.path.join(tmp, "single.pdf")
rgb_path = os.path.join(tmp, "rgb.pdf")
l_path = os.path.join(tmp, "gray.pdf")
cmyk_path = os.path.join(tmp, "cmyk.pdf")
bw_path = os.path.join(tmp, "bw.pdf")
la_path = os.path.join(tmp, "la.pdf")
rgba_path = os.path.join(tmp, "rgba.pdf")
multi_path = os.path.join(tmp, "multi.pdf")
dpi_path = os.path.join(tmp, "dpi.pdf")
res0_path = os.path.join(tmp, "res0.pdf")


def make_l(w, h, fill):
    im = Image.new("L", (w, h))
    im.putdata([(fill + x + y) % 256 for y in range(h) for x in range(w)])
    return im


def make_rgb(w, h, base):
    im = Image.new("RGB", (w, h))
    im.putdata([((base + x + y) % 256, (base * 2 + x) % 256, (base * 3 + y) % 256) for y in range(h) for x in range(w)])
    return im


def make_p():
    im = Image.new("P", (4, 3))
    im.putpalette(bytes(i % 256 for i in range(768)))
    im.putdata([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11])
    return im


def make_cmyk(w, h):
    im = Image.new("CMYK", (w, h))
    im.putdata([((x * 4) % 256, (y * 7) % 256, 128, 255) for y in range(h) for x in range(w)])
    return im


def make_bw():
    im = Image.new("1", (4, 3))
    im.putdata([0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0])
    return im


# P-mode save -> ASCIIHexDecode (fully deterministic except timestamps)
p = make_p()
p.save(single_path, format="PDF")
data = open(single_path, "rb").read()
out["single_p_len"] = len(data)
out["single_p_head"] = data[:200].hex()
out["single_p_tail"] = data[-260:].hex()
out["single_p_full"] = data.hex()

# RGB/L/CMYK -> DCTDecode JPEG payloads
make_rgb(4, 2, 10).save(rgb_path, format="PDF")
make_l(4, 2, 30).save(l_path, format="PDF")
make_cmyk(4, 2).save(cmyk_path, format="PDF")
for name, path in [("rgb", rgb_path), ("gray", l_path), ("cmyk", cmyk_path)]:
    data = open(path, "rb").read()
    out[f"{name}_len"] = len(data)
    out[f"{name}_head"] = data[:260].hex()

# mode 1 -> CCITTFaxDecode group4
make_bw().save(bw_path, format="PDF")
data = open(bw_path, "rb").read()
out["bw_len"] = len(data)
out["bw_head"] = data[:400].hex()

# LA/RGBA -> JPXDecode (JPEG2000)
try:
    Image.new("LA", (2, 2)).save(la_path, format="PDF")
    out["la_save"] = "OK len=" + str(len(open(la_path, "rb").read()))
except Exception as e:
    out["la_save"] = f"{type(e).__name__}: {e}"
try:
    Image.new("RGBA", (2, 2)).save(rgba_path, format="PDF")
    out["rgba_save"] = "OK len=" + str(len(open(rgba_path, "rb").read()))
except Exception as e:
    out["rgba_save"] = f"{type(e).__name__}: {e}"

# mode errors
for mode in ["I", "F", "I;16", "RGBX"]:
    try:
        im = Image.new(mode, (2, 2))
        im.save(os.path.join(tmp, "err.pdf"), format="PDF")
        out[f"err_{mode}"] = "NO ERROR"
    except Exception as e:
        out[f"err_{mode}"] = f"{type(e).__name__}: {e}"

# P with transparency -> SMask via JPXDecode
ptrans = Image.new("P", (2, 2))
ptrans.info["transparency"] = b"\x00\xff"
try:
    ptrans.save(os.path.join(tmp, "ptrans.pdf"), format="PDF")
    out["ptrans_save"] = "OK len=" + str(len(open(os.path.join(tmp, "ptrans.pdf"), "rb").read()))
except Exception as e:
    out["ptrans_save"] = f"{type(e).__name__}: {e}"

# dpi option
make_rgb(4, 2, 10).save(dpi_path, format="PDF", dpi=(300.0, 150.0))
data = open(dpi_path, "rb").read()
out["dpi_head"] = data[:700].hex()

# resolution option
make_rgb(4, 2, 10).save(os.path.join(tmp, "res.pdf"), format="PDF", resolution=144.0)
out["res_head"] = open(os.path.join(tmp, "res.pdf"), "rb").read()[:700].hex()

# resolution=0 -> ZeroDivisionError?
try:
    make_rgb(4, 2, 10).save(res0_path, format="PDF", resolution=0)
    out["res0"] = "NO ERROR"
except Exception as e:
    out["res0"] = f"{type(e).__name__}: {e}"

# multi-page: save_all + append_images
a = make_rgb(4, 2, 10)
b = make_rgb(4, 2, 60)
a.save(multi_path, format="PDF", save_all=True, append_images=[b])
data = open(multi_path, "rb").read()
out["multi_len"] = len(data)
out["multi_head"] = data[:800].hex()
out["multi_tail"] = data[-260:].hex()

# BytesIO save (filename None)
try:
    buf = io.BytesIO()
    make_p().save(buf, format="PDF")
    out["bytesio_save"] = "OK len=" + str(len(buf.getvalue()))
    out["bytesio_head"] = buf.getvalue()[:200].hex()
except Exception as e:
    out["bytesio_save"] = f"{type(e).__name__}: {e}"

# open behavior: unregistered?
try:
    im = Image.open(single_path)
    out["open"] = {"format": im.format, "mode": im.mode, "size": list(im.size)}
except Exception as e:
    out["open"] = f"{type(e).__name__}: {e}"

# registered extensions / description
try:
    out["ext_pdf"] = [k for k, v in Image.registered_extensions().items() if v == "PDF"]
except Exception:
    out["ext_pdf"] = None
out["format_description"] = None
try:
    out["format_description"] = Image.format_description("PDF")
except Exception:
    pass
out["version"] = Image.__version__

print(json.dumps(out, indent=1, default=str))
with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "probe_pdf.json"), "w") as f:
    json.dump(out, f, indent=1, default=str)
