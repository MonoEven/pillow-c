"""One-shot oracle probe for FMT-TIFF-003AG.

Builds a Pillow-valid little-endian BigTIFF 4x3 2x2-tiled mode-L fixture with
IFD0 ICCProfile (tag 34675, type 7) and XMP (tag 700, type 1) and prints the
exact Pillow 11.3.0 open/info/getexif behavior.
"""

import os
import struct
import sys
import tempfile


def le16(value):
    return struct.pack("<H", value)


def le64(value):
    return struct.pack("<Q", value)


def xmp_packet():
    return (
        '<?xpacket begin=""?><x:xmpmeta xmlns:x="adobe:ns:meta/">'
        '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
        '<rdf:Description xmlns:dc="http://purl.org/dc/elements/1.1/">'
        '<dc:title><rdf:Alt><rdf:li xml:lang="x-default">Hello</rdf:li></rdf:Alt></dc:title>'
        "</rdf:Description></rdf:RDF></x:xmpmeta><?xpacket end=\"w\"?>"
    ).encode("utf-8")


def build_fixture(icc, xmp, icc_type=7, xmp_type=1):
    channels = 1
    photometric = 1
    entries = [
        (256, 4, 1, 4),
        (257, 4, 1, 3),
        (258, 3, 1, 8),
        (259, 3, 1, 1),
        (262, 3, 1, photometric),
        (277, 3, 1, channels),
        (284, 3, 1, 1),
        (322, 4, 1, 2),
        (323, 4, 1, 2),
    ]
    tile_size = 2 * 2 * channels
    entry_count = len(entries) + 4  # 324, 325, 700, 34675
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 4 * 8
    pixel_offset = tile_byte_counts_offset + 4 * 8
    icc_offset = pixel_offset + tile_size * 4
    xmp_offset = icc_offset + len(icc)
    entries.append((324, 16, 4, tile_offsets_offset))
    entries.append((325, 16, 4, tile_byte_counts_offset))
    entries.append((700, xmp_type, len(xmp), xmp_offset))
    entries.append((34675, icc_type, len(icc), icc_offset))
    entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II"
    out += le16(43)
    out += le16(8)
    out += le16(0)
    out += le64(16)
    out += le64(len(entries))
    for tag, typ, count, value in entries:
        out += le16(tag)
        out += le16(typ)
        out += le64(count)
        out += le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + tile_size, pixel_offset + tile_size * 2, pixel_offset + tile_size * 3):
        out += le64(offset)
    for value in (tile_size, tile_size, tile_size, tile_size):
        out += le64(value)
    for index in range(tile_size * 4):
        out.append(index + 1)
    out += icc
    out += xmp
    return bytes(out)


def build_two_frame_fixture(icc0, xmp0, icc1, xmp1, has_icc1=True, has_xmp1=True):
    """Little-endian BigTIFF, two chained 4x3 2x2-tiled L IFDs.

    IFD0 always carries ICC/XMP; IFD1 carries them only when has_icc1/
    has_xmp1 are true.
    """

    def ifd_entries(has_icc, has_xmp, icc, xmp):
        entries = [
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
        return entries, 11 + (1 if has_xmp else 0) + (1 if has_icc else 0)

    ifd0_offset = 16
    ifd0_list, ifd0_count = ifd_entries(True, True, icc0, xmp0)
    ifd1_offset = ifd0_offset + 8 + ifd0_count * 20 + 8
    ifd1_list, ifd1_count = ifd_entries(has_icc1, has_xmp1, icc1, xmp1)
    f0_tiles = ifd1_offset + 8 + ifd1_count * 20 + 8
    f0_counts = f0_tiles + 32
    f1_tiles = f0_counts + 32
    f1_counts = f1_tiles + 32
    pixel0 = f1_counts + 32
    pixel1 = pixel0 + 16
    icc0_off = pixel1 + 16
    xmp0_off = icc0_off + len(icc0)
    icc1_off = xmp0_off + len(xmp0)
    xmp1_off = icc1_off + (len(icc1) if has_icc1 else 0)

    def finalize(entries, icc_off, xmp_off, has_icc, has_xmp, icc, xmp, tiles_off, counts_off):
        result = list(entries)
        result.append((324, 16, 4, tiles_off))
        result.append((325, 16, 4, counts_off))
        if has_xmp:
            result.append((700, 1, len(xmp), xmp_off))
        if has_icc:
            result.append((34675, 7, len(icc), icc_off))
        result.sort(key=lambda e: e[0])
        return result

    ifd0 = finalize(ifd0_list, icc0_off, xmp0_off, True, True, icc0, xmp0, f0_tiles, f0_counts)
    ifd1 = finalize(ifd1_list, icc1_off, xmp1_off, has_icc1, has_xmp1, icc1, xmp1, f1_tiles, f1_counts)

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(ifd0_offset)
    out += le64(len(ifd0))
    for tag, typ, count, value in ifd0:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(ifd1_offset)
    out += le64(len(ifd1))
    for tag, typ, count, value in ifd1:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel0, pixel0 + 4, pixel0 + 8, pixel0 + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for offset in (pixel1, pixel1 + 4, pixel1 + 8, pixel1 + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    out += bytes(range(1, 17))
    out += bytes(range(65, 81))
    out += icc0
    out += xmp0
    if has_icc1:
        out += icc1
    if has_xmp1:
        out += xmp1
    return bytes(out)


def main():
    from PIL import Image

    cases = [
        ("icc12-type7", bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]), xmp_packet(), 7, 1),
        ("icc6-inline-type7", bytes([1, 2, 3, 4, 5, 6]), xmp_packet(), 7, 1),
        ("icc12-type1-xmp-type7", bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]), xmp_packet(), 1, 7),
    ]
    for name, icc, xmp, icc_type, xmp_type in cases:
        run_case(name, icc, xmp, icc_type, xmp_type)
    run_dpi_cases()
    run_ascii_cases()
    run_uint_cases()
    run_short_array_cases()
    run_rational_cases()
    run_family_cases()
    run_two_frame_cases()
    run_malformed_cases()


def run_case(name, icc, xmp, icc_type, xmp_type):
    from PIL import Image

    data = build_fixture(icc, xmp, icc_type, xmp_type)
    path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-icc-xmp-%s.tif" % name)
    with open(path, "wb") as handle:
        handle.write(data)
    try:
        with Image.open(path) as im:
            print("case:", name)
            print("mode:", im.mode)
            print("size:", im.size)
            print("n_frames:", im.n_frames)
            print("tobytes:", list(im.tobytes()))
            print("info icc_profile:", im.info.get("icc_profile"))
            print("info xmp len:", None if im.info.get("xmp") is None else len(im.info["xmp"]))
            exif = im.getexif()
            print("exif[34675]:", repr(exif.get(34675)))
            print("exif[700] len:", None if exif.get(700) is None else len(exif[700]))
    finally:
        try:
            os.remove(path)
        except OSError:
            pass


def build_dpi_fixture(has_x, num_x, den_x, has_y, num_y, den_y, has_unit, unit):
    """Same 4x3 2x2-tiled L BigTIFF with optional RATIONAL 282/283 and
    SHORT 296 resolution entries (8-byte inline value fields)."""

    def pack_rational(num, den):
        return num | (den << 32)

    entries = [
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
    if has_x:
        entries.append((282, 5, 1, pack_rational(num_x, den_x)))
    if has_y:
        entries.append((283, 5, 1, pack_rational(num_y, den_y)))
    if has_unit:
        entries.append((296, 3, 1, unit))
    tile_size = 4
    entry_count = len(entries) + 2
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    entries.append((324, 16, 4, tile_offsets_offset))
    entries.append((325, 16, 4, tile_byte_counts_offset))
    entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(entries))
    for tag, typ, count, value in entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    return bytes(out)


def run_dpi_cases():
    from PIL import Image

    cases = [
        ("dpi-300-150", build_dpi_fixture(True, 300, 1, True, 150, 1, True, 2)),
        ("dpi-fractional", build_dpi_fixture(True, 291, 2, True, 144, 1, True, 2)),
        ("dpi-no-unit", build_dpi_fixture(True, 300, 1, True, 150, 1, False, 0)),
        ("dpi-cm-unit", build_dpi_fixture(True, 118, 1, True, 59, 1, True, 3)),
        ("dpi-absent", build_dpi_fixture(False, 0, 1, False, 0, 1, False, 0)),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                print("mode/size:", im.mode, im.size)
                print("info dpi:", im.info.get("dpi"))
                print("info resolution:", im.info.get("resolution"))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_ascii_fixture(entries):
    """Same 4x3 2x2-tiled L BigTIFF with ASCII (type 2) tags.

    entries is a list of (tag, bytes-value) pairs; count <= 8 values are
    stored inline in the value field, larger ones at out-of-line offsets.
    """

    def pack_inline(data):
        value = 0
        for index, byte in enumerate(data):
            value |= byte << (index * 8)
        return value

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
    entry_count = len(base) + 2 + len(entries)
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    blobs = bytearray()
    cursor = pixel_offset + 16
    full_entries = list(base)
    for tag, data in entries:
        if len(data) <= 8:
            full_entries.append((tag, 2, len(data), pack_inline(data)))
        else:
            full_entries.append((tag, 2, len(data), cursor))
            blobs += data
            cursor += len(data)
    full_entries.append((324, 16, 4, tile_offsets_offset))
    full_entries.append((325, 16, 4, tile_byte_counts_offset))
    full_entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(full_entries))
    for tag, typ, count, value in full_entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    out += blobs
    return bytes(out)


def run_ascii_cases():
    from PIL import Image

    cases = [
        ("ascii-out-of-line", build_ascii_fixture([(270, b"Document Alpha\0"), (315, b"Ada Lovelace\0")])),
        ("ascii-inline", build_ascii_fixture([(315, b"A\0"), (305, b"pillow-c\0")])),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                print("mode/size:", im.mode, im.size)
                exif = im.getexif()
                for tag in (269, 270, 271, 272, 285, 305, 306, 315, 316, 33432):
                    value = exif.get(tag)
                    if value is not None:
                        print("exif[%d]:" % tag, repr(value))
                print("tag_v2 270:", repr(im.tag_v2.get(270)))
                print("tag_v2 315:", repr(im.tag_v2.get(315)))
                print("info keys:", sorted(im.info.keys()))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_uint_fixture(extras):
    """Same 4x3 2x2-tiled L BigTIFF with extra scalar uint tags.

    extras is a list of (tag, type, value) entries appended to the base
    shape; type 3 SHORT, 4 LONG, and 16 LONG8 all sit inline in the value
    field with count 1.
    """
    entries = [
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
    entries.extend(extras)
    entry_count = len(entries) + 2
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    entries.append((324, 16, 4, tile_offsets_offset))
    entries.append((325, 16, 4, tile_byte_counts_offset))
    entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(entries))
    for tag, typ, count, value in entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    return bytes(out)


def run_uint_cases():
    from PIL import Image

    cases = [
        ("uint-scalars", build_uint_fixture([(317, 3, 1, 1), (339, 16, 1, 1), (340, 3, 1, 0), (341, 3, 1, 255)])),
        ("uint-array-long", build_uint_fixture([(279, 4, 2, 12 | (34 << 32))])),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                exif = im.getexif()
                for tag in (256, 257, 258, 259, 262, 277, 279, 284, 317, 322, 323, 324, 325, 339, 340, 341):
                    value = exif.get(tag)
                    if value is not None:
                        print("exif[%d]:" % tag, repr(value))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_short_array_fixture(entries):
    """Same 4x3 2x2-tiled L BigTIFF with extra SHORT (type 3) array tags.

    entries is a list of (tag, [values]); byte sizes of eight or fewer are
    inline in the value field, larger arrays live at out-of-line offsets.
    """

    def pack_inline(values):
        result = 0
        for index, value in enumerate(values):
            result |= value << (index * 16)
        return result

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
    entry_count = len(base) + 2 + len(entries)
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    blobs = bytearray()
    cursor = pixel_offset + 16
    full_entries = list(base)
    for tag, values in entries:
        size = len(values) * 2
        if size <= 8:
            full_entries.append((tag, 3, len(values), pack_inline(values)))
        else:
            full_entries.append((tag, 3, len(values), cursor))
            for value in values:
                blobs += struct.pack("<H", value)
            cursor += size
    full_entries.append((324, 16, 4, tile_offsets_offset))
    full_entries.append((325, 16, 4, tile_byte_counts_offset))
    full_entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(full_entries))
    for tag, typ, count, value in full_entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    out += blobs
    return bytes(out)


def run_short_array_cases():
    from PIL import Image

    cases = [
        ("short-arrays", build_short_array_fixture([
            (530, [2, 1]),
            (291, [0, 128, 255]),
            (297, [3, 7]),
            (34735, [1, 1, 0, 1, 1024, 0, 1, 1]),
            (342, [0, 255, 1, 254, 2, 253]),
            (42081, [2, 5]),
            (50712, [0, 1, 2, 3]),
            (37396, [7, 9, 11, 13]),
            (50829, [0, 0, 1, 2]),
        ])),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                exif = im.getexif()
                for tag in (291, 297, 342, 530, 34735, 37396, 42081, 50712, 50829):
                    value = exif.get(tag)
                    if value is not None:
                        print("exif[%d]:" % tag, repr(value))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_rational_fixture(entries):
    """Same 4x3 2x2-tiled L BigTIFF with extra RATIONAL (type 5) tags.

    entries is a list of (tag, [pairs]) where pairs are [num, den]; a single
    pair (eight bytes) sits inline in the value field, arrays live at
    out-of-line offsets.
    """

    def pack_inline(pair):
        return pair[0] | (pair[1] << 32)

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
    entry_count = len(base) + 2 + len(entries)
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    blobs = bytearray()
    cursor = pixel_offset + 16
    full_entries = list(base)
    for tag, pairs in entries:
        size = len(pairs) * 8
        if size <= 8:
            full_entries.append((tag, 5, len(pairs), pack_inline(pairs[0])))
        else:
            full_entries.append((tag, 5, len(pairs), cursor))
            for pair in pairs:
                blobs += struct.pack("<II", pair[0], pair[1])
            cursor += size
    full_entries.append((324, 16, 4, tile_offsets_offset))
    full_entries.append((325, 16, 4, tile_byte_counts_offset))
    full_entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(full_entries))
    for tag, typ, count, value in full_entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    out += blobs
    return bytes(out)


def run_rational_cases():
    from PIL import Image

    cases = [
        ("rationals", build_rational_fixture([
            (33434, [[1, 125]]),
            (33437, [[14, 5]]),
            (37386, [[50, 1]]),
            (318, [[1, 2], [3, 4]]),
            (319, [[1, 2], [3, 4], [5, 6], [7, 8], [9, 10], [11, 12]]),
            (529, [[1, 2], [3, 4], [5, 6]]),
            (532, [[0, 1], [255, 1], [1, 2], [3, 4], [5, 6], [7, 8]]),
        ])),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                exif = im.getexif()
                for tag in (318, 319, 529, 532, 33434, 33437, 37386):
                    value = exif.get(tag)
                    if value is not None:
                        print("exif[%d]:" % tag, repr(value))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def run_two_frame_cases():
    from PIL import Image

    icc0 = bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])
    icc1 = bytes([21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32])
    xmp0 = xmp_packet().replace(b"Hello", b"First")
    xmp1 = xmp_packet().replace(b"Hello", b"Second")

    cases = [
        ("both-frames-metadata", build_two_frame_fixture(icc0, xmp0, icc1, xmp1, True, True)),
        ("frame1-no-icc", build_two_frame_fixture(icc0, xmp0, icc1, xmp1, False, True)),
        ("frame1-no-xmp", build_two_frame_fixture(icc0, xmp0, icc1, xmp1, True, False)),
        ("frame1-no-metadata", build_two_frame_fixture(icc0, xmp0, icc1, xmp1, False, False)),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-two-frame-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                print("n_frames:", im.n_frames)
                print("frame0 icc:", im.info.get("icc_profile"))
                print("frame0 xmp:", im.info.get("xmp"))
                print("frame0 bytes:", list(im.tobytes()))
                exif0 = im.getexif()
                print("frame0 exif[34675]:", repr(exif0.get(34675)))
                print("frame0 exif[700] len:", None if exif0.get(700) is None else len(exif0[700]))
                im.seek(1)
                print("frame1 icc:", im.info.get("icc_profile"))
                print("frame1 xmp:", im.info.get("xmp"))
                print("frame1 bytes:", list(im.tobytes()))
                exif1 = im.getexif()
                print("frame1 exif[34675]:", repr(exif1.get(34675)))
                print("frame1 exif[700] len:", None if exif1.get(700) is None else len(exif1[700]))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_generic_fixture(entries):
    """Same 4x3 2x2-tiled L BigTIFF with extra typed tags.

    entries is a list of (tag, type, count, value); value is either a
    packed integer stored inline or a bytes/bytearray blob placed at an
    out-of-line offset.
    """
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
    entry_count = len(base) + 2 + len(entries)
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    blobs = bytearray()
    cursor = pixel_offset + 16
    full_entries = list(base)
    for tag, typ, count, value in entries:
        if isinstance(value, (bytes, bytearray)):
            if len(value) <= 8:
                full_entries.append((tag, typ, count, int.from_bytes(bytes(value).ljust(8, b"\x00"), "little")))
            else:
                full_entries.append((tag, typ, count, cursor))
                blobs += value
                cursor += len(value)
        else:
            full_entries.append((tag, typ, count, value))
    full_entries.append((324, 16, 4, tile_offsets_offset))
    full_entries.append((325, 16, 4, tile_byte_counts_offset))
    full_entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(full_entries))
    for tag, typ, count, value in full_entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    out += blobs
    return bytes(out)


def pack_double(values):
    result = bytearray()
    for value in values:
        result += struct.pack("<d", value)
    if len(result) <= 8:
        return int.from_bytes(result.ljust(8, b"\x00"), "little")
    return bytes(result)


def pack_float(values):
    result = bytearray()
    for value in values:
        result += struct.pack("<f", value)
    if len(result) <= 8:
        return int.from_bytes(result.ljust(8, b"\x00"), "little")
    return bytes(result)


def pack_signed_rational(pairs):
    result = bytearray()
    for num, den in pairs:
        result += struct.pack("<ii", num, den)
    if len(result) <= 8:
        return int.from_bytes(result.ljust(8, b"\x00"), "little")
    return bytes(result)


def run_family_cases():
    from PIL import Image

    cases = [
        ("signed-rationals", build_generic_fixture([
            (37377, 10, 1, pack_signed_rational([(-3, 1)])),
            (37379, 10, 1, pack_signed_rational([(17, 5)])),
            (37380, 10, 1, pack_signed_rational([(3, 7)])),
            (50716, 10, 1, pack_signed_rational([(1, 4)])),
            (51044, 10, 1, pack_signed_rational([(9, 2)])),
        ]), [37377, 37379, 37380, 50716, 51044]),
        ("signed-rational-arrays", build_generic_fixture([
            (50715, 10, 2, pack_signed_rational([(1, 2), (3, 4)])),
            (50722, 10, 9, pack_signed_rational([(-2, 1), (-1, 2), (0, 1), (1, 2), (2, 1), (3, 2), (4, 1), (5, 2), (6, 1)])),
            (50964, 10, 9, pack_signed_rational([(-1, 3)] * 9)),
            (52531, 10, 9, pack_signed_rational([(1, 3)] * 9)),
        ]), [50715, 50722, 50964, 52531]),
        ("double-arrays", build_generic_fixture([
            (33550, 12, 3, pack_double([1.5, 2.5, 3.5])),
            (33922, 12, 6, pack_double([0.0, 1.0, 0.0, 0.0, 0.0, 1.0])),
            (34736, 12, 3, pack_double([-1.0, 2.0, 0.5])),
            (51041, 12, 4, pack_double([1.0, 2.0, 3.0, 4.0])),
        ]), [33550, 33922, 34736, 51041]),
        ("float-arrays", build_generic_fixture([
            (50939, 11, 6, pack_float([1.0, 0.0, 0.0, 0.0, 1.0, 0.0])),
            (50940, 11, 18, pack_float([float(i) for i in range(18)])),
        ]), [50939, 50940]),
        ("byte-arrays", build_generic_fixture([
            (34377, 1, 16, bytes([11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26])),
            (50781, 1, 16, bytes([1] * 16)),
            (50831, 1, 8, bytes([2] * 8)),
            (51043, 1, 8, bytes([3] * 8)),
        ]), [34377, 50781, 50831, 51043]),
        ("undefined-blobs", build_generic_fixture([
            (37510, 7, 6, b"com \x00\x00\x01\x00"),
            (37724, 7, 12, bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12])),
            (34856, 1, 8, bytes([7] * 8)),
            (50969, 1, 16, bytes([9] * 16)),
            (52525, 1, 4, bytes([5] * 4)),
        ]), [37510, 37724, 34856, 50969, 52525]),
    ]
    for name, data, tags in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            with Image.open(path) as im:
                print("case:", name)
                print("mode/size:", im.mode, im.size)
                exif = im.getexif()
                for tag in tags:
                    value = exif.get(tag)
                    if value is not None:
                        print("exif[%d]:" % tag, repr(value))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


def build_malformed_fixture(mutations):
    """Same 4x3 2x2-tiled L BigTIFF with hand-crafted malformed entries.

    mutations is a list of (tag, type, count, value) raw entries appended to
    the base shape; the caller supplies bad counts/offsets/types directly.
    """
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
    entries.extend(mutations)
    entry_count = len(entries) + 2
    tile_offsets_offset = 16 + 8 + entry_count * 20 + 8
    tile_byte_counts_offset = tile_offsets_offset + 32
    pixel_offset = tile_byte_counts_offset + 32
    entries.append((324, 16, 4, tile_offsets_offset))
    entries.append((325, 16, 4, tile_byte_counts_offset))
    entries.sort(key=lambda e: e[0])

    out = bytearray()
    out += b"II" + le16(43) + le16(8) + le16(0) + le64(16)
    out += le64(len(entries))
    for tag, typ, count, value in entries:
        out += le16(tag) + le16(typ) + le64(count) + le64(value)
    out += le64(0)
    for offset in (pixel_offset, pixel_offset + 4, pixel_offset + 8, pixel_offset + 12):
        out += le64(offset)
    for value in (4, 4, 4, 4):
        out += le64(value)
    for index in range(16):
        out.append(index + 1)
    return bytes(out)


def run_malformed_cases():
    from PIL import Image

    file_size = 16 + 8 + 13 * 20 + 8 + 32 + 32 + 16
    cases = [
        ("truncated-blob", build_malformed_fixture([(34675, 7, 12, file_size + 100)])),
        ("overflow-count", build_malformed_fixture([(33434, 5, 0xFFFFFFFF, 0)])),
        ("invalid-type-ascii", build_malformed_fixture([(270, 4, 1, 7)])),
        ("invalid-type-rational", build_malformed_fixture([(282, 3, 1, 2)])),
        ("zero-count-blob", build_malformed_fixture([(700, 7, 0, 0)])),
        ("zero-denominator", build_malformed_fixture([(33434, 5, 1, 1 | (0 << 32))])),
    ]
    for name, data in cases:
        path = os.path.join(tempfile.gettempdir(), "probe-bigtiff-malformed-%s.tif" % name)
        with open(path, "wb") as handle:
            handle.write(data)
        try:
            try:
                with Image.open(path) as im:
                    print("case:", name)
                    print("opened: mode/size:", im.mode, im.size)
                    try:
                        print("tobytes:", list(im.tobytes()))
                    except Exception as err:
                        print("tobytes error:", type(err).__name__, str(err))
                    print("info keys:", sorted(im.info.keys()))
                    exif = im.getexif()
                    print("exif[34675]:", repr(exif.get(34675)))
                    print("exif[700]:", repr(exif.get(700)))
                    print("exif[270]:", repr(exif.get(270)))
                    print("exif[282]:", repr(exif.get(282)))
                    print("exif[33434]:", repr(exif.get(33434)))
            except Exception as err:
                print("case:", name)
                print("open error:", type(err).__name__, str(err))
        finally:
            try:
                os.remove(path)
            except OSError:
                pass


if __name__ == "__main__":
    main()
