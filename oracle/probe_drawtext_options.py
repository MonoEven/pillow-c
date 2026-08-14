# API-DRAW-TEXT-001 oracle probe: ImageDraw text/multiline_text option
# surface, getdraw/ImageDraw2, ImageStat.Global, ImageFilter base classes,
# ImageMath lambda_eval/imagemath_* — pinned against local Pillow 11.3.0.
import io
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageMath, ImageStat
from PIL import ImageDraw2

def p(label, fn):
    try:
        r = fn()
        print(f"OK  {label}: {r!r}")
    except Exception as e:
        print(f"ERR {label}: {type(e).__name__}: {e}")

ARIAL = "C:/Windows/Fonts/arial.ttf"
font = ImageFont.truetype(ARIAL, 16)
bmp = ImageFont.load_default_imagefont()
im = Image.new("RGB", (300, 200))
d = ImageDraw.Draw(im)

# --- direction/features/language: libraqm KeyError shapes
p("text direction", lambda: d.text((5, 5), "hi", font=font, direction="rtl"))
p("text features", lambda: d.text((5, 5), "hi", font=font, features=["kern"]))
p("text language", lambda: d.text((5, 5), "hi", font=font, language="en"))
p("textlength direction", lambda: d.textlength("hi", font, direction="rtl"))
p("textbbox direction", lambda: d.textbbox((0, 0), "hi", font, direction="rtl"))
p("multiline direction", lambda: d.multiline_text((5, 5), "a\nb", font=font, direction="rtl"))
p("text bitmap direction", lambda: d.text((5, 5), "hi", font=bmp, direction="rtl"))
p("textlength bitmap direction", lambda: d.textlength("hi", bmp, direction="rtl"))
p("textbbox bitmap direction", lambda: d.textbbox((0, 0), "hi", bmp, direction="rtl"))

# --- embedded_color
p("text embedded L mode", lambda: ImageDraw.Draw(Image.new("L", (20, 20))).text((2, 2), "hi", embedded_color=True))
p("text embedded P mode", lambda: ImageDraw.Draw(Image.new("P", (20, 20))).text((2, 2), "hi", embedded_color=True))
p("textlength embedded L", lambda: ImageDraw.Draw(Image.new("L", (20, 20))).textlength("hi", embedded_color=True))
p("textbbox embedded L", lambda: ImageDraw.Draw(Image.new("L", (20, 20))).textbbox((0, 0), "hi", embedded_color=True))
p("multiline embedded L", lambda: ImageDraw.Draw(Image.new("L", (20, 20))).multiline_text((2, 2), "a\nb", embedded_color=True))
p("text embedded RGB ok", lambda: (d.text((5, 5), "hi", embedded_color=True), im.getpixel((5, 5)))[1])
p("text embedded RGBA ok", lambda: (lambda i: (ImageDraw.Draw(i).text((5, 5), "hi", embedded_color=True), i.getpixel((5, 5)))[1])(Image.new("RGBA", (60, 40))))

# --- spacing
p("mlbbox spacing float 6.5 ttf", lambda: d.multiline_textbbox((0, 0), "aaa\nbbb", font=font, spacing=6.5))
p("mlbbox spacing float 6.5 bitmap", lambda: d.multiline_textbbox((0, 0), "aaa\nbbb", font=bmp, spacing=6.5))
p("ml spacing str bitmap", lambda: d.multiline_text((5, 5), "a\nb", font=bmp, spacing="x"))
p("ml spacing str ttf", lambda: d.multiline_text((5, 5), "a\nb", font=font, spacing="x"))
p("ml spacing None ttf", lambda: d.multiline_text((5, 5), "a\nb", font=font, spacing=None))
p("ml spacing int 0", lambda: d.multiline_textbbox((0, 0), "a\nb", font=font, spacing=0))
p("ml spacing int -2", lambda: d.multiline_textbbox((0, 0), "a\nb", font=font, spacing=-2))
p("mlbbox spacing int 8", lambda: d.multiline_textbbox((0, 0), "a\nb", font=font, spacing=8))

# --- align
for a in ("left", "center", "right", "justify"):
    p(f"mlbbox align={a}", lambda a=a: d.multiline_textbbox((0, 0), "aaaa\nbb\ncccccc", font=font, align=a))
p("ml align bogus", lambda: d.multiline_text((5, 5), "a\nb", font=font, align="bogus"))
p("mlbbox single line align=center", lambda: d.multiline_textbbox((0, 0), "aaaa", font=font, align="center"))
p("text newline uses spacing/align", lambda: (lambda i: (ImageDraw.Draw(i).text((0, 0), "aaaa\nbb", font=font, align="center"), i.getbbox())[1])(Image.new("RGB", (100, 60))))

# --- anchor
p("mlbbox anchor ma", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, anchor="ma"))
p("mlbbox anchor mm", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, anchor="mm"))
p("mlbbox anchor md", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, anchor="md"))
p("ml anchor mb", lambda: d.multiline_text((10, 20), "a\nb", font=font, anchor="mb"))
p("ml anchor single", lambda: d.multiline_text((10, 20), "a\nb", font=font, anchor="a"))
p("mlbbox anchor ttb", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, direction="ttb"))
p("mlbbox anchor ttb mb", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, direction="ttb", anchor="mb"))
p("mlbbox anchor ttb rm", lambda: d.multiline_textbbox((10, 20), "a\nb", font=font, direction="ttb", anchor="rm"))

# --- font_size
p("load_default(24).size", lambda: ImageFont.load_default(24).size)
p("textlength font_size=24", lambda: d.textlength("hi", font_size=24))
p("textbbox font_size=24", lambda: d.textbbox((0, 0), "hi", font_size=24))
p("mlbbox font_size=24", lambda: d.multiline_textbbox((0, 0), "a\nb", font_size=24))
p("text font_size draws", lambda: (d.text((5, 5), "hi", font_size=24), None)[1])
p("textlength multiline", lambda: d.textlength("a\nb", font))
p("textbbox multiline", lambda: d.textbbox((0, 0), "a\nb", font))

# --- getdraw / ImageDraw2
p("getdraw returns", lambda: (lambda g: (g[0].__class__.__name__, g[1].__name__))(ImageDraw.getdraw(im)))
p("getdraw none", lambda: (lambda g: (g[0], g[1].__name__))(ImageDraw.getdraw()))
p("getdraw hints deprecated", lambda: (lambda g: g[0].__class__.__name__)(ImageDraw.getdraw(im, ["hint"])))
p("Pen attrs", lambda: (ImageDraw2.Pen((1, 2, 3), 4).color, ImageDraw2.Pen((1, 2, 3), 4).width, ImageDraw2.Pen((1, 2, 3), 4, 128).opacity))
p("Brush attrs", lambda: (ImageDraw2.Brush((4, 5, 6)).color, ImageDraw2.Brush((4, 5, 6), 200).opacity))
p("Font attrs", lambda: (ImageDraw2.Font((255, 0, 0), ARIAL, 16).color, ImageDraw2.Font((255, 0, 0), ARIAL, 16).font.size))
p("Draw2 rect", lambda: (lambda i: (ImageDraw2.Draw(i).rectangle([1, 1, 9, 9], ImageDraw2.Pen((255, 0, 0), 1), ImageDraw2.Brush((0, 255, 0))), i.getpixel((5, 5)), i.getpixel((1, 1)))[1:])(Image.new("RGB", (12, 12))))
p("Draw2 line width", lambda: (lambda i: (ImageDraw2.Draw(i).line([(1, 1), (9, 9)], ImageDraw2.Pen((0, 0, 255), 2)), i.getpixel((1, 1)), i.getpixel((5, 5)))[1:])(Image.new("RGB", (12, 12))))
p("Draw2 text", lambda: (lambda i: (ImageDraw2.Draw(i).text((2, 2), "hi", ImageDraw2.Font((255, 0, 0), ARIAL, 16)), i.getbbox())[1])(Image.new("RGB", (80, 40))))
p("Draw2 flush", lambda: ImageDraw2.Draw(Image.new("RGB", (4, 4))).flush().size)
p("Draw2 transform rect", lambda: (lambda i: (ImageDraw2.Draw(i).settransform((3, 5)), ImageDraw2.Draw(i).rectangle([0, 0, 2, 2], None, ImageDraw2.Brush((255, 0, 0))), i.getpixel((3, 5)))[2])(Image.new("RGB", (10, 10))))
p("Draw2 str image", lambda: ImageDraw2.Draw("RGB", [4, 4], (0, 0, 0)).flush().size)
p("Draw2 str missing size", lambda: ImageDraw2.Draw("RGB"))
p("Draw2 textbbox", lambda: ImageDraw2.Draw(Image.new("RGB", (80, 40))).textbbox((0, 0), "hi", ImageDraw2.Font((0, 0, 0), ARIAL, 16)))
p("Draw2 textlength", lambda: ImageDraw2.Draw(Image.new("RGB", (80, 40))).textlength("hi", ImageDraw2.Font((0, 0, 0), ARIAL, 16)))

# --- ImageStat.Global
p("Global is Stat", lambda: ImageStat.Global is ImageStat.Stat)
p("Global sum", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).sum)
p("Global extrema", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).extrema)
p("Global count", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).count)
p("Global mean", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).mean)
p("Global median", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).median)
p("Global rms", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).rms)
p("Global var", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).var)
p("Global stddev", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).stddev)
p("Global sum2", lambda: ImageStat.Global(Image.new("L", (4, 4), 100)).sum2)

# --- ImageFilter base classes
p("Filter()", lambda: ImageFilter.Filter())
p("MultibandFilter()", lambda: ImageFilter.MultibandFilter())
p("BuiltinFilter()", lambda: ImageFilter.BuiltinFilter())
p("BuiltinFilter().name", lambda: ImageFilter.BuiltinFilter().name)
p("BuiltinFilter().filterargs", lambda: ImageFilter.BuiltinFilter().filterargs)
p("BuiltinFilter().filter", lambda: ImageFilter.BuiltinFilter().filter(Image.new("L", (2, 2))))
k = ImageFilter.Kernel((3, 3), list(range(9)))
p("Kernel.filterargs", lambda: k.filterargs)
p("Kernel.name", lambda: k.name)
p("Kernel(3, [1]*9)", lambda: ImageFilter.Kernel(3, [1] * 9))
p("Kernel((3,3), (1,2))", lambda: ImageFilter.Kernel((3, 3), (1, 2)))
p("Kernel((3,3)).filter mode 1", lambda: k.filter(Image.new("1", (4, 4))))
p("Kernel((3,3)).filter P", lambda: k.filter(Image.new("P", (4, 4))))
p("RankFilter(4,0).filter", lambda: ImageFilter.RankFilter(4, 0).filter(Image.new("L", (4, 4))))
p("RankFilter(0,0).filter", lambda: ImageFilter.RankFilter(0, 0).filter(Image.new("L", (4, 4))))
p("MinFilter(3).filter P", lambda: ImageFilter.MinFilter(3).filter(Image.new("P", (4, 4))))
p("ModeFilter(3).filter P", lambda: ImageFilter.ModeFilter(3).filter(Image.new("P", (4, 4))))
p("Kernel.filter P", lambda: ImageFilter.Kernel((3, 3), [1] * 9).filter(Image.new("P", (4, 4))))
p("BLUR.filterargs", lambda: ImageFilter.BLUR.filterargs)
p("BLUR.name", lambda: ImageFilter.BLUR.name)
p("issubclass kernel builtin", lambda: issubclass(ImageFilter.Kernel, ImageFilter.BuiltinFilter))
p("issubclass rank filter", lambda: issubclass(ImageFilter.RankFilter, ImageFilter.Filter))
p("issubclass min rank", lambda: issubclass(ImageFilter.MinFilter, ImageFilter.RankFilter))
p("issubclass blur builtin", lambda: issubclass(ImageFilter.BLUR, ImageFilter.BuiltinFilter))
p("issubclass gaussian multiband", lambda: issubclass(ImageFilter.GaussianBlur, ImageFilter.MultibandFilter))
p("GaussianBlur(0) copy", lambda: ImageFilter.GaussianBlur(0).filter(im).getpixel((0, 0)))
p("BoxBlur((1,2)).radius", lambda: ImageFilter.BoxBlur((1, 2)).radius)
p("GaussianBlur((1,2)).radius", lambda: ImageFilter.GaussianBlur((1, 2)).radius)
p("BoxBlur(2).radius", lambda: ImageFilter.BoxBlur(2).radius)
p("GaussianBlur(2).radius", lambda: ImageFilter.GaussianBlur(2).radius)
p("BoxBlur((1,-2))", lambda: ImageFilter.BoxBlur((1, -2)))
p("BoxBlur('x')", lambda: ImageFilter.BoxBlur("x"))
p("BoxBlur((1,'x'))", lambda: ImageFilter.BoxBlur((1, "x")))
p("GaussianBlur('x')", lambda: ImageFilter.GaussianBlur("x").filter(im))
p("GaussianBlur((1,'x'))", lambda: ImageFilter.GaussianBlur((1, "x")).filter(im))
p("UnsharpMask('x')", lambda: ImageFilter.UnsharpMask("x").filter(im))
p("ModeFilter(2).filter", lambda: ImageFilter.ModeFilter(2).filter(Image.new("L", (4, 4))))
p("MedianFilter('3')", lambda: ImageFilter.MedianFilter("3").filter(Image.new("L", (4, 4))))
p("MinFilter('3')", lambda: ImageFilter.MinFilter("3").filter(Image.new("L", (4, 4))))
p("MinFilter(3.5)", lambda: ImageFilter.MinFilter(3.5).filter(Image.new("L", (4, 4))))
p("Kernel str size", lambda: ImageFilter.Kernel("33", [1] * 9).filter(Image.new("L", (4, 4))))
p("Kernel float kernel", lambda: ImageFilter.Kernel((3, 3), [1.0] * 9).filter(Image.new("L", (4, 4))))

# --- ImageMath
p("imagemath_int", lambda: ImageMath.imagemath_int(ImageMath._Operand(im)).im.mode)
p("imagemath_float", lambda: ImageMath.imagemath_float(ImageMath._Operand(im)).im.mode)
p("imagemath_equal", lambda: ImageMath.imagemath_equal(ImageMath._Operand(im), 5).im.mode)
p("imagemath_notequal", lambda: ImageMath.imagemath_notequal(ImageMath._Operand(im), 5).im.mode)
p("imagemath_min", lambda: ImageMath.imagemath_min(ImageMath._Operand(im), 5).im.mode)
p("imagemath_max", lambda: ImageMath.imagemath_max(ImageMath._Operand(im), 5).im.mode)
p("imagemath_convert", lambda: ImageMath.imagemath_convert(ImageMath._Operand(im), "L").im.mode)
p("lambda_eval scalar", lambda: ImageMath.lambda_eval(lambda a: 5))
p("lambda_eval add", lambda: ImageMath.lambda_eval(lambda a: a["x"] + 1, x=Image.new("L", (2, 2), 10)).getpixel((0, 0)))
p("lambda_eval float", lambda: ImageMath.lambda_eval(lambda a: a["float"](a["x"]), x=Image.new("L", (2, 2), 10)).mode)
p("lambda_eval convert", lambda: ImageMath.lambda_eval(lambda a: a["convert"](a["x"], "L"), x=im).mode)
p("lambda_eval options", lambda: ImageMath.lambda_eval(lambda a: a["x"].im, {"x": Image.new("L", (2, 2), 3)}))
p("lambda_eval ops keys", lambda: sorted(ImageMath.lambda_eval(lambda a: list(a.keys()), x=1)))
p("lambda_eval equal", lambda: ImageMath.lambda_eval(lambda a: a["equal"](a["x"], 3), x=Image.new("L", (2, 2), 3)).getpixel((0, 0)))
