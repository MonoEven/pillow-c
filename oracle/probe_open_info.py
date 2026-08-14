# API-OPENINFO-001 oracle probe: open-side info attributes that the facade
# does not expose yet (JPEG quantization/progressive/adobe, GIF
# version/extension, TIFF compression, TGA compression/orientation).
import io
from PIL import Image, ImageFile

def p(label, fn):
    try:
        print(f"OK  {label}: {fn()!r}")
    except Exception as e:
        print(f"ERR {label}: {type(e).__name__}: {e}")

# --- JPEG baseline (through PIL's own encoder)
base = io.BytesIO()
Image.new("RGB", (8, 8), (1, 2, 3)).save(base, "JPEG")
base.seek(0)
im = Image.open(base)
p("jpeg quantization", lambda: (im.quantization, len(im.quantization)))
p("jpeg quantization first table", lambda: (len(im.quantization[0]), im.quantization[0][:4]))
p("jpeg progressive", lambda: im.info.get("progressive"))
p("jpeg progression", lambda: im.info.get("progression"))
p("jpeg adobe", lambda: im.info.get("adobe"))
p("jpeg adobe_transform", lambda: im.info.get("adobe_transform"))
p("jpeg jfif", lambda: im.info.get("jfif"))
p("jpeg jfif_version", lambda: im.info.get("jfif_version"))

# --- JPEG progressive
prog = io.BytesIO()
Image.new("RGB", (8, 8), (1, 2, 3)).save(prog, "JPEG", progressive=True)
prog.seek(0)
imp = Image.open(prog)
p("prog progressive", lambda: imp.info.get("progressive"))
p("prog progression", lambda: imp.info.get("progression"))

# --- JPEG CMYK (adobe marker)
cmyk = io.BytesIO()
Image.new("CMYK", (8, 8), (1, 2, 3, 4)).save(cmyk, "JPEG")
cmyk.seek(0)
imc = Image.open(cmyk)
p("cmyk adobe", lambda: imc.info.get("adobe"))
p("cmyk adobe_transform", lambda: imc.info.get("adobe_transform"))

# --- JPEG qtables explicit
qt = io.BytesIO()
Image.new("L", (8, 8), 5).save(qt, "JPEG", qtables=[[4] * 64])
qt.seek(0)
imq = Image.open(qt)
p("qtables quantization first4", lambda: imq.quantization[0][:4])

# --- GIF
g = io.BytesIO()
frames = []
first = Image.new("P", (4, 4))
first.putpalette([i for c in range(256) for i in (c, 0, 0)])
frames.append(first)
frames.append(Image.new("P", (4, 4), 1))
frames[0].save(g, "GIF", save_all=True, append_images=frames[1:], duration=100, comment=b"hello")
g.seek(0)
img = Image.open(g)
p("gif version", lambda: img.info.get("version"))
p("gif extension", lambda: img.info.get("extension"))
p("gif background", lambda: img.info.get("background"))
p("gif duration", lambda: img.info.get("duration"))
p("gif comment", lambda: img.info.get("comment"))
img.seek(1)
p("gif frame1 version", lambda: img.info.get("version"))
p("gif frame1 extension", lambda: img.info.get("extension"))

# --- animated GIF with loop
g2 = io.BytesIO()
frames[0].save(g2, "GIF", save_all=True, append_images=frames[1:], duration=[100, 200], loop=3, comment=b"x")
g2.seek(0)
im2 = Image.open(g2)
p("gif2 version", lambda: im2.info.get("version"))
p("gif2 extension", lambda: im2.info.get("extension"))
p("gif2 duration", lambda: im2.info.get("duration"))
im2.seek(1)
p("gif2 f1 extension", lambda: im2.info.get("extension"))
p("gif2 f1 duration", lambda: im2.info.get("duration"))
p("gif2 loop", lambda: im2.info.get("loop"))

# --- TIFF compressions
for comp in (None, "raw", "tiff_lzw", "packbits", "tiff_adobe_deflate", "tiff_ccitt", "jpeg"):
    t = io.BytesIO()
    mode = "1" if comp == "tiff_ccitt" else "L"
    imt = Image.new(mode, (8, 8), 100)
    try:
        imt.save(t, "TIFF", compression=comp)
        t.seek(0)
        opened = Image.open(t)
        print(f"OK  tiff {comp}: compression={opened.info.get('compression')!r} mode={opened.mode}")
        opened.close()
    except Exception as e:
        print(f"ERR tiff {comp}: {type(e).__name__}: {e}")

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

# --- opened image attribute shapes after load
b = io.BytesIO()
Image.new("RGB", (2, 2)).save(b, "PNG")
b.seek(0)
imp2 = Image.open(b)
p("png info keys", lambda: sorted(imp2.info.keys()))
imp2.load()
p("png info keys after load", lambda: sorted(imp2.info.keys()))

# fresh images: no quantization attribute?
fresh = Image.new("RGB", (2, 2))
p("fresh quantization", lambda: fresh.quantization)
p("fresh has quantization attr", lambda: hasattr(fresh, "quantization"))
