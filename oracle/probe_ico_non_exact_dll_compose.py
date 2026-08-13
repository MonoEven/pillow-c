"""FMT-TIFF-001B cross-check: DLL ICO non-exact source selection -> Pillow reopen.

Mirrors the oracle probes: aspect-preserving fallback of the LAST provided
image, no upscaling, and exact-size matches.
"""

import ctypes
import os
import tempfile

from PIL import Image

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)


def make_image(lib, width, height, value):
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    image = ctypes.c_void_p()
    assert create(width, height, 4, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    payload = bytes([value, 0, 0, 255]) * (width * height)
    ctypes.memmove(ptr, payload, len(payload))
    return image


def main():
    lib = ctypes.CDLL(DLL)
    save = lib.pillow_c_image_save_ico_frames_format_options
    save.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_size_t, ctypes.c_char_p,
                     ctypes.POINTER(ctypes.c_int), ctypes.c_size_t, ctypes.c_int, ctypes.c_char_p]
    save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    def run_case(name, images, sizes, expected):
        path = os.path.join(tempfile.gettempdir(), "dll-ico-%s.ico" % name)
        try:
            handles = (ctypes.c_void_p * len(images))(*images)
            size_values = []
            for size in sizes:
                size_values.extend(size)
            size_buf = (ctypes.c_int * len(size_values))(*size_values)
            status = save(handles, len(images), path.encode("utf-8"),
                          size_buf, len(sizes), 1, None)
            assert status == 0, (name, status)
            with Image.open(path) as ico:
                got = {}
                for size in sorted(ico.ico.sizes()):
                    im = ico.ico.getimage(size)
                    got[size] = im.getpixel((0, 0))
            ok = got == expected
            print("%s: %s (expected %s)" % (name, got, expected))
            if not ok:
                failures += 1
        finally:
            try:
                os.remove(path)
            except OSError:
                pass
            for image in images:
                free(image)

    # non-exact downscale: 64 base + 16 + 48 appends; 32 -> 48 thumbnail
    run_case("downscale",
             [make_image(lib, 64, 64, 64), make_image(lib, 16, 16, 16), make_image(lib, 48, 48, 48)],
             [(16, 16), (32, 32), (48, 48), (64, 64)],
             {(16, 16): (16, 0, 0, 255), (32, 32): (48, 0, 0, 255),
              (48, 48): (48, 0, 0, 255), (64, 64): (64, 0, 0, 255)})

    # aspect: last append 64x32 -> 32x16 and 16x8 fallback entries
    run_case("aspect",
             [make_image(lib, 64, 64, 64), make_image(lib, 64, 32, 32)],
             [(16, 16), (32, 32), (64, 64)],
             {(16, 8): (32, 0, 0, 255), (32, 16): (32, 0, 0, 255), (64, 64): (64, 0, 0, 255)})

    # smaller last: 24x24 stays 24x24 for the 32 request, 16 for the 16
    run_case("small-last",
             [make_image(lib, 64, 64, 64), make_image(lib, 24, 24, 24)],
             [(16, 16), (32, 32), (64, 64)],
             {(16, 16): (24, 0, 0, 255), (24, 24): (24, 0, 0, 255), (64, 64): (64, 0, 0, 255)})

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
