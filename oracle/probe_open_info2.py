# API-OPENINFO-001 follow-up: GIF frames / TIFF / TGA / fresh attributes.
import io
from PIL import Image

def p(label, fn):
    try:
        print(f"OK  {label}: {fn()!r}")
    except Exception as e:
        print(f"ERR {label}: {type(e).__name__}: {e}")

def make_p(color):
    fr = Image.new("P", (4, 4), color)
    fr.putpalette([(c * 3) % 256 for c in range(256) for _ in (0, 1, 2)])
    return fr

# --- animated GIF
g = io.BytesIO()
f0, f1 = make_p(0), make_p(1)
f0.save(g, "GIF", save_all=True, append_images=[f1], duration=[100, 200], loop=3, comment=b"x")
g.seek(0)
im = Image.open(g)
p("gif version", lambda: im.info.get("version"))
p("gif extension", lambda: im.info.get("extension"))
p("gif background", lambda: im.info.get("background"))
p("gif duration", lambda: im.info.get("duration"))
p("gif loop", lambda: im.info.get("loop"))
p("gif comment", lambda: im.info.get("comment"))
p("gif n_frames", lambda: im.n_frames)
im.seek(1)
p("gif f1 version", lambda: im.info.get("version"))
p("gif f1 extension", lambda: im.info.get("extension"))
p("gif f1 duration", lambda: im.info.get("duration"))
p("gif f1 background", lambda: im.info.get("background"))

# --- GIF87a (write with version param unsupported; use a crafted 87a header)
# simplest: take the 89a bytes and rewrite the header
data = g.getvalue()
data87 = b"GIF87a" + data[6:]
im87 = Image.open(io.BytesIO(data87))
p("gif87a version", lambda: im87.info.get("version"))

# --- TIFF
for comp in (None, "raw", "tiff_lzw", "packbits", "tiff_adobe_deflate", "jpeg"):
    t = io.BytesIO()
    imt = Image.new("L", (8, 8), 100)
    try:
        imt.save(t, "TIFF", compression=comp)
        t.seek(0)
        opened = Image.open(t)
        print(f"OK  tiff {comp}: compression={opened.info.get('compression')!r}")
        opened.close()
    except Exception as e:
        print(f"ERR tiff {comp}: {type(e).__name__}: {e}")
t = io.BytesIO()
Image.new("1", (8, 8), 1).save(t, "TIFF", compression="group4")
t.seek(0)
opened = Image.open(t)
p("tiff group4", lambda: opened.info.get("compression"))
opened.close()

# --- TGA
for comp, orient in [(None, None), ("rle", 1), (None, -1)]:
    tga = io.BytesIO()
    imt = Image.new("RGB", (4, 4), (1, 2, 3))
    try:
        imt.save(tga, "TGA", **({} if comp is None else {"compression": comp}), **({} if orient is None else {"orientation": orient}))
        tga.seek(0)
        opened = Image.open(tga)
        print(f"OK  tga {comp}/{orient}: compression={opened.info.get('compression')!r} orientation={opened.info.get('orientation')!r}")
        opened.close()
    except Exception as e:
        print(f"ERR tga {comp}/{orient}: {type(e).__name__}: {e}")

# --- fresh image attribute shapes
fresh = Image.new("RGB", (2, 2))
p("fresh quantization attr", lambda: hasattr(fresh, "quantization"))
p("fresh quantization", lambda: fresh.quantization)
p("fresh palette attr", lambda: hasattr(fresh, "palette"))
p("fresh mode attr", lambda: fresh.mode)
p("fresh info", lambda: fresh.info)

# --- ImageFile-level attributes on opened files (JPEG)
b = io.BytesIO()
Image.new("RGB", (4, 4)).save(b, "JPEG")
b.seek(0)
imj = Image.open(b)
p("jpeg custom_mimetype", lambda: imj.get_format_mimetype())
p("jpeg format", lambda: imj.format)
p("jpeg size", lambda: imj.size)
p("jpeg tile", lambda: [t[0] for t in imj.tile])
