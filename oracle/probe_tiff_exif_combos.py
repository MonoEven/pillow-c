"""One-shot oracle probe for FMT-TIFF-003AX.

Saves a 2x2 mode-L image as classic TIFF with Pillow 11.3.0 combining
exif= with dpi=, icc_profile=, and tiffinfo= options, then reopens each
result to print the combined behavior.
"""

import os
import tempfile

from PIL import Image


def main():
    im = Image.new("L", (2, 2))
    im.putdata([1, 2, 3, 4])
    exif = Image.Exif()
    exif[270] = "Description Text"
    exif[33434] = 0.008
    exif[37377] = -3.0

    icc = bytes(range(1, 17))

    cases = [
        ("exif-dpi", {"exif": exif, "dpi": (300.0, 150.0)}),
        ("exif-icc", {"exif": exif, "icc_profile": icc}),
        ("exif-tiffinfo", {"exif": exif, "tiffinfo": {315: "Ada Lovelace"}}),
        ("exif-dpi-icc-tiffinfo", {"exif": exif, "dpi": (300.0, 150.0), "icc_profile": icc, "tiffinfo": {315: "Ada Lovelace"}}),
        ("exif-tiffinfo-270-collision", {"exif": exif, "tiffinfo": {270: "From Tiffinfo"}}),
    ]
    for name, options in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-tiff-combo-%s.tif" % name)
        try:
            im.save(path, "TIFF", **options)
            with Image.open(path) as reopened:
                flat = reopened.getexif()
                print("case:", name)
                print("  size:", reopened.size, "bytes:", list(reopened.tobytes()))
                print("  info dpi:", reopened.info.get("dpi"))
                print("  icc:", None if reopened.info.get("icc_profile") is None else len(reopened.info["icc_profile"]))
                print("  exif[270]:", repr(flat.get(270)))
                print("  exif[315]:", repr(flat.get(315)))
                print("  exif[33434]:", repr(flat.get(33434)))
                print("  exif[37377]:", repr(flat.get(37377)))
                print("  exif[282]:", repr(flat.get(282)))
                print("  exif[296]:", repr(flat.get(296)))
        except Exception as err:
            print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
