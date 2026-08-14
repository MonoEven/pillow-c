import io, sys, tempfile, os
from PIL import Image

def p(label, fn):
    try:
        r = fn()
        print(f"{label} => OK {r!r}")
    except Exception as e:
        print(f"{label} => {type(e).__name__}: {str(e)!r}")

print("=== resize P/1 explicit BICUBIC pixel check ===")
pm = Image.new("P",(4,1))
for x in range(4): pm.putpixel((x,0), x)  # palette indices 0,1,2,3
pal = [0]*768
for i in range(4):
    pal[i*3]=i*60; pal[i*3+1]=i*60; pal[i*3+2]=i*60
pm.putpalette(pal)
print("P nearest indices:", list(Image.new("P",(4,1)).resize((2,1), Image.Resampling.NEAREST).getdata()))
# rebuild pm since resize returns new image; use explicit
r_bicubic = pm.resize((2,1), Image.Resampling.BICUBIC)
print("P bicubic indices:", list(r_bicubic.getdata()))
print("P bicubic mode:", r_bicubic.mode)

print("=== convert matrix lengths ===")
p("convert RGB matrix len4 pixel", lambda: Image.new("RGB",(1,1),(100,100,100)).convert("RGB",(1,0,0,0)).getpixel((0,0)))
p("convert RGB matrix len12 pixel", lambda: Image.new("RGB",(1,1),(100,100,100)).convert("RGB",(1,0,0,0, 0,1,0,0, 0,0,1,0)).getpixel((0,0)))
p("convert L matrix len4", lambda: Image.new("L",(1,1),100).convert("L",(2,0)).getpixel((0,0)))
p("convert L matrix len12", lambda: Image.new("L",(1,1),100).convert("L",tuple([1.0]*12)))
p("convert L matrix len3", lambda: Image.new("L",(1,1),100).convert("L",(1,0,0)))

print("=== convert LAB to various ===")
for m in ["1","L","LA","P","PA","RGB","RGBA","RGBX","CMYK","YCbCr","LAB","HSV","I","F","I;16"]:
    p(f"LAB->{m}", lambda m=m: Image.new("LAB",(1,1),(50,0,0)).convert(m).mode)

print("=== convert matrix to wrong modes ===")
p("convert L matrix to RGB", lambda: Image.new("RGB",(1,1)).convert("RGB",(2,0)))
p("convert RGBA matrix", lambda: Image.new("RGBA",(1,1)).convert("RGBA",(1,0,0,0)))

print("=== quantize edge ===")
p("quantize float colors RGB", lambda: Image.new("RGB",(1,1)).quantize(10.5))
p("quantize str colors RGB", lambda: Image.new("RGB",(1,1)).quantize("abc"))
p("quantize mode 1", lambda: Image.new("1",(1,1)).quantize(256))
p("quantize mode I", lambda: Image.new("I",(1,1)).quantize(256))
p("quantize mode F", lambda: Image.new("F",(1,1)).quantize(256))
p("quantize mode CMYK", lambda: Image.new("CMYK",(1,1)).quantize(256))
p("quantize mode LAB", lambda: Image.new("LAB",(1,1)).quantize(256))
p("quantize mode P", lambda: Image.new("P",(1,1)).quantize(256))
p("quantize kmeans -1", lambda: Image.new("RGB",(1,1)).quantize(256, kmeans=-1))
p("quantize palette non-P", lambda: Image.new("RGB",(1,1)).quantize(256, palette=Image.new("L",(1,1))))
p("quantize palette on L", lambda: Image.new("L",(1,1)).quantize(256, palette=Image.new("P",(1,1))))
p("quantize palette on RGBA", lambda: Image.new("RGBA",(1,1)).quantize(256, palette=Image.new("P",(1,1))))
p("quantize RGBA mediancut", lambda: Image.new("RGBA",(1,1)).quantize(256, method=Image.Quantize.MEDIANCUT))

print("=== closed image attribute access ===")
imc = Image.new("L",(2,2)); imc.close()
p("closed mode", lambda: imc.mode)
p("closed size", lambda: imc.size)
p("closed width", lambda: imc.width)
p("closed format", lambda: imc.format)
p("closed getbands", lambda: imc.getbands())
p("closed copy", lambda: imc.copy())
p("closed filename", lambda: imc.filename)

print("=== putpixel mode1 value semantics ===")
def mk1(v):
    im = Image.new("1",(1,1))
    im.putpixel((0,0), v)
    return im.getpixel((0,0))
p("putpixel 1 mode value 255", lambda: mk1(255))
p("putpixel 1 mode value 2", lambda: mk1(2))
p("putpixel 1 mode value 128", lambda: mk1(128))
p("putpixel 1 mode value -1", lambda: mk1(-1))

print("=== paste color box ===")
p("paste color 2box RGB", lambda: Image.new("RGB",(2,2)).paste((1,2,3),(0,0)))
p("paste int on RGB", lambda: Image.new("RGB",(2,2)).paste(5,(0,0,1,1)))
p("paste 4-tuple on L", lambda: Image.new("L",(2,2)).paste((1,2,3,4),(0,0,1,1)))

print("=== getdata band ===")
p("getdata L", lambda: list(Image.new("L",(2,2),5).getdata()))
p("getdata band 1 RGB", lambda: list(Image.new("RGB",(2,2),(1,2,3)).getdata(1)))
p("getdata band 3 RGB", lambda: Image.new("RGB",(2,2)).getdata(3))
p("getdata band -1 RGB", lambda: Image.new("RGB",(2,2)).getdata(-1))

print("=== tobytes/frombytes raw ===")
p("tobytes RGB", lambda: Image.new("RGB",(1,1),(1,2,3)).tobytes())
p("tobytes raw BGR", lambda: Image.new("RGB",(1,1),(1,2,3)).tobytes("raw","BGR"))
p("frombytes RGB raw RGB", lambda: Image.frombytes("RGB",(1,1), b"\x01\x02\x03").getpixel((0,0)))
p("frombytes RGB raw BGR", lambda: Image.frombytes("RGB",(1,1), b"\x01\x02\x03", "raw", "BGR").getpixel((0,0)))

print("=== entropy/histogram mask ===")
p("histogram mask RGB", lambda: Image.new("L",(2,2),5).histogram(Image.new("RGB",(2,2))))
p("histogram mask wrong size", lambda: Image.new("L",(2,2),5).histogram(Image.new("L",(3,3))))
p("entropy mask RGB", lambda: Image.new("L",(2,2),5).entropy(Image.new("RGB",(2,2))))
p("entropy mask wrong size", lambda: Image.new("L",(2,2),5).entropy(Image.new("L",(3,3))))

print("=== reduce ===")
p("reduce factor 0", lambda: Image.new("L",(4,4)).reduce(0))
p("reduce factor -1", lambda: Image.new("L",(4,4)).reduce(-1))
p("reduce float 1.5", lambda: Image.new("L",(4,4)).reduce(1.5))
p("reduce P mode", lambda: Image.new("P",(4,4)).reduce(2))
p("reduce I;16", lambda: Image.new("I;16",(4,4)).reduce(2))

print("=== transpose list ===")
for t in [Image.Transpose.FLIP_LEFT_RIGHT, Image.Transpose.FLIP_TOP_BOTTOM, Image.Transpose.ROTATE_90, Image.Transpose.ROTATE_180, Image.Transpose.ROTATE_270, Image.Transpose.TRANSPOSE, Image.Transpose.TRANSVERSE]:
    p(f"transpose {t}", lambda t=t: Image.new("L",(2,3)).transpose(t).size)
