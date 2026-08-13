"""One-shot oracle probe for FMT-TIFF-003AZ.

Saves images as BigTIFF with Pillow 11.3.0 (bigtiff=True) for the
uncompressed single-frame mode matrix and dumps the file headers plus
reopen behavior.
"""

import os
import tempfile

from PIL import Image


def hexdump(data, step=32, limit=224):
    for index in range(0, min(len(data), limit), step):
        chunk = data[index:index + step]
        print("%04d  %s" % (index, chunk.hex(" ")))


def main():
    cases = [
        ("L", Image.new("L", (2, 2), 7)),
        ("RGB", Image.new("RGB", (3, 2), (10, 20, 30))),
        ("RGBA", Image.new("RGBA", (2, 3), (10, 20, 30, 40))),
        ("LA", Image.new("LA", (2, 2), (5, 6))),
    ]
    for name, im in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-save-%s.tif" % name)
        try:
            im.save(path, "TIFF", big_tiff=True)
            with open(path, "rb") as handle:
                data = handle.read()
            print("case:", name, "size:", len(data))
            hexdump(data, 16, 160)
            with Image.open(path) as reopened:
                print("reopen mode/size:", reopened.mode, reopened.size)
                print("reopen bytes:", list(reopened.tobytes())[:16])
                print("reopen bigtiff:", reopened.tag_v2.sizeof_long == 8 if hasattr(reopened, "tag_v2") else "n/a")
        except Exception as err:
            print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
