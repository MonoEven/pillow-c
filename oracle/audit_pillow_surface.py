"""Full public-surface inventory of the installed Pillow 11.3.0."""

import json
import sys

import PIL
import PIL.Image as Image

result = {"pillow_version": PIL.__version__}

# Top-level PIL names.
result["PIL"] = sorted(
    n for n in dir(PIL) if not n.startswith("_") and not n[0].islower()
)

# Image.Image non-private names.
result["Image_names"] = sorted(
    n for n in dir(Image.Image) if not n.startswith("_")
)

# Registered formats.
result["registered_extensions"] = {
    ext: name for ext, name in Image.registered_extensions().items()
}
result["SAVE"] = sorted(Image.SAVE.keys())
result["OPEN"] = sorted(Image.OPEN.keys())
result["MIME"] = sorted(Image.MIME.keys())

# Key submodules: non-private names per module.
modules = [
    "ImageChops", "ImageColor", "ImageCms", "ImageDraw", "ImageEnhance",
    "ImageFile", "ImageFilter", "ImageFont", "ImageGrab", "ImageMath",
    "ImageMode", "ImageOps", "ImagePalette", "ImagePath", "ImageQt",
    "ImageSequence", "ImageStat", "ImageTk", "ImageTransform",
]
for modname in modules:
    try:
        mod = __import__("PIL." + modname, fromlist=[modname])
        result[modname] = sorted(
            n for n in dir(mod) if not n.startswith("_")
        )
    except Exception as err:  # noqa: BLE001
        result[modname] = ["<import error: %s>" % err]

# ImagingCore (im) names.
im = Image.new("L", (1, 1))
result["ImagingCore"] = sorted(n for n in dir(im.im) if not n.startswith("_"))

with open(sys.argv[1], "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=1, default=str)

print("PIL version:", PIL.__version__)
for key in ("PIL", "Image_names", "SAVE", "OPEN", "ImagingCore"):
    print(key, len(result[key]))
for modname in modules:
    print(modname, len(result[modname]))
