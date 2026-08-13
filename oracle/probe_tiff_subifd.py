"""One-shot oracle probe for FMT-TIFF-003AT.

Builds a Pillow-valid little-endian classic strip TIFF 2x2 mode-L fixture
whose IFD0 carries ExifIFD (tag 34665, LONG) and GPSInfo (tag 34853, LONG)
sub-IFD pointers, and prints the exact Pillow 11.3.0 getexif()/get_ifd()
behavior.
"""

import os
import struct
import tempfile


def le16(value):
    return struct.pack("<H", value)


def le32(value):
    return struct.pack("<I", value)


def build_fixture():
    # IFD0: 11 entries. Sub-IFDs placed after IFD0 and the pixel bytes.
    ifd0_count = 11
    ifd0_end = 8 + 2 + ifd0_count * 12 + 4
    strip_offset = ifd0_end
    exif_ifd_offset = strip_offset + 4
    gps_ifd_offset = exif_ifd_offset + 2 + 3 * 12 + 4
    exif_blobs_offset = gps_ifd_offset + 2 + 3 * 12 + 4

    # Exif sub-IFD: 36867 ASCII (20 bytes), 33434 RATIONAL 1/125,
    # 37377 SRATIONAL -3/1. Blobs sit after both sub-IFDs at 282.
    ascii_value = b"2024:01:01 00:00:00\x00"  # 20 bytes
    rational_bytes = struct.pack("<II", 1, 125)
    srational_bytes = struct.pack("<ii", -3, 1)
    exif_entries = [
        (36867, 2, 20, 282),
        (33434, 5, 1, 302),
        (37377, 10, 1, 310),
    ]

    # GPS sub-IFD: 1 ASCII "N" (inline), 2 RATIONAL[3], 3 ASCII "W" (inline),
    # 5 uint 1, 6 RATIONAL 36/1, 20 RATIONAL[3], 21 RATIONAL 1/1.
    gps_entries = [
        (1, 2, 2, 0x4E),
        (2, 5, 3, 318),
        (3, 2, 1, 0x57),
        (5, 3, 1, 1),
        (6, 5, 1, 374),
        (20, 5, 3, 342),
        (21, 5, 1, 366),
    ]

    entries = [
        (256, 4, 1, 2),
        (257, 4, 1, 2),
        (258, 3, 1, 8),
        (259, 3, 1, 1),
        (262, 3, 1, 1),
        (273, 4, 1, strip_offset),
        (277, 3, 1, 1),
        (278, 4, 1, 2),
        (279, 4, 1, 4),
        (34665, 4, 1, exif_ifd_offset),
        (34853, 4, 1, gps_ifd_offset),
    ]
    entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(42) + le32(8)
    out += le16(len(entries))
    for tag, typ, count, value in entries:
        out += le16(tag) + le16(typ) + le32(count) + le32(value)
    out += le32(0)
    out += bytes([1, 2, 3, 4])
    out += le16(len(exif_entries))
    for tag, typ, count, value in exif_entries:
        out += le16(tag) + le16(typ) + le32(count) + le32(value)
    out += le32(0)
    out += le16(len(gps_entries))
    for tag, typ, count, value in gps_entries:
        out += le16(tag) + le16(typ) + le32(count) + le32(value)
    out += le32(0)
    out += ascii_value
    out += rational_bytes
    out += srational_bytes
    out += struct.pack("<II", 36, 1) + struct.pack("<II", 0, 1) + struct.pack("<II", 0, 1)
    out += struct.pack("<II", 1, 2) + struct.pack("<II", 3, 4) + struct.pack("<II", 5, 6)
    out += struct.pack("<II", 1, 1)
    out += struct.pack("<II", 36, 1)
    return bytes(out)


def main():
    from PIL import Image

    data = build_fixture()
    path = os.path.join(tempfile.gettempdir(), "probe-tiff-subifd.tif")
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
