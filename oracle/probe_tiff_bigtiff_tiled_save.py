"""Probe Pillow 11.3.0 BigTIFF save with tile=(2,2) — the tiled layout the
native open route consumes."""

import os
import tempfile

from PIL import Image


def main():
    cases = [
        ("L", Image.new("L", (2, 2), 7)),
        ("RGB", Image.new("RGB", (3, 2), (10, 20, 30))),
        ("RGBA", Image.new("RGBA", (2, 3), (10, 20, 30, 40))),
        ("LA", Image.new("LA", (2, 2), (5, 6))),
    ]
    for name, im in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-tiled-%s.tif" % name)
        try:
            im.save(path, "TIFF", big_tiff=True, tile=(2, 2))
            with open(path, "rb") as handle:
                data = handle.read()
            print("case:", name, "size:", len(data))
            for index in range(0, min(len(data), 96), 16):
                print("%04d  %s" % (index, data[index:index + 16].hex(" ")))
            with Image.open(path) as reopened:
                print("reopen:", reopened.mode, reopened.size, list(reopened.tobytes())[:12])
        except Exception as err:
            print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
