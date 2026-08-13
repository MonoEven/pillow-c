"""Probe whether Pillow 11.3.0 classic save_all is chained or concatenated."""

import os
import tempfile

from PIL import Image


def main():
    frame0 = Image.new("L", (2, 2), 7)
    frame1 = Image.new("L", (2, 2), 9)
    path = os.path.join(tempfile.gettempdir(), "probe-classic-two-frame.tif")
    try:
        frame0.save(path, "TIFF", save_all=True, append_images=[frame1])
        with open(path, "rb") as handle:
            data = handle.read()
        print("size:", len(data))
        for index in range(0, len(data), 16):
            print("%04d  %s" % (index, data[index:index + 16].hex(" ")))
        with Image.open(path) as im:
            print("n_frames:", im.n_frames)
            im.seek(1)
            print("frame1:", list(im.tobytes()))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
