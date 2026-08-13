"""FMT-TIFF-003BE cross-check: DLL BigTIFF metadata save -> Pillow reopen.

Verifies the new metadata export against the Pillow 11.3.0 authority.
"""

import ctypes
import os
import struct
import tempfile

from PIL import Image

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    save = lib.pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options
    save.argtypes = [
        ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_char_p,
        ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_int,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_size_t),
        ctypes.c_size_t,
    ]
    save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    icc = bytes(range(20))
    xmp = b"<x:xmpmeta>probe</x:xmpmeta>"

    cases = [
        ("dpi", dict(has_dpi=1, dpi_x=300.0, dpi_y=150.0)),
        ("icc", dict(icc=icc)),
        ("desc", dict(ascii=[(270, b"desc\0")])),
        ("artist", dict(ascii=[(315, b"artist\0")])),
        ("desc-probe", dict(ascii=[(270, b"desc-probe\0")])),
        ("xmp", dict(xmp=xmp)),
        ("all", dict(has_dpi=1, dpi_x=300.0, dpi_y=150.0, icc=icc, xmp=xmp,
                     ascii=[(270, b"desc\0"), (315, b"artist\0")])),
    ]
    failures = 0
    for name, opts in cases:
        image = ctypes.c_void_p()
        assert create(2, 2, 1, ctypes.byref(image)) == 0, name
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0, name
        ctypes.memmove(ptr, bytes([7, 7, 7, 7]), 4)
        path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-meta-%s.tif" % name)
        try:
            images = (ctypes.c_void_p * 1)(image)
            ascii_pairs = opts.get("ascii", [])
            tags = (ctypes.c_int * len(ascii_pairs))(*[t for t, _ in ascii_pairs])
            buffers = [ctypes.create_string_buffer(v) for _, v in ascii_pairs]
            value_ptrs = (ctypes.c_char_p * len(ascii_pairs))(*[ctypes.cast(b, ctypes.c_char_p) for b in buffers])
            sizes = (ctypes.c_size_t * len(ascii_pairs))(*[len(v) for _, v in ascii_pairs])
            icc_buf = (ctypes.c_uint8 * len(icc)).from_buffer_copy(icc) if "icc" in opts else None
            xmp_buf = (ctypes.c_uint8 * len(xmp)).from_buffer_copy(xmp) if "xmp" in opts else None
            status = save(
                images, 1, path.encode("utf-8"),
                opts.get("has_dpi", 0), opts.get("dpi_x", 0.0), opts.get("dpi_y", 0.0),
                1,
                icc_buf, len(icc) if icc_buf else 0,
                xmp_buf, len(xmp) if xmp_buf else 0,
                tags, value_ptrs, sizes, len(ascii_pairs),
            )
            assert status == 0, (name, "save", status)
            with open(path, "rb") as handle:
                data = handle.read()
            is_bigtiff = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
            with Image.open(path) as reopened:
                ok = (
                    is_bigtiff
                    and reopened.size == (2, 2)
                    and reopened.tobytes() == b"\x07\x07\x07\x07"
                )
                if opts.get("has_dpi"):
                    ok = ok and reopened.info.get("dpi") == (300.0, 150.0)
                if "icc" in opts:
                    ok = ok and reopened.info.get("icc_profile") == icc
                if "xmp" in opts:
                    ok = ok and reopened.info.get("xmp") == xmp
                exif = reopened.getexif()
                for tag, value in ascii_pairs:
                    ok = ok and exif.get(tag) == value.rstrip(b"\0").decode()
            print("%-11s bigtiff=%-5s bytes/dpi/icc/xmp/ascii_ok=%s" % (name, is_bigtiff, ok))
            if not ok:
                failures += 1
                print("  info:", reopened.info if 'reopened' in dir() else None)
        finally:
            try:
                os.remove(path)
            except OSError:
                pass
        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
