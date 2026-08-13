"""FMT-TIFF-003BI cross-check: DLL BigTIFF mode-1 save -> Pillow reopen.

Verifies the bilevel BigTIFF save (photometric 1, no 258, packed strips)
against the Pillow 11.3.0 authority, plus the DLL reopen of Pillow's own
mode-1 BigTIFF.
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

MODE_1 = 5


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    save = lib.pillow_c_image_save_tiff_bigtiff
    save.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    # Mode-1 DLL storage is one byte per pixel (0/1); 9x2 pattern matching
    # the oracle probe.
    unpacked = bytes([1, 0, 1, 0, 1, 0, 1, 0, 1,
                      0, 1, 0, 1, 0, 1, 0, 1, 1])
    expected_packed = bytes([0xAA, 0x80, 0x55, 0x80])

    image = ctypes.c_void_p()
    assert create(9, 2, MODE_1, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    assert size.value == len(unpacked)
    ctypes.memmove(ptr, unpacked, len(unpacked))
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-1.tif")
    try:
        assert save(image, path.encode("utf-8")) == 0
        data = open(path, "rb").read()
        is_bigtiff = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            ok = (
                is_bigtiff
                and reopened.mode == "1"
                and reopened.size == (9, 2)
                and reopened.tobytes() == expected_packed
            )
        print("1 save: bigtiff=%s mode/bytes_ok=%s" % (is_bigtiff, ok))
        if not ok:
            failures += 1
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(image)

    # DLL reopen of Pillow's own mode-1 BigTIFF
    path = os.path.join(tempfile.gettempdir(), "pillow-bigtiff-1.tif")
    try:
        im = Image.new("1", (9, 2), 0)
        im.putdata(list(unpacked))
        im.save(path, "TIFF", big_tiff=True)
        open_fn = lib.pillow_c_image_open_tiff
        open_fn.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        open_fn.restype = ctypes.c_int
        loaded = ctypes.c_void_p()
        assert open_fn(path.encode("utf-8"), ctypes.byref(loaded)) == 0
        data_ptr = ctypes.POINTER(ctypes.c_uint8)()
        data_size = ctypes.c_size_t()
        assert data_fn(loaded, ctypes.byref(data_ptr), ctypes.byref(data_size)) == 0
        expected_unpacked = bytes(255 if bit else 0 for bit in unpacked)
        bytes_ok = bytes(data_ptr[:data_size.value]) == expected_unpacked
        print("1 open: bytes_ok=%s" % bytes_ok)
        if not bytes_ok:
            failures += 1
        free(loaded)
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
