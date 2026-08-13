"""FMT-TIFF-003BJ cross-check: DLL mixed-size BigTIFF frames -> Pillow reopen.

Verifies the chained per-frame-dimension layout against Pillow 11.3.0.
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
    save = lib.pillow_c_image_save_tiff_bigtiff_frames_compression_options
    save.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int]
    save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-mixed.tif")
    first = make_image(lib, 2, 2, 1, b"\x07\x07\x07\x07")
    second = make_image(lib, 3, 1, 1, b"\x09\x09\x09")
    third = make_image(lib, 1, 3, 1, b"\x05\x06\x04")
    try:
        images = (ctypes.c_void_p * 3)(first, second, third)
        status = save(images, 3, path.encode("utf-8"), 1)
        assert status == 0, status
        data = open(path, "rb").read()
        ok = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            n = getattr(reopened, "n_frames", 1)
            ok = ok and n == 3
            for index, (expected_size, expected_bytes) in enumerate([
                ((2, 2), b"\x07\x07\x07\x07"),
                ((3, 1), b"\x09\x09\x09"),
                ((1, 3), b"\x05\x06\x04"),
            ]):
                reopened.seek(index)
                ok = ok and reopened.size == expected_size
                ok = ok and reopened.tobytes() == expected_bytes
        print("mixed-size three-frame bigtiff=%s per-frame_size/bytes_ok=%s" % (data[:2] == b"II", ok))
        if not ok:
            failures += 1
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(first)
        free(second)
        free(third)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
