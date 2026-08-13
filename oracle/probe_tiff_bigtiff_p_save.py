"""One-shot oracle probe for FMT-TIFF-003BH.

Saves a P-mode image with Pillow 11.3.0 big_tiff=True and dumps the
layout plus reopen behavior.
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-p.tif")
    try:
        im = Image.new("P", (2, 2), 0)
        im.putpalette([0, 0, 0, 255, 0, 0, 0, 255, 0, 255, 255, 255])
        im.putdata([0, 1, 2, 3])
        im.save(path, "TIFF", big_tiff=True)
        data = open(path, "rb").read()
        print("size:", len(data), "bigtiff:", data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
        for i in range(0, len(data), 16):
            print("%04d  %s" % (i, data[i:i + 16].hex(" ")))
        with Image.open(path) as reopened:
            print("reopen:", reopened.mode, reopened.size, list(reopened.tobytes()))
            print("palette:", list(reopened.getpalette()))
            print("info:", {k: v for k, v in reopened.info.items()})
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # P + dpi composition
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-p-dpi.tif")
    try:
        im = Image.new("P", (2, 2), 0)
        im.putpalette([0, 0, 0, 255, 0, 0, 0, 255, 0, 255, 255, 255])
        im.save(path, "TIFF", big_tiff=True, dpi=(300.0, 150.0))
        data = open(path, "rb").read()
        print("p+dpi size:", len(data), "bigtiff:", data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
        with Image.open(path) as reopened:
            print("reopen dpi:", reopened.info.get("dpi"), "mode:", reopened.mode,
                  "bytes:", list(reopened.tobytes()))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
