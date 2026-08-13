"""MODE-NUM-001CL cross-check: DLL boxed resize -> Pillow parity.

Mode I/F boxed resize plus the resize behind thumbnail (facade computes
the aspect size, then routes through the plain resize export).
"""

import ctypes
import os
import struct

from PIL import Image

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)

MODE_I = 8
MODE_F = 9

I_VALUES = [1000, -2000, 3000, 7]
F_VALUES = [1.5, -2.5, 3.5, 0.25]

I_BIG = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200]
F_BIG = [0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2]


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    resize = lib.pillow_c_image_resize
    resize.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    resize.restype = ctypes.c_int
    resize_box = lib.pillow_c_image_resize_box
    resize_box.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                           ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double,
                           ctypes.POINTER(ctypes.c_void_p)]
    resize_box.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    for label, mode, values, fmt, pil_mode in [
        ("I", MODE_I, I_VALUES, "<i", "I"),
        ("F", MODE_F, F_VALUES, "<f", "F"),
    ]:
        image = ctypes.c_void_p()
        assert create(2, 2, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        ctypes.memmove(ptr, struct.pack("<4%s" % fmt[1:], *values), 16)

        for resample, pil_resample in [
            (0, Image.Resampling.NEAREST),
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            out = ctypes.c_void_p()
            status = resize_box(image, 3, 3, resample, 0.5, 0.5, 1.5, 1.5, ctypes.byref(out))
            assert status == 0, (label, resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<9%s" % fmt[1:], bytes(out_ptr[:36]))
            expected = _pillow_boxed(pil_mode, values, pil_resample)
            ok = list(got) == expected
            print("%s BOX %s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        free(image)

    # Thumbnail path: 4x3 -> 2x2 through the plain resize export.
    for label, mode, values, fmt, pil_mode in [
        ("I", MODE_I, I_BIG, "<i", "I"),
        ("F", MODE_F, F_BIG, "<f", "F"),
    ]:
        image = ctypes.c_void_p()
        assert create(4, 3, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        ctypes.memmove(ptr, struct.pack("<12%s" % fmt[1:], *values), 48)

        for resample, pil_resample in [
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            out = ctypes.c_void_p()
            status = resize(image, 2, 2, resample, ctypes.byref(out))
            assert status == 0, (label, resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<4%s" % fmt[1:], bytes(out_ptr[:16]))
            expected = _pillow_thumb(pil_mode, values, pil_resample)
            ok = list(got) == expected
            print("%s THUMB %s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_boxed(mode, values, resample):
    im = Image.new(mode, (2, 2))
    im.putdata(values)
    out = im.resize((3, 3), resample=resample, box=(0.5, 0.5, 1.5, 1.5))
    return list(out.getdata())


def _pillow_thumb(mode, values, resample):
    im = Image.new(mode, (4, 3))
    im.putdata(values)
    im.thumbnail((2, 2), resample=resample)
    return list(im.getdata())


if __name__ == "__main__":
    main()
