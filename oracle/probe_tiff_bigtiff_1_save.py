"""One-shot oracle probe for FMT-TIFF-003BI.

Saves a mode-1 image with Pillow 11.3.0 big_tiff=True and dumps the
layout plus reopen behavior (raw and packbits).
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    for compression, name in [("raw", "raw"), ("packbits", "packbits")]:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-1-%s.tif" % name)
        try:
            im = Image.new("1", (9, 2), 0)
            im.putdata([1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1])
            kwargs = {"big_tiff": True}
            if compression != "raw":
                kwargs["compression"] = compression
            im.save(path, "TIFF", **kwargs)
            data = open(path, "rb").read()
            print("case:", name, "size:", len(data), "bigtiff:",
                  data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
            for i in range(0, min(len(data), 260), 16):
                print("%04d  %s" % (i, data[i:i + 16].hex(" ")))
            with Image.open(path) as reopened:
                print("reopen:", reopened.mode, reopened.size, list(reopened.tobytes()))
        except Exception as err:
            print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
