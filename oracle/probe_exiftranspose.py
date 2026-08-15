"""Oracle pin: Pillow 11.3.0 exif_transpose EXIF tag-stripping behavior."""
import warnings
warnings.simplefilter("ignore")
import numpy as np
from PIL import Image, ImageOps, ExifTags

def make_jpeg(path, orientation, extra_tag=False):
    im = Image.new("RGB", (4, 2))
    pixels = np.array([[[1, 2, 3], [4, 5, 6], [7, 8, 9], [10, 11, 12]],
                       [[13, 14, 15], [16, 17, 18], [19, 20, 21], [22, 23, 24]]],
                      dtype=np.uint8)
    im = Image.fromarray(pixels)
    exif = Image.Exif()
    exif[ExifTags.Base.Orientation] = orientation
    if extra_tag:
        exif[ExifTags.Base.Make] = "TestMake"
    im.save(path, "JPEG", exif=exif.tobytes())

import os, tempfile
tmp = tempfile.mkdtemp()

# case 1: orientation only -> exif removed entirely
p1 = os.path.join(tmp, "o6_only.jpg")
make_jpeg(p1, 6)
im = Image.open(p1)
out = ImageOps.exif_transpose(im)
print("o6-only copy:", out.size, out.mode, "info_keys:", sorted(im.info.keys()),
      "out_info_keys:", sorted(out.info.keys()),
      "getexif_orient:", out.getexif().get(ExifTags.Base.Orientation, "MISSING"))

# case 2: orientation + Make -> exif kept, orientation deleted
p2 = os.path.join(tmp, "o6_make.jpg")
make_jpeg(p2, 6, extra_tag=True)
im = Image.open(p2)
out = ImageOps.exif_transpose(im)
ex = out.getexif()
print("o6+make copy:", out.size, "out_info_keys:", sorted(out.info.keys()),
      "getexif_orient:", ex.get(ExifTags.Base.Orientation, "MISSING"),
      "make:", ex.get(ExifTags.Base.Make, "MISSING"))

# case 3: in_place=True orientation only
im = Image.open(p1)
ImageOps.exif_transpose(im, in_place=True)
print("o6-only inplace:", im.size, "info_keys:", sorted(im.info.keys()),
      "getexif_orient:", im.getexif().get(ExifTags.Base.Orientation, "MISSING"))

# case 4: orientation 1 (no transpose needed)
p3 = os.path.join(tmp, "o1.jpg")
make_jpeg(p3, 1, extra_tag=True)
im = Image.open(p3)
out = ImageOps.exif_transpose(im)
ex = out.getexif()
print("o1+make copy:", out.size, "out_info_keys:", sorted(out.info.keys()),
      "getexif_orient:", ex.get(ExifTags.Base.Orientation, "MISSING"),
      "make:", ex.get(ExifTags.Base.Make, "MISSING"))

# case 5: in_place orientation 1 + Make
im = Image.open(p3)
ImageOps.exif_transpose(im, in_place=True)
print("o1+make inplace:", im.size, "info_keys:", sorted(im.info.keys()),
      "orient:", im.getexif().get(ExifTags.Base.Orientation, "MISSING"))

# case 6: no EXIF at all
p4 = os.path.join(tmp, "noexif.png")
Image.new("RGB", (2, 2), "red").save(p4, "PNG")
im = Image.open(p4)
out = ImageOps.exif_transpose(im)
print("noexif copy:", out.size, "info_keys:", sorted(out.info.keys()))
