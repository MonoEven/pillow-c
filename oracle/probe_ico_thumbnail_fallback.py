"""FMT-ICO-001B edge probes: aspect-ratio thumbnail fallback and
smaller-than-size sources in Pillow 11.3.0.
"""

import os
import tempfile

from PIL import Image


def pattern(im, value):
    im.putdata([value] * (im.width * im.height))
    return im


def main():
    # Non-square last source: thumbnail preserves aspect
    path = os.path.join(tempfile.gettempdir(), "probe-ico-aspect.ico")
    try:
        base = pattern(Image.new("L", (64, 64)), 64)
        wide = pattern(Image.new("L", (64, 32)), 32)
        base.save(path, "ICO", sizes=[(16, 16), (32, 32), (64, 64)],
                  append_images=[wide])
        with Image.open(path) as ico:
            print("aspect ico sizes:", ico.ico.sizes())
            for size in sorted(ico.ico.sizes()):
                im = ico.ico.getimage(size)
                print("  size", size, "actual", im.size, "first pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("aspect ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # Last source smaller than the requested size: thumbnail keeps it small
    path = os.path.join(tempfile.gettempdir(), "probe-ico-small-last.ico")
    try:
        base = pattern(Image.new("L", (64, 64)), 64)
        tiny = pattern(Image.new("L", (24, 24)), 24)
        base.save(path, "ICO", sizes=[(16, 16), (32, 32), (64, 64)],
                  append_images=[tiny])
        with Image.open(path) as ico:
            print("small-last ico sizes:", ico.ico.sizes())
            for size in sorted(ico.ico.sizes()):
                im = ico.ico.getimage(size)
                print("  size", size, "actual", im.size, "first pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("small-last ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
