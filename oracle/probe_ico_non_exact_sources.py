"""One-shot oracle probe for FMT-ICO-001B.

Pillow 11.3.0 ICO save with append_images and a sizes list where the
sources do not match the sizes exactly; identifies which source feeds
each emitted size entry.
"""

import os
import tempfile

from PIL import Image


def pattern(im, value):
    data = [value] * (im.width * im.height)
    im.putdata(data)
    return im


def main():
    path = os.path.join(tempfile.gettempdir(), "probe-ico-non-exact.ico")
    try:
        base = pattern(Image.new("L", (64, 64)), 64)
        small = pattern(Image.new("L", (16, 16)), 16)
        mid = pattern(Image.new("L", (48, 48)), 48)
        base.save(path, "ICO", sizes=[(16, 16), (32, 32), (48, 48), (64, 64)],
                  append_images=[small, mid])
        with Image.open(path) as ico:
            sizes = ico.ico.sizes()
            print("ico sizes:", sizes)
            for size in sizes:
                im = ico.ico.getimage(size)
                first = im.getpixel((0, 0))
                print("size", size, "mode", im.mode, "first pixel", first)
    except Exception as err:
        print("ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # Upscale case: sources smaller than the requested size
    path = os.path.join(tempfile.gettempdir(), "probe-ico-upscale.ico")
    try:
        base = pattern(Image.new("L", (16, 16)), 16)
        base.save(path, "ICO", sizes=[(16, 16), (32, 32), (64, 64)])
        with Image.open(path) as ico:
            print("upscale ico sizes:", ico.ico.sizes())
            for size in ico.ico.sizes():
                im = ico.ico.getimage(size)
                print("size", size, "first pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("upscale ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
