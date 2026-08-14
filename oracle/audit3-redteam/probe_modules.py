import PIL.Image, PIL.ImageOps, PIL.ImageDraw, PIL.ImageFilter, PIL.ImageFont, PIL.ImageEnhance, PIL.ImageStat, PIL.ImageSequence, PIL.ImageChops, PIL.ImageCms, PIL.ImageMath, PIL.ImageGrab, PIL.ImagePalette, PIL.ImageFile, PIL.ImagePath
try:
    import PIL.ImageQt
except Exception as e:
    print("ImageQt import err", e)
try:
    import PIL.ImageTk
except Exception as e:
    print("ImageTk import err", e)
import PIL.ImageTransform

mods = {
    "ImageOps": PIL.ImageOps,
    "ImageDraw": PIL.ImageDraw,
    "ImageFilter": PIL.ImageFilter,
    "ImageFont": PIL.ImageFont,
    "ImageEnhance": PIL.ImageEnhance,
    "ImageStat": PIL.ImageStat,
    "ImageSequence": PIL.ImageSequence,
    "ImageChops": PIL.ImageChops,
    "ImageCms": PIL.ImageCms,
    "ImageMath": PIL.ImageMath,
    "ImageGrab": PIL.ImageGrab,
    "ImagePalette": PIL.ImagePalette,
    "ImageFile": PIL.ImageFile,
    "ImagePath": PIL.ImagePath,
    "ImageTransform": PIL.ImageTransform,
}
for name, m in mods.items():
    names = [n for n in dir(m) if not n.startswith("_")]
    print(f"=== {name} ===")
    print("   " + ", ".join(names))
