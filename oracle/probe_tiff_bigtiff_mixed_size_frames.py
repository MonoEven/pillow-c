"""One-shot oracle probe for FMT-TIFF-003BJ.

Pillow 11.3.0 save_all + big_tiff with same-mode but different-size
frames; dumps layout and reopen behavior per frame.
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-mixed.tif")
    try:
        first = Image.new("L", (2, 2), 7)
        second = Image.new("L", (3, 1), 9)
        first.save(path, "TIFF", big_tiff=True, save_all=True, append_images=[second])
        data = open(path, "rb").read()
        print("size:", len(data), "bigtiff:",
              data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            print("n_frames:", n)
            for index in range(n):
                reopened.seek(index)
                print("frame", index, "mode:", reopened.mode, "size:", reopened.size,
                      "bytes:", list(reopened.tobytes()))
    except Exception as err:
        print("ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
