"""One-shot oracle probe for FMT-TIFF-003BD.

Saves numeric modes with Pillow 11.3.0 big_tiff=True and dumps headers
plus reopen behavior.
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    cases = [
        ("I16", Image.new("I;16", (2, 2), 513)),
        ("I", Image.new("I", (2, 2), 7)),
        ("F", Image.new("F", (2, 2), 1.5)),
        ("CMYK", Image.new("CMYK", (2, 2), (10, 20, 30, 40))),
    ]
    for name, im in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-num-%s.tif" % name)
        try:
            im.save(path, "TIFF", big_tiff=True)
            with open(path, "rb") as handle:
                data = handle.read()
            print("case:", name, "size:", len(data))
            for index in range(0, min(len(data), 240), 16):
                print("%04d  %s" % (index, data[index:index + 16].hex(" ")))
            with Image.open(path) as reopened:
                print("reopen:", reopened.mode, reopened.size, list(reopened.tobytes())[:8])
        except Exception as err:
            print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
