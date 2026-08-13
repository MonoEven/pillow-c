"""One-shot oracle probe for FMT-TIFF-003AV.

Saves a 2x2 mode-L image as classic TIFF with Pillow 11.3.0 using the
exif= option (Image.Exif object form), dumps the resulting file bytes, and
reopens it to print getexif()/get_ifd() behavior.
"""

import os
import tempfile

from PIL import Image
from PIL.ExifTags import TAGS


def hexdump(data, step=32):
    for index in range(0, len(data), step):
        chunk = data[index:index + step]
        print("%04d  %s" % (index, chunk.hex(" ")))


def main():
    im = Image.new("L", (2, 2))
    im.putdata([1, 2, 3, 4])
    exif = Image.Exif()
    exif[270] = "Description Text"
    exif[33434] = 0.008
    exif[37377] = -3.0
    exif[282] = 300.0
    exif[283] = 150.0
    exif[296] = 2
    exif[318] = (0.5, 0.75)
    exif[50719] = (1, 2)

    path = os.path.join(tempfile.gettempdir(), "probe-tiff-exif-save.tif")
    im.save(path, "TIFF", exif=exif)

    with open(path, "rb") as handle:
        data = handle.read()
    print("file size:", len(data))
    hexdump(data[:448])

    with Image.open(path) as reopened:
        print("mode/size:", reopened.mode, reopened.size)
        print("tobytes:", list(reopened.tobytes()))
        print("info dpi:", reopened.info.get("dpi"))
        flat = reopened.getexif()
        print("exif keys:", sorted(flat.keys()))
        print("exif[34665]:", repr(flat.get(34665)))
        print("exif[270]:", repr(flat.get(270)))
        print("exif[33434]:", repr(flat.get(33434)))
        print("exif[37377]:", repr(flat.get(37377)))
        print("exif[318]:", repr(flat.get(318)))
        print("exif[50719]:", repr(flat.get(50719)))
        exif_ifd = flat.get_ifd(0x8769)
        print("exif_ifd:", None if exif_ifd is None else {k: exif_ifd.get(k) for k in exif_ifd})

    # compression composition (scalar exif only: array exif plus compression
    # fails upstream in libtiff, see probe_tiff_exif_compression.py)
    scalar_exif = Image.Exif()
    scalar_exif[270] = "Description Text"
    scalar_exif[282] = 300.0
    scalar_exif[283] = 150.0
    scalar_exif[296] = 2
    path3 = os.path.join(tempfile.gettempdir(), "probe-tiff-exif-save-lzw.tif")
    im.save(path3, "TIFF", exif=scalar_exif, compression="tiff_lzw")
    with open(path3, "rb") as handle:
        data3 = handle.read()
    print("lzw file size:", len(data3))
    with Image.open(path3) as reopened3:
        print("lzw mode/size:", reopened3.mode, reopened3.size)
        print("lzw tobytes:", list(reopened3.tobytes()))
        print("lzw info dpi:", reopened3.info.get("dpi"))
        flat3 = reopened3.getexif()
        print("lzw exif[318]:", repr(flat3.get(318)))
        print("lzw exif[50719]:", repr(flat3.get(50719)))
        print("lzw exif[270]:", repr(flat3.get(270)))

    # bytes form
    raw = scalar_exif.tobytes()
    path2 = os.path.join(tempfile.gettempdir(), "probe-tiff-exif-save-bytes.tif")
    im.save(path2, "TIFF", exif=raw)
    with open(path2, "rb") as handle:
        data2 = handle.read()
    print("bytes-form file size:", len(data2))
    with Image.open(path2) as reopened2:
        flat2 = reopened2.getexif()
        print("bytes-form exif keys:", sorted(flat2.keys()))
        print("bytes-form exif[270]:", repr(flat2.get(270)))
        print("bytes-form exif[318]:", repr(flat2.get(318)))
        print("bytes-form exif[50719]:", repr(flat2.get(50719)))
        print("bytes-form dpi:", reopened2.info.get("dpi"))

    for p in (path, path2, path3):
        try:
            os.remove(p)
        except OSError:
            pass


if __name__ == "__main__":
    main()
