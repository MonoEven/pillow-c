"""Probe which Pillow 11.3.0 TIFF exif= + compression combinations work."""

import os
import tempfile

from PIL import Image


def main():
    im = Image.new("L", (2, 2))
    im.putdata([1, 2, 3, 4])
    scalars = Image.Exif()
    scalars[270] = "Description Text"
    scalars[282] = 300.0
    scalars[283] = 150.0
    scalars[296] = 2
    arrays = Image.Exif()
    arrays[270] = "Description Text"
    arrays[318] = (0.5, 0.75)
    arrays[50719] = (1, 2)

    for name, exif in (("scalars", scalars), ("arrays", arrays)):
        for compression in ("raw", "packbits", "lzw", "tiff_lzw", "deflate", "tiff_adobe_deflate"):
            path = os.path.join(tempfile.gettempdir(), "probe-combo-%s-%s.tif" % (name, compression))
            try:
                im.save(path, "TIFF", exif=exif, compression=compression)
                with Image.open(path) as reopened:
                    flat = reopened.getexif()
                    print(name, compression, "OK size=", os.path.getsize(path),
                          "318=", repr(flat.get(318)), "50719=", repr(flat.get(50719)),
                          "dpi=", reopened.info.get("dpi"))
            except Exception as err:
                print(name, compression, "ERR", type(err).__name__, str(err))
            finally:
                try:
                    os.remove(path)
                except OSError:
                    pass


if __name__ == "__main__":
    main()
