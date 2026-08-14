import io, sys, traceback
from PIL import Image

def p(label, fn):
    try:
        r = fn()
        print(f"{label} => OK {r!r}")
    except Exception as e:
        print(f"{label} => {type(e).__name__}: {str(e)!r}")

print("=== resize default/forced resample ===")
p("resize I;16 default", lambda: Image.new("I;16", (4,4)).resize((2,2)).getpixel((0,0)))
p("resize I;16B default", lambda: Image.new("I;16B", (4,4)).resize((2,2)))
p("resize P explicit BICUBIC", lambda: Image.new("P", (4,4)).resize((2,2), Image.Resampling.BICUBIC))
p("resize 1 explicit BICUBIC", lambda: Image.new("1", (4,4)).resize((2,2), Image.Resampling.BICUBIC))
p("resize BGR;15 default", lambda: Image.new("BGR;15", (4,4)).resize((2,2)))
p("resize L default size", lambda: Image.new("L",(4,4)).resize((2,2)).size)
p("resize unknown resample 999", lambda: Image.new("L",(4,4)).resize((2,2), 999))
p("resize reducing_gap 0.5", lambda: Image.new("L",(4,4)).resize((2,2), reducing_gap=0.5))

print("=== crop ===")
im = Image.new("L",(10,10),100)
p("crop float box", lambda: im.crop((0.6,0.6,4.4,4.4)).size)
p("crop inverted right<left", lambda: im.crop((5,5,0,0)))
p("crop inverted lower<upper", lambda: im.crop((0,5,5,0)))
p("crop negative box", lambda: im.crop((-2,-2,2,2)).size)
p("crop None", lambda: im.crop().size)
p("crop out of bounds", lambda: im.crop((8,8,12,12)).size)

print("=== convert ===")
p("convert RGB matrix len4", lambda: Image.new("RGB",(1,1)).convert("RGB", (1,0,0,0)))
p("convert L matrix len12", lambda: Image.new("L",(1,1)).convert("L", tuple([1.0]*12)))
p("convert P matrix", lambda: Image.new("RGB",(1,1)).convert("P", (1,0,0,0)))
p("convert matrix to P", lambda: Image.new("RGB",(1,1)).convert("P", matrix=(1,0,0,0)))
p("RGBA->RGB alpha", lambda: Image.new("RGBA",(1,1),(255,0,0,0)).convert("RGB").getpixel((0,0)))
p("RGBA->P", lambda: Image.new("RGBA",(1,1),(10,20,30,40)).convert("P").mode)
p("LAB->RGB", lambda: Image.new("LAB",(1,1),(50,0,0)).convert("RGB"))
p("LAB->L", lambda: Image.new("LAB",(1,1),(50,0,0)).convert("L"))
p("convert no mode P img", lambda: Image.new("P",(1,1)).convert().mode)
p("convert same mode", lambda: Image.new("RGB",(1,1)).convert("RGB").size)
p("RGB->1 default dither", lambda: Image.new("RGB",(2,2),(100,100,100)).convert("1").getpixel((0,0)))

print("=== quantize ===")
p("quantize 0 colors", lambda: Image.new("RGB",(1,1)).quantize(0))
p("quantize 257 colors", lambda: Image.new("RGB",(1,1)).quantize(257))
p("quantize L mode", lambda: Image.new("L",(1,1)).quantize(256))
p("quantize method=2 (median) L", lambda: Image.new("L",(1,1)).quantize(256, method=2))

print("=== getpixel/putpixel ===")
im = Image.new("L",(2,2))
p("getpixel oob", lambda: im.getpixel((5,5)))
p("getpixel negative", lambda: im.getpixel((-1,0)))
p("getpixel 3-tuple on L", lambda: im.getpixel((0,0,0)))
p("putpixel wrong tuple RGB len2", lambda: Image.new("RGB",(1,1)).putpixel((0,0),(1,2)))
p("putpixel mode1 value 255", lambda: Image.new("1",(1,1)).putpixel((0,0),255))
p("putpixel mode1 value 1 getpixel", lambda: (lambda i: (i.putpixel((0,0),1), i.getpixel((0,0))))(Image.new("1",(1,1))))
p("putpixel oob", lambda: im.putpixel((5,5),0))
p("putpixel string color", lambda: Image.new("RGB",(1,1)).putpixel((0,0),"red"))
p("putpixel I mode float", lambda: Image.new("I",(1,1)).putpixel((0,0), 300))

print("=== save/open ===")
im = Image.new("RGB",(2,2))
p("save unknown ext", lambda: im.save(r"%TEMP%\pillow-audit\x.unknownext".replace("%TEMP%", __import__("tempfile").gettempdir())))
p("open nonexistent", lambda: Image.open(r"%TEMP%\pillow-audit\does_not_exist.png".replace("%TEMP%", __import__("tempfile").gettempdir())))
p("open bad mode arg", lambda: Image.open(r"%TEMP%\pillow-audit\x.png".replace("%TEMP%", __import__("tempfile").gettempdir()), mode="RGB"))
p("save to bad dir", lambda: im.save(r"Z:\nonexistent\dir\x.png"))

print("=== closed image ===")
imc = Image.new("L",(2,2)); imc.close()
p("closed getpixel", lambda: imc.getpixel((0,0)))
p("closed resize", lambda: imc.resize((1,1)))
p("closed load", lambda: imc.load())
p("closed mode", lambda: imc.mode)

print("=== point ===")
p("point bad lut len", lambda: Image.new("L",(1,1)).point([1,2,3]))
p("point 256 lut", lambda: Image.new("L",(1,1)).point([255]*256))
p("point 512 lut L", lambda: Image.new("L",(1,1)).point([255]*512))
p("point mode=RGB on L lut256", lambda: Image.new("L",(1,1)).point([255]*256, "RGB"))
p("point I list", lambda: Image.new("I",(1,1)).point([255]*256))

print("=== filter ===")
p("filter bad type", lambda: Image.new("L",(1,1)).filter("x"))
p("filter int", lambda: Image.new("L",(1,1)).filter(5))

print("=== getchannel ===")
p("getchannel X on RGB", lambda: Image.new("RGB",(1,1)).getchannel("X"))
p("getchannel 5 on RGB", lambda: Image.new("RGB",(1,1)).getchannel(5))
p("getchannel R on RGB mode", lambda: Image.new("RGB",(1,1)).getchannel("R").mode)

print("=== getbands ===")
for m in ["1","L","LA","P","PA","RGB","RGBA","RGBX","CMYK","YCbCr","LAB","HSV","I","F","I;16","BGR;24"]:
    try:
        print(f"getbands {m} => {Image.new(m,(1,1)).getbands()}")
    except Exception as e:
        print(f"getbands {m} => {type(e).__name__}: {str(e)!r}")

print("=== getcolors ===")
p("getcolors 0", lambda: Image.new("L",(1,1)).getcolors(0))
p("getcolors 256 solid", lambda: Image.new("L",(2,2),5).getcolors(256))
p("getcolors I mode", lambda: Image.new("I",(1,1)).getcolors(256))
p("getcolors F mode", lambda: Image.new("F",(1,1)).getcolors(256))

print("=== getbbox ===")
p("getbbox empty", lambda: Image.new("L",(0,0)).getbbox())
p("getbbox all zero", lambda: Image.new("L",(2,2),0).getbbox())
p("getbbox alpha_only", lambda: Image.new("RGBA",(2,2),(255,255,255,0)).getbbox())
p("getbbox alpha_only=False", lambda: Image.new("RGBA",(2,2),(255,255,255,0)).getbbox(alpha_only=False))

print("=== extrema/entropy/histogram ===")
p("getextrema L", lambda: Image.new("L",(2,2),5).getextrema())
p("getextrema RGB", lambda: Image.new("RGB",(1,1),(1,2,3)).getextrema())
p("getextrema empty", lambda: Image.new("L",(0,0)).getextrema())
p("entropy L", lambda: Image.new("L",(2,2),5).entropy())
p("histogram L", lambda: len(Image.new("L",(2,2),5).histogram()))

print("=== tobytes/frombytes ===")
im = Image.new("L",(2,2))
b = im.tobytes()
p("tobytes L len", lambda: len(im.tobytes()))
p("tobytes encoder raw args", lambda: Image.new("RGB",(1,1)).tobytes("raw","RGB"))
p("frombytes bad size", lambda: Image.frombytes("L",(4,4), b"\x00"*4))
p("frombytes I;16", lambda: Image.frombytes("I;16",(1,1), b"\x01\x00").getpixel((0,0)))

print("=== thumbnail ===")
im = Image.new("L",(10,10))
p("thumbnail returns", lambda: im.thumbnail((5,5)))
p("thumbnail size after", lambda: im.size)
p("thumbnail 0 size", lambda: Image.new("L",(10,10)).thumbnail((0,0)))
p("thumbnail neg size", lambda: Image.new("L",(10,10)).thumbnail((-5,5)))

print("=== transpose ===")
p("transpose flip", lambda: Image.new("L",(2,2)).transpose(Image.Transpose.FLIP_LEFT_RIGHT).size)
p("transpose bad", lambda: Image.new("L",(2,2)).transpose(99))

print("=== size/filename/format/mode ===")
im = Image.new("RGB",(3,4))
print("new: filename=", repr(im.filename), "format=", repr(im.format), "mode=", im.mode, "size=", im.size)
import tempfile, os
td = tempfile.gettempdir()
fp = os.path.join(td, "pillow-audit", "probe_tmp.png")
Image.new("RGB",(2,2)).save(fp)
o = Image.open(fp)
print("open: filename=", repr(o.filename), "format=", repr(o.format), "mode=", o.mode)
print("info dict:", o.info)

print("=== copy/info ===")
im = Image.new("RGB",(2,2)); im.info["foo"]="bar"
c = im.copy()
print("copy info:", c.info)
print("copy same mode:", c.mode, c.size)

print("=== paste ===")
p("paste color 4box", lambda: (lambda i: (i.paste((1,2,3),(0,0,1,1)), i.getpixel((0,0))))(Image.new("RGB",(2,2))))
p("paste 2box no size", lambda: Image.new("RGB",(2,2)).paste((1,2,3),(0,0)))
p("paste image 2box", lambda: Image.new("RGB",(2,2)).paste(Image.new("RGB",(1,1)),(0,0)))
p("paste RGBA onto RGB", lambda: Image.new("RGB",(2,2)).paste(Image.new("RGBA",(1,1),(255,0,0,0)),(0,0)))
