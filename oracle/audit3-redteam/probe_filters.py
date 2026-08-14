from PIL import Image, ImageFilter, ImageEnhance
import io

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
p("Kernel(size=(2,2))", lambda: im.filter(ImageFilter.Kernel((2,2),[1]*4)).size)
p("Kernel(size=(3,3), 8 elems)", lambda: im.filter(ImageFilter.Kernel((3,3),[1]*8)).size)
p("Color3DLUT wrong size", lambda: ImageFilter.Color3DLUT.generate(4).filter(Image.new("RGB",(4,4))))
p("Enhance.Contrast(2.0)", lambda: ImageEnhance.Contrast(im).enhance(2.0).size)
p("Enhance.Contrast(-1.0)", lambda: ImageEnhance.Contrast(im).enhance(-1.0).size)

# SGI rle option actually does something?
import io
p("SGI rle=1 save size", lambda: len(io.BytesIO().__setattr__('x',1) and io.BytesIO().getbuffer() ) if False else 0)

# PNG transparency on mode L (single int)
im9 = Image.new("L", (2,2), 5)
p("PNG L transparency=int", lambda: len((lambda b:(im9.save(b,"PNG",transparency=3), b.getvalue())[1])(io.BytesIO())))

# TIFF group3/group4 compression accepted?
imt = Image.new("1", (16,16), 1)
p("TIFF mode1 group4", lambda: len((lambda b:(imt.save(b,"TIFF",compression="group4"), b.getvalue())[1])(io.BytesIO())))
p("TIFF mode1 group3", lambda: len((lambda b:(imt.save(b,"TIFF",compression="group3"), b.getvalue())[1])(io.BytesIO())))
p("TIFF modeL tiff_ccitt", lambda: len((lambda b:(im.convert("L").save(b,"TIFF",compression="tiff_ccitt"), b.getvalue())[1])(io.BytesIO())))
