"""One-shot oracle probe for FMT-TIFF-003AU.

Builds a Pillow-valid little-endian BigTIFF 4x3 2x2-tiled mode-L fixture
whose IFD0 carries ExifIFD (tag 34665, LONG) and GPSInfo (tag 34853, LONG)
sub-IFD pointers (20-byte entries, 64-bit counts), and prints the exact
Pillow 11.3.0 getexif()/get_ifd() behavior.
"""

import os
import struct
import tempfile


def le16(value):
    return struct.pack("<H", value)


def le64(value):
    return struct.pack("<Q", value)


def bigtiff_entry(tag, typ, count, value):
    return le16(tag) + le16(typ) + le64(count) + le64(value)


def build_fixture():
    base = [
        (256, 4, 1, 4),
        (257, 4, 1, 3),
        (258, 3, 1, 8),
        (259, 3, 1, 1),
        (262, 3, 1, 1),
        (277, 3, 1, 1),
        (284, 3, 1, 1),
        (322, 4, 1, 2),
        (323, 4, 1, 2),
    ]
    entries = list(base)
    entries.append((34665, 4, 1, 372))
    entries.append((34853, 4, 1, 448))
    entry_count = len(entries) + 2
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    entries.append((324, 16, 4, tile_offsets_offset))
    entries.append((325, 16, 4, tile_byte_counts_offset))
    entries.sort(key=lambda e: e[0])

    exif_entries = [
        (36867, 2, 20, 544),
        (33434, 5, 1, 1 | (125 << 32)),
        (37377, 10, 1, ((-3) & 0xFFFFFFFF) | (1 << 32)),
    ]
    gps_entries = [
        (1, 2, 2, 0x4E),
        (2, 5, 3, 564),
        (5, 3, 1, 1),
        (6, 5, 1, 36 | (1 << 32)),
    ]

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(entries))
    for tag, typ, count, value in entries:
        out += bigtiff_entry(tag, typ, count, value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    assert len(out) == 372, len(out)
    out += le64(len(exif_entries))
    for tag, typ, count, value in exif_entries:
        out += bigtiff_entry(tag, typ, count, value)
    out += le64(0)
    assert len(out) == 448, len(out)
    out += le64(len(gps_entries))
    for tag, typ, count, value in gps_entries:
        out += bigtiff_entry(tag, typ, count, value)
    out += le64(0)
    assert len(out) == 544, len(out)
    out += b"2024:01:01 00:00:00\x00"
    out += struct.pack("<II", 36, 1) + struct.pack("<II", 0, 1) + struct.pack("<II", 0, 1)
    return bytes(out)


def main():
    from PIL import Image

    data = build_fixture()
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-subifd.tif")
    with open(path, "wb") as handle:
        handle.write(data)
    try:
        with Image.open(path) as im:
            print("mode/size:", im.mode, im.size)
            print("tobytes:", list(im.tobytes()))
            exif = im.getexif()
            print("exif keys:", sorted(exif.keys()))
            print("exif[34665]:", repr(exif.get(34665)))
            print("exif[34853]:", repr(exif.get(34853)))
            print("exif[36867]:", repr(exif.get(36867)))
            print("exif[33434]:", repr(exif.get(33434)))
            print("exif[37377]:", repr(exif.get(37377)))
            print("exif[1]:", repr(exif.get(1)))
            print("exif[2]:", repr(exif.get(2)))
            print("exif[5]:", repr(exif.get(5)))
            print("exif[6]:", repr(exif.get(6)))
            exif_ifd = exif.get_ifd(0x8769)
            print("exif_ifd:", None if exif_ifd is None else {k: exif_ifd.get(k) for k in exif_ifd})
            gps_ifd = exif.get_ifd(0x8825)
            print("gps_ifd:", None if gps_ifd is None else {k: gps_ifd.get(k) for k in gps_ifd})
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


if __name__ == "__main__":
    main()
