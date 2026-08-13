"""FMT-ICO-001C cross-check: DLL multi-source matrix -> Pillow reopen."""

import ctypes
import os
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
    ctypes.memmove(ptr, payload * (size.value // len(payload)), size.value)
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

    def run_case(name, images, sizes, bitmap_format, expected):
        path = os.path.join(tempfile.gettempdir(), "dll-ico-matrix-%s.ico" % name)
        try:
            handles = (ctypes.c_void_p * len(images))(*images)
            size_values = [v for size in sizes for v in size]
            size_buf = (ctypes.c_int * len(size_values))(*size_values)
            fmt = bitmap_format.encode() if bitmap_format else None
            status = save(handles, len(images), path.encode("utf-8"),
                          size_buf, len(sizes), 1, fmt)
            assert status == 0, (name, status)
            with Image.open(path) as ico:
                got = {}
                for size in sorted(ico.ico.sizes()):
                    im = ico.ico.getimage(size)
                    got[size] = (im.mode, im.getpixel((0, 0)))
            ok = got == expected
            print("%s: %s" % (name, got))
            if not ok:
                failures += 1
                print("  expected:", expected)
        finally:
            try:
                os.remove(path)
            except OSError:
                pass
            for image in images:
                free(image)

    # mixed modes: RGB base + L + RGBA appends
    run_case("mixed",
             [make_image(lib, 32, 32, 3, b"\xc8\x00\x00"),
              make_image(lib, 16, 16, 1, b"\x10"),
              make_image(lib, 24, 24, 4, b"\x00\x00\xff\xff")],
             [(16, 16), (24, 24), (32, 32)], None,
             {(16, 16): ("L", 16), (24, 24): ("RGBA", (0, 0, 255, 255)),
              (32, 32): ("RGB", (200, 0, 0))})

    # same-size PNG: first wins
    run_case("samesize",
             [make_image(lib, 32, 32, 4, b"\xff\x00\x00\xff"),
              make_image(lib, 16, 16, 4, b"\x00\xff\x00\xff"),
              make_image(lib, 16, 16, 4, b"\x00\x00\xff\xff")],
             [(16, 16), (32, 32)], None,
             {(16, 16): ("RGBA", (0, 255, 0, 255)), (32, 32): ("RGBA", (255, 0, 0, 255))})

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
