"""FMT-TIFF-003BD cross-check: DLL numeric BigTIFF strip save -> Pillow reopen.

Verifies the native writer against the Pillow 11.3.0 authority.
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

MODE_L, MODE_CMYK, MODE_I, MODE_F, MODE_I16, MODE_I16B = 1, 7, 8, 9, 11, 12


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

    cases = [
        ("i16", MODE_I16, (3, 2),
         struct.pack("<6H", 0, 1, 513, 65535, 32768, 42),
         struct.pack("<6H", 0, 1, 513, 65535, 32768, 42), "I;16"),
        ("i16b", MODE_I16B, (3, 2),
         struct.pack(">6H", 0, 1, 513, 65535, 32768, 42),
         struct.pack("<6H", 0, 1, 513, 65535, 32768, 42), "I;16"),
        ("i", MODE_I, (3, 2),
         struct.pack("<6i", -1, 7, 0x01020304, -123456, 0, 2147483647),
         struct.pack("<6i", -1, 7, 0x01020304, -123456, 0, 2147483647), "I"),
        ("f", MODE_F, (3, 2),
         struct.pack("<6f", 0.0, 1.5, -2.25, 3.14159, 1e30, -0.125),
         struct.pack("<6f", 0.0, 1.5, -2.25, 3.14159, 1e30, -0.125), "F"),
        ("cmyk", MODE_CMYK, (3, 2),
         bytes([10, 20, 30, 40, 0, 0, 0, 0, 255, 255, 255, 255,
                1, 2, 3, 4, 128, 129, 130, 131, 250, 251, 252, 253]),
         bytes([10, 20, 30, 40, 0, 0, 0, 0, 255, 255, 255, 255,
                1, 2, 3, 4, 128, 129, 130, 131, 250, 251, 252, 253]), "CMYK"),
    ]

    failures = 0
    for name, mode, (width, height), payload, expected_payload, expected_mode in cases:
        image = ctypes.c_void_p()
        status = create(width, height, mode, ctypes.byref(image))
        assert status == 0, (name, "create", status)
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0, name
        assert size.value == len(payload), (name, size.value, len(payload))
        ctypes.memmove(ptr, payload, len(payload))
        path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-num-%s.tif" % name)
        try:
            save_status = save(image, path.encode("utf-8"))
            assert save_status == 0, (name, "save", save_status)
            with open(path, "rb") as handle:
                header = handle.read(16)
            is_bigtiff = header[:2] == b"II" and struct.unpack("<H", header[2:4])[0] == 43
            with Image.open(path) as reopened:
                raw = reopened.tobytes()
                ok = (
                    is_bigtiff
                    and reopened.mode == expected_mode
                    and reopened.size == (width, height)
                    and raw == expected_payload
                )
            print("%-6s bigtiff=%-5s mode=%-6s size=%s bytes_ok=%s" % (
                name, is_bigtiff, expected_mode, (width, height), raw == expected_payload))
            if not ok:
                failures += 1
                print("  got:", raw.hex(" "))
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
