"""One-shot oracle probe for FMT-ICO-001C.

Mixed-mode sources, multiple same-size PNG sources, and bmp-format
duplicate bit-depths in Pillow 11.3.0.
"""

import os
import tempfile

from PIL import Image


def main():
    # Mixed modes: base RGB + appended L and RGBA of distinct sizes
    path = os.path.join(tempfile.gettempdir(), "probe-ico-mixed.ico")
    try:
        base = Image.new("RGB", (32, 32), (200, 0, 0))
        gray = Image.new("L", (16, 16), 16)
        rgba = Image.new("RGBA", (24, 24), (0, 0, 255, 255))
        base.save(path, "ICO", sizes=[(16, 16), (24, 24), (32, 32)],
                  append_images=[gray, rgba])
        with Image.open(path) as ico:
            for size in sorted(ico.ico.sizes()):
                im = ico.ico.getimage(size)
                print("mixed size", size, "mode", im.mode, "pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("mixed ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # Multiple same-size PNG sources: first wins?
    path = os.path.join(tempfile.gettempdir(), "probe-ico-samesize.ico")
    try:
        base = Image.new("RGBA", (32, 32), (255, 0, 0, 255))
        first = Image.new("RGBA", (16, 16), (0, 255, 0, 255))
        second = Image.new("RGBA", (16, 16), (0, 0, 255, 255))
        base.save(path, "ICO", sizes=[(16, 16), (32, 32)], append_images=[first, second])
        with Image.open(path) as ico:
            for size in sorted(ico.ico.sizes()):
                im = ico.ico.getimage(size)
                print("samesize size", size, "pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("samesize ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # bmp format with two same-size sources of distinct bit depths
    path = os.path.join(tempfile.gettempdir(), "probe-ico-bmp-dup.ico")
    try:
        base = Image.new("RGBA", (32, 32), (255, 0, 0, 255))
        high = Image.new("RGBA", (16, 16), (0, 255, 0, 255))
        low = Image.new("RGB", (16, 16), (0, 0, 255))
        base.save(path, "ICO", sizes=[(16, 16), (32, 32)], bitmap_format="bmp",
                  append_images=[high, low])
        with Image.open(path) as ico:
            print("bmp-dup sizes:", ico.ico.sizes())
            for size in sorted(ico.ico.sizes()):
                im = ico.ico.getimage(size)
                print("  size", size, "mode", im.mode, "pixel", im.getpixel((0, 0)))
    except Exception as err:
        print("bmp-dup ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
