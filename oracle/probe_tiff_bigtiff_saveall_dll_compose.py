"""FMT-TIFF-003BG cross-check: DLL BigTIFF multi-frame save -> Pillow reopen.

Verifies numeric save_all and metadata save_all compositions against the
Pillow 11.3.0 authority (chained IFDs, per-frame metadata).
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


def make_image(lib, width, height, mode, payload):
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    image = ctypes.c_void_p()
    assert create(width, height, mode, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, payload, len(payload))
    return image


def main():
    lib = ctypes.CDLL(DLL)
    frames_save = lib.pillow_c_image_save_tiff_bigtiff_frames_compression_options
    frames_save.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int]
    frames_save.restype = ctypes.c_int
    meta_save = lib.pillow_c_image_save_tiff_bigtiff_frames_metadata_ascii_entries_options
    meta_save.argtypes = [
        ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_char_p,
        ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_int,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_size_t),
        ctypes.c_size_t,
    ]
    meta_save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    # 1. Numeric two-frame BigTIFF (I;16 frames)
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-sa-i16.tif")
    first = make_image(lib, 2, 2, 11, struct.pack("<4H", 1, 2, 513, 65535))
    second = make_image(lib, 2, 2, 11, struct.pack("<4H", 7, 7, 7, 7))
    try:
        images = (ctypes.c_void_p * 2)(first, second)
        status = frames_save(images, 2, path.encode("utf-8"), 1)
        assert status == 0, status
        data = open(path, "rb").read()
        ok = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            ok = ok and n == 2
            reopened.seek(0)
            ok = ok and reopened.tobytes() == struct.pack("<4H", 1, 2, 513, 65535)
            reopened.seek(1)
            ok = ok and reopened.tobytes() == struct.pack("<4H", 7, 7, 7, 7)
        print("numeric two-frame bigtiff=%s frames/bytes_ok=%s" % (data[:2] == b"II", ok))
        if not ok:
            failures += 1
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(first)
        free(second)

    # 2. Metadata two-frame BigTIFF (L frames with dpi/icc/ascii per frame)
    icc = bytes(range(20))
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-sa-meta.tif")
    first = make_image(lib, 2, 2, 1, b"\x07\x07\x07\x07")
    second = make_image(lib, 2, 2, 1, b"\x09\x09\x09\x09")
    try:
        desc = ctypes.create_string_buffer(b"desc-probe\0")
        tags = (ctypes.c_int * 1)(270)
        value_ptrs = (ctypes.c_char_p * 1)(ctypes.cast(desc, ctypes.c_char_p))
        sizes = (ctypes.c_size_t * 1)(11)
        icc_buf = (ctypes.c_uint8 * len(icc)).from_buffer_copy(icc)
        images = (ctypes.c_void_p * 2)(first, second)
        status = meta_save(
            images, 2, path.encode("utf-8"),
            1, 300.0, 150.0, 1,
            icc_buf, len(icc), None, 0,
            tags, value_ptrs, sizes, 1,
        )
        assert status == 0, status
        data = open(path, "rb").read()
        ok = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            ok = ok and n == 2
            for index, expected_bytes in enumerate([b"\x07\x07\x07\x07", b"\x09\x09\x09\x09"]):
                reopened.seek(index)
                ok = ok and reopened.tobytes() == expected_bytes
                ok = ok and reopened.info.get("dpi") == (300.0, 150.0)
                ok = ok and reopened.info.get("icc_profile") == icc
                ok = ok and reopened.getexif().get(270) == "desc-probe"
        print("metadata two-frame bigtiff=%s per-frame_metadata_ok=%s" % (data[:2] == b"II", ok))
        if not ok:
            failures += 1
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(first)
        free(second)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
