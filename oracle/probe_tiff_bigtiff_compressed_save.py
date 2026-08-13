"""One-shot oracle probe for FMT-TIFF-003BA.

Saves with Pillow 11.3.0 using big_tiff=True plus compression, dumping
headers and reopen behavior.
"""

import os
import tempfile

from PIL import Image


def main():
    cases = [
        ("L", Image.new("L", (2, 2), 7)),
        ("RGB", Image.new("RGB", (3, 2), (10, 20, 30))),
    ]
    for name, im in cases:
        for compression in ("packbits", "tiff_lzw", "tiff_adobe_deflate"):
            path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-cmp-%s-%s.tif" % (name, compression))
            try:
                im.save(path, "TIFF", big_tiff=True, compression=compression)
                with open(path, "rb") as handle:
                    data = handle.read()
                print("case:", name, compression, "size:", len(data))
                for index in range(0, min(len(data), 64), 16):
                    print("%04d  %s" % (index, data[index:index + 16].hex(" ")))
                with Image.open(path) as reopened:
                    print("reopen:", reopened.mode, reopened.size, list(reopened.tobytes())[:12])
            except Exception as err:
                print("case:", name, compression, "ERR", type(err).__name__, str(err))
            finally:
                try:
                    os.remove(path)
                except OSError:
                    pass


if __name__ == "__main__":
    main()
