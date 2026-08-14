import io, sys, os
from PIL import Image, ImageDraw, ImageFont

def probe(label, fn):
    try:
        r = fn()
        print(f"OK   {label}: {r!r}")
    except Exception as e:
        print(f"ERR  {label}: {type(e).__name__}: {e}")

out = r"C:\Users\78442\AppData\Local\Temp\pillow-audit"

def img(mode="RGB", size=(8,8)):
    return Image.new(mode, size, (10,20,30) if mode in ("RGB","RGBA") else 10)

# ---------------- PNG ----------------
def png_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "PNG", **kw)
    return len(b.getvalue())

# compress_type / dictionary
probe("PNG compress_type=1", lambda: png_save(compress_type=1))
probe("PNG compress_type=999 (invalid)", lambda: png_save(compress_type=999))
probe("PNG dictionary=b'hello'", lambda: png_save(dictionary=b"hello"))
probe("PNG compress_level=99 (invalid)", lambda: png_save(compress_level=99))
probe("PNG compress_level=-5 (invalid)", lambda: png_save(compress_level=-5))
probe("PNG compress_level='abc' (invalid type)", lambda: png_save(compress_level="abc"))
# bits for P mode
def png_p_bits(bits):
    im = Image.new("P", (8,8))
    im.putpalette([i//3 for i in range(768)])
    b = io.BytesIO()
    im.save(b, "PNG", bits=bits)
    return len(b.getvalue())
probe("PNG P-mode bits=1", lambda: png_p_bits(1))
probe("PNG P-mode bits=2", lambda: png_p_bits(2))
probe("PNG P-mode bits=9 (invalid)", lambda: png_p_bits(9))
# interlace / gamma: not Pillow options
probe("PNG interlace=True (ignored by Pillow?)", lambda: png_save(interlace=True))
probe("PNG gamma=2.2 (ignored by Pillow?)", lambda: png_save(gamma=2.2))

# ---------------- JPEG ----------------
def jpeg_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "JPEG", **kw)
    return len(b.getvalue())
probe("JPEG smooth=100", lambda: jpeg_save(smooth=100))
probe("JPEG streamtype=1", lambda: jpeg_save(streamtype=1))
probe("JPEG quality=-10", lambda: jpeg_save(quality=-10))
probe("JPEG quality=101", lambda: jpeg_save(quality=101))
probe("JPEG quality='bogus'", lambda: jpeg_save(quality="bogus"))
probe("JPEG subsampling='bogus'", lambda: jpeg_save(subsampling="bogus"))
probe("JPEG subsampling=7", lambda: jpeg_save(subsampling=7))

# ---------------- TIFF ----------------
def tiff_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "TIFF", **kw)
    return len(b.getvalue())
probe("TIFF quality=80 (jpeg compression)", lambda: tiff_save(compression="jpeg", quality=80))
probe("TIFF quality=80 (raw compression -> error)", lambda: tiff_save(quality=80))
probe("TIFF quality=200 (invalid)", lambda: tiff_save(compression="jpeg", quality=200))
probe("TIFF quality='x' (invalid type)", lambda: tiff_save(compression="jpeg", quality="x"))
probe("TIFF strip_size=4096", lambda: tiff_save(strip_size=4096))
probe("TIFF resolution=300", lambda: tiff_save(resolution=300))
probe("TIFF resolution_unit=3", lambda: tiff_save(resolution=300, resolution_unit=3))
probe("TIFF description='hello'", lambda: tiff_save(description="hello"))
probe("TIFF software='x'", lambda: tiff_save(software="x"))
probe("TIFF artist='a'", lambda: tiff_save(artist="a"))
probe("TIFF date_time='2020:01:01 00:00:00'", lambda: tiff_save(date_time="2020:01:01 00:00:00"))
probe("TIFF copyright='c'", lambda: tiff_save(copyright="c"))
probe("TIFF compression='tiff_jpeg' alias", lambda: tiff_save(compression="tiff_jpeg"))
probe("TIFF compression='tiff_deflate' alias", lambda: tiff_save(compression="tiff_deflate"))
probe("TIFF compression='bogus'", lambda: tiff_save(compression="bogus"))

# ---------------- QOI ----------------
def qoi_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "QOI", **kw)
    return len(b.getvalue())
probe("QOI colorspace='sRGB'", lambda: qoi_save(colorspace="sRGB"))
probe("QOI colorspace='linear'", lambda: qoi_save(colorspace="linear"))

# ---------------- TGA ----------------
def tga_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "TGA", **kw)
    return len(b.getvalue())
probe("TGA id_section=b'abc'", lambda: tga_save(id_section=b"abc"))
probe("TGA orientation=2", lambda: tga_save(orientation=2))
probe("TGA rle=True", lambda: tga_save(rle=True))
probe("TGA compression='tga_rle'", lambda: tga_save(compression="tga_rle"))
probe("TGA compression='bogus'", lambda: tga_save(compression="bogus"))

# ---------------- GIF ----------------
def gif_save(**kw):
    b = io.BytesIO()
    im = Image.new("P", (8,8), 1)
    im.save(b, "GIF", **kw)
    return len(b.getvalue())
probe("GIF interlace=True", lambda: gif_save(interlace=True))
probe("GIF interlace=False", lambda: gif_save(interlace=False))
probe("GIF palette=custom", lambda: gif_save(palette=[0,0,0, 255,255,255]))

# ---------------- BMP / PPM / ICO ----------------
def bmp_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "BMP", **kw)
    return len(b.getvalue())
probe("BMP bogus option (ignored?)", lambda: bmp_save(bogus=1))

def ico_save(**kw):
    b = io.BytesIO()
    img("RGBA").save(b, "ICO", **kw)
    return len(b.getvalue())
probe("ICO sizes=[(16,16)]", lambda: ico_save(sizes=[(16,16)]))
probe("ICO sizes=bogus", lambda: ico_save(sizes="bogus"))
probe("ICO bitmap_format='bmp'", lambda: ico_save(bitmap_format="bmp"))
probe("ICO bitmap_format='bogus'", lambda: ico_save(bitmap_format="bogus"))

# ---------------- SGI ----------------
def sgi_save(**kw):
    b = io.BytesIO()
    img("RGB").save(b, "SGI", **kw)
    return len(b.getvalue())
probe("SGI bpc=1", lambda: sgi_save(bpc=1))
probe("SGI bpc=2", lambda: sgi_save(bpc=2))
probe("SGI bpc=3 (invalid)", lambda: sgi_save(bpc=3))
probe("SGI rle=0", lambda: sgi_save(rle=0))
probe("SGI rle=1 (invalid?)", lambda: sgi_save(rle=1))

print("---- done ----")
