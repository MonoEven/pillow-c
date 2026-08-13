"""One-shot oracle probe for FMT-TIFF-003BG.

Pillow 11.3.0 save_all=True + big_tiff=True with numeric modes and with
metadata families; dumps layout and reopen behavior per frame.
"""

import os
import struct
import tempfile

from PIL import Image


def main():
    icc = bytes(range(20))
    xmp = b"<x:xmpmeta>probe</x:xmpmeta>"

    # 1. Numeric save_all: I;16 two frames
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-sa-i16.tif")
    try:
        first = Image.new("I;16", (2, 2), 513)
        second = Image.new("I;16", (2, 2), 7)
        first.save(path, "TIFF", big_tiff=True, save_all=True, append_images=[second])
        data = open(path, "rb").read()
        print("i16 save_all size:", len(data), "bigtiff:", data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
        with Image.open(path) as reopened:
            print("  n_frames:", getattr(reopened, "n_frames", 1))
            for index in range(getattr(reopened, "n_frames", 1)):
                reopened.seek(index)
                print("  frame", index, reopened.mode, list(reopened.tobytes())[:8])
    except Exception as err:
        print("i16 save_all ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # 2. Metadata save_all: L two frames with dpi + icc + tiffinfo
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-sa-meta.tif")
    try:
        first = Image.new("L", (2, 2), 7)
        second = Image.new("L", (2, 2), 9)
        first.save(path, "TIFF", big_tiff=True, save_all=True, append_images=[second],
                   dpi=(300.0, 150.0), icc_profile=icc, tiffinfo={270: "desc-probe"})
        data = open(path, "rb").read()
        print("meta save_all size:", len(data), "bigtiff:", data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43)
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            print("  n_frames:", n)
            for index in range(n):
                reopened.seek(index)
                info = {k: (v[:12] if isinstance(v, bytes) else v) for k, v in reopened.info.items()}
                print("  frame", index, "info:", info, "exif keys:", sorted(reopened.getexif().keys()))
    except Exception as err:
        print("meta save_all ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    # 3. Numeric save_all with compression (expect classic fallback)
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-sa-i16-cmp.tif")
    try:
        first = Image.new("I;16", (2, 2), 513)
        second = Image.new("I;16", (2, 2), 7)
        first.save(path, "TIFF", big_tiff=True, save_all=True, append_images=[second],
                   compression="lzw")
        data = open(path, "rb").read()
        print("i16 save_all lzw kind:", "bigtiff" if (data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43) else "classic")
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            print("  n_frames:", n)
            reopened.seek(1)
            print("  frame1 bytes:", list(reopened.tobytes())[:8])
    except Exception as err:
        print("i16 save_all lzw ERR", type(err).__name__, str(err))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
