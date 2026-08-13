"""One-shot oracle probe for FMT-TIFF-003BB.

Saves two-frame TIFFs with Pillow 11.3.0 using big_tiff=True plus
save_all, dumping headers and reopen behavior.
"""

import os
import tempfile

from PIL import Image


def main():
    frame0 = Image.new("L", (2, 2), 7)
    frame1 = Image.new("L", (2, 2), 9)
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-two-frame.tif")
    try:
        frame0.save(path, "TIFF", save_all=True, append_images=[frame1], big_tiff=True)
        with open(path, "rb") as handle:
            data = handle.read()
        print("size:", len(data))
        for index in range(0, min(len(data), 448), 16):
            print("%04d  %s" % (index, data[index:index + 16].hex(" ")))
        with Image.open(path) as im:
            print("n_frames:", im.n_frames)
            print("frame0:", im.mode, list(im.tobytes()))
            im.seek(1)
            print("frame1:", im.mode, list(im.tobytes()))
    except Exception as err:
        print("ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
