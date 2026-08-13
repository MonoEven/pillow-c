"""One-shot oracle probe for FMT-TIFF-003BE.

Saves L images with Pillow 11.3.0 big_tiff=True plus each metadata family
and dumps header/IFD0 behavior plus reopen exposure.
"""

import os
import struct
import tempfile

from PIL import Image
from PIL.TiffImagePlugin import ImageFileDirectory_v2


def tag_map(data):
    """Parse classic or BigTIFF IFD0 into {tag: (type, count, raw)}."""
    le = data[:2] == b"II"
    big = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
    if big:
        offset_size = 8
        ifd0 = struct.unpack("<Q" if le else ">Q", data[8:16])[0]
        count = struct.unpack("<Q" if le else ">Q", data[ifd0:ifd0 + 8])[0]
        entries = data[ifd0 + 8:ifd0 + 8 + count * 20]
        out = {}
        for i in range(count):
            entry = entries[i * 20:(i + 1) * 20]
            tag = struct.unpack("<H" if le else ">H", entry[0:2])[0]
            typ = struct.unpack("<H" if le else ">H", entry[2:4])[0]
            cnt = struct.unpack("<Q" if le else ">Q", entry[4:12])[0]
            val = entry[12:20]
            out[tag] = (typ, cnt, val)
        return out, "bigtiff"
    ifd0 = struct.unpack("<I" if le else ">I", data[4:8])[0]
    count = struct.unpack("<H" if le else ">H", data[ifd0:ifd0 + 2])[0]
    entries = data[ifd0 + 2:ifd0 + 2 + count * 12]
    out = {}
    for i in range(count):
        entry = entries[i * 12:(i + 1) * 12]
        tag = struct.unpack("<H" if le else ">H", entry[0:2])[0]
        typ = struct.unpack("<H" if le else ">H", entry[2:4])[0]
        cnt = struct.unpack("<I" if le else ">I", entry[4:8])[0]
        val = entry[8:12]
        out[tag] = (typ, cnt, val)
    return out, "classic"


def main():
    icc = bytes(range(32))
    xmp = b"<x:xmpmeta>probe</x:xmpmeta>"
    cases = [
        ("dpi", {"dpi": (300.0, 150.0)}),
        ("icc", {"icc_profile": icc}),
        ("tiffinfo270", {"tiffinfo": {270: "desc-probe"}}),
        ("tiffinfo315", {"tiffinfo": {315: "artist-probe"}}),
        ("tiffinfo700", {"tiffinfo": {700: xmp}}),
        ("tiffinfo270_315", {"tiffinfo": {270: "desc-probe", 315: "artist-probe"}}),
        ("dpi_icc", {"dpi": (300.0, 150.0), "icc_profile": icc}),
        ("dpi_tiffinfo", {"dpi": (300.0, 150.0), "tiffinfo": {270: "desc-probe"}}),
    ]
    for name, extra in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-meta-%s.tif" % name)
        try:
            im = Image.new("L", (2, 2), 7)
            kwargs = {"big_tiff": True}
            kwargs.update(extra)
            try:
                im.save(path, "TIFF", **kwargs)
                with open(path, "rb") as handle:
                    data = handle.read()
                tags, kind = tag_map(data)
                print("case:", name, "kind:", kind, "size:", len(data),
                      "tags:", sorted(tags))
                with Image.open(path) as reopened:
                    info = {k: (v[:12] if isinstance(v, bytes) else v) for k, v in reopened.info.items()}
                    exif = reopened.getexif()
                    keys = sorted(exif.keys())
                    print("  reopen info:", info)
                    print("  reopen exif:", keys, "dpi:", reopened.info.get("dpi"))
            except Exception as err:
                print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass

    # exif family (object + bytes forms) and compression composition
    from PIL import Image as Im2
    for name, exif_kwargs in [
        ("exif_obj", {"exif": Image.Exif()}),
        ("exif_bytes", {"exif": b"Exif\x00\x00MM\x00\x2a\x00\x00\x00\x08"}),
    ]:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        try:
            im = Image.new("L", (2, 2), 7)
            if name == "exif_obj":
                exif = Image.Exif()
                exif[270] = "desc-exif"
                exif[305] = "Pillow"
                exif_kwargs = {"exif": exif}
            try:
                im.save(path, "TIFF", big_tiff=True, **exif_kwargs)
                with open(path, "rb") as handle:
                    data = handle.read()
                tags, kind = tag_map(data)
                print("case:", name, "kind:", kind, "tags:", sorted(tags))
                with Image.open(path) as reopened:
                    print("  reopen exif keys:", sorted(reopened.getexif().keys()))
            except Exception as err:
                print("case:", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass

    for name, comp in [("packbits", "packbits"), ("lzw", "lzw")]:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-cmp-%s.tif" % name)
        try:
            im = Image.new("L", (2, 2), 7)
            try:
                im.save(path, "TIFF", big_tiff=True, compression=comp,
                        dpi=(300.0, 150.0), icc_profile=icc,
                        tiffinfo={270: "desc-probe", 700: xmp})
                with open(path, "rb") as handle:
                    data = handle.read()
                tags, kind = tag_map(data)
                print("case: cmp+meta", name, "kind:", kind, "tags:", sorted(tags))
            except Exception as err:
                print("case: cmp+meta", name, "ERR", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
