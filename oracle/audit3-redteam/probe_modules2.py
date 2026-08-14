import inspect
from PIL import Image, ImageDraw, ImageFont, ImageStat, ImageCms, ImageMath, ImageFilter

# ImageDraw.text signature
print("Draw.text sig:", inspect.signature(ImageDraw.ImageDraw.text))
print("Draw.multiline_text sig:", inspect.signature(ImageDraw.ImageDraw.multiline_text))
print("Draw.multiline_textbbox sig:", inspect.signature(ImageDraw.ImageDraw.multiline_textbbox))
print("ImageDraw.getdraw:", hasattr(ImageDraw, "getdraw"))
print("ImageDraw.floodfill:", hasattr(ImageDraw, "floodfill"))

# ImageFont
print("ImageFont.truetype:", hasattr(ImageFont, "truetype"))
print("ImageFont.load:", hasattr(ImageFont, "load"))
print("ImageFont.load_path:", hasattr(ImageFont, "load_path"))
print("ImageFont.load_default_imagefont:", hasattr(ImageFont, "load_default_imagefont"))
print("ImageFont.features:", ImageFont.features)
print("ImageFont.MAX_STRING_LENGTH:", ImageFont.MAX_STRING_LENGTH)

# ImageStat.Global
print("ImageStat.Global:", hasattr(ImageStat, "Global"), type(getattr(ImageStat, "Global", None)))

# ImageMath
print("ImageMath.lambda_eval:", hasattr(ImageMath, "lambda_eval"))
print("ImageMath.imagemath_int:", hasattr(ImageMath, "imagemath_int"))

# ImageFilter base classes
for n in ["Filter", "BuiltinFilter", "MultibandFilter", "Color3DLUT"]:
    print("ImageFilter." + n + ":", hasattr(ImageFilter, n))

# ImageCms constants
for n in ["Direction", "Flags", "Intent", "PyCMSError", "versions", "get_display_profile", "buildProofTransformFromOpenProfiles", "getOpenProfile", "createProfile", "buildTransform", "profileToProfile", "applyTransform", "getProfileName"]:
    print("ImageCms." + n + ":", hasattr(ImageCms, n))

# ImageDraw text with direction/language/features/embedded_color actually work?
im = Image.new("RGB", (100, 30))
d = ImageDraw.Draw(im)
font = ImageFont.load_default()
try:
    d.text((5,5), "hello", direction="rtl")
    print("text direction=rtl: OK")
except Exception as e:
    print("text direction=rtl:", type(e).__name__, e)
try:
    d.text((5,5), "hello", embedded_color=True)
    print("text embedded_color=True: OK")
except Exception as e:
    print("text embedded_color=True:", type(e).__name__, e)
try:
    d.text((5,5), "hello", features=["kern"])
    print("text features=['kern']: OK")
except Exception as e:
    print("text features:", type(e).__name__, e)
try:
    d.text((5,5), "hello", language="en")
    print("text language='en': OK")
except Exception as e:
    print("text language:", type(e).__name__, e)

# multiline text spacing/align validation
try:
    d.multiline_text((5,5), "a\nb", spacing="x")
except Exception as e:
    print("multiline spacing='x':", type(e).__name__, e)
try:
    d.multiline_text((5,5), "a\nb", align="bogus")
except Exception as e:
    print("multiline align='bogus':", type(e).__name__, e)

# ImageStat.Global usage
im2 = Image.new("L", (4,4), 100)
print("Stat.sum:", ImageStat.Stat(im2).sum)
print("Global.sum:", ImageStat.Global.sum if hasattr(ImageStat, "Global") else "N/A")
