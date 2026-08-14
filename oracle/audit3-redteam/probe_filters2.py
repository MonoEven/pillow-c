from PIL import Image, ImageFilter, ImageEnhance
import io, traceback

def p(label, fn):
    try:
        print(f"OK  {label}: {fn()!r}")
    except Exception as e:
        print(f"ERR {label}: {type(e).__name__}: {e}")

im = Image.new("L", (8,8), 100)

p("GaussianBlur(radius=-1)", lambda: im.filter(ImageFilter.GaussianBlur(-1)))
p("BoxBlur(radius=-1)", lambda: im.filter(ImageFilter.BoxBlur(-1)))
p("UnsharpMask(radius=2, percent=150, threshold=3)", lambda: im.filter(ImageFilter.UnsharpMask(2,150,3)).size)
p("RankFilter(size=4)", lambda: im.filter(ImageFilter.RankFilter(4)).size)
p("RankFilter(size=5)", lambda: im.filter(ImageFilter.RankFilter(5)).size)
p("MinFilter(size=0)", lambda: im.filter(ImageFilter.MinFilter(0)).size)
p("MedianFilter(size=2)", lambda: im.filter(ImageFilter.MedianFilter(2)).size)
p("Kernel(size=(2,2), 4 elems)", lambda: im.filter(ImageFilter.Kernel((2,2),[1]*4)).size)
p("Kernel(size=(3,3), 8 elems)", lambda: im.filter(ImageFilter.Kernel((3,3),[1]*8)).size)
p("Enhance.Contrast(2.0)", lambda: ImageEnhance.Contrast(im).enhance(2.0).size)
p("Enhance.Contrast(-1.0)", lambda: ImageEnhance.Contrast(im).enhance(-1.0).size)

# TIFF group3/group4
imt = Image.new("1", (16,16), 1)
try:
    b = io.BytesIO(); imt.save(b, "TIFF", compression="group4")
    print("OK  TIFF mode1 group4:", len(b.getvalue()))
except Exception as e:
    print("ERR TIFF group4:", type(e).__name__, e)
try:
    b = io.BytesIO(); imt.save(b, "TIFF", compression="group3")
    print("OK  TIFF mode1 group3:", len(b.getvalue()))
except Exception as e:
    print("ERR TIFF group3:", type(e).__name__, e)

# PNG transparency int on L
im9 = Image.new("L", (2,2), 5)
try:
    b = io.BytesIO(); im9.save(b, "PNG", transparency=3)
    print("OK  PNG L transparency=int:", len(b.getvalue()))
except Exception as e:
    print("ERR PNG L transparency:", type(e).__name__, e)

# PNG transparency int on RGB (invalid)
imr = Image.new("RGB", (2,2), (1,2,3))
try:
    b = io.BytesIO(); imr.save(b, "PNG", transparency=3)
    print("OK  PNG RGB transparency=int:", len(b.getvalue()))
except Exception as e:
    print("ERR PNG RGB transparency=int:", type(e).__name__, e)
