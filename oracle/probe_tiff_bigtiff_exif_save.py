"""One-shot oracle probe for FMT-TIFF-003BF.

Saves an L image with Pillow 11.3.0 big_tiff=True plus an exif object
covering the bounded families, and dumps the BigTIFF IFD0 layout.
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-exif.tif")
    try:
        exif = Image.Exif()
        exif[270] = "desc-probe"          # ascii, 11 bytes incl NUL -> out-of-line
        exif[315] = "artist"              # ascii, 7 bytes -> inline
        exif[317] = 3                     # uint SHORT
        exif[296] = 2                     # uint SHORT (resolution unit, no dpi trio)
        exif[256] = 2                     # uint LONG (overlaps base 256! skip - probe collision)
        exif.pop(256, None)
        exif[33434] = (1, 2)              # rational
        exif[318] = (1, 2, 3, 4)            # rational array (flat pairs, out-of-line)
        exif[34735] = [1, 2, 3]           # short array (count 3, 6 bytes <= 8 -> inline)
        exif[36864] = b"0230"             # undefined
        exif[40091] = b"ASCII\0\0\0"      # undefined (XPComment-like)
        exif[37380] = (3, 7)              # signed rational
        exif[50719] = [1, 2, 3]           # uint LONG array (12 bytes -> out-of-line)

        im = Image.new("L", (2, 2), 7)
        im.save(path, "TIFF", big_tiff=True, exif=exif)
        data = open(path, "rb").read()
        print("size:", len(data))
        for i in range(0, len(data), 16):
            print("%04d  %s" % (i, data[i:i + 16].hex(" ")))
        with Image.open(path) as reopened:
            print("reopen exif keys:", sorted(reopened.getexif().keys()))
            got = reopened.getexif()
            for tag in [270, 315, 317, 296, 33434, 318, 34735, 36864, 40091, 37380, 50719]:
                print("  exif[%d] = %r" % (tag, got.get(tag)))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
