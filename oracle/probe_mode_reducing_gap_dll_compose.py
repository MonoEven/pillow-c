"""MODE-NUM-001CN cross-check: DLL reducing-gap resize -> Pillow parity.

Mode I/F reduce+resize parity and the I;16 reduce boundary.
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
MODE_I16 = 11

I_VALUES = [i * 37 % 4000 for i in range(24 * 24)]
F_VALUES = [((i * 13) % 100) / 7.0 - 3.0 for i in range(24 * 24)]
I16_VALUES = [(i * 977) % 60000 for i in range(24 * 24)]

INVALID = -3


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    resize_gap = lib.pillow_c_image_resize_reducing_gap
    resize_gap.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                           ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double,
                           ctypes.c_double, ctypes.POINTER(ctypes.c_void_p)]
    resize_gap.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    for label, mode, values, fmt, pil_mode in [
        ("I", MODE_I, I_VALUES, "<i", "I"),
        ("F", MODE_F, F_VALUES, "<f", "F"),
    ]:
        image = ctypes.c_void_p()
        assert create(24, 24, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        ctypes.memmove(ptr, struct.pack("<576%s" % fmt[1:], *values), 576 * 4)

        for resample, pil_resample in [
            (0, Image.Resampling.NEAREST),
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            out = ctypes.c_void_p()
            status = resize_gap(image, 3, 3, resample, 0.0, 0.0, 24.0, 24.0, 2.0, ctypes.byref(out))
            assert status == 0, (label, resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<9%s" % fmt[1:], bytes(out_ptr[:36]))
            expected = _pillow_gap(pil_mode, values, pil_resample)
            ok = list(got) == expected
            print("%s GAP %s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        free(image)

    # I;16: NEAREST parity (no reduce step), bilinear boundary.
    image = ctypes.c_void_p()
    assert create(24, 24, MODE_I16, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, struct.pack("<576H", *I16_VALUES), 576 * 2)

    out = ctypes.c_void_p()
    status = resize_gap(image, 3, 3, 0, 0.0, 0.0, 24.0, 24.0, 2.0, ctypes.byref(out))
    assert status == 0, status
    out_ptr = ctypes.POINTER(ctypes.c_uint8)()
    out_size = ctypes.c_size_t()
    assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
    got = struct.unpack("<9H", bytes(out_ptr[:18]))
    expected = _pillow_gap("I;16", I16_VALUES, Image.Resampling.NEAREST)
    ok = list(got) == expected
    print("I;16 GAP NEAREST: %s (expected %s)" % (list(got), expected))
    if not ok:
        failures += 1
    free(out)

    out = ctypes.c_void_p()
    status = resize_gap(image, 3, 3, 2, 0.0, 0.0, 24.0, 24.0, 2.0, ctypes.byref(out))
    ok = status == INVALID
    print("I;16 GAP BILINEAR: boundary status %s (expected %s)" % (status, INVALID))
    if not ok:
        failures += 1
    if out:
        free(out)
    free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_gap(mode, values, resample):
    im = Image.new(mode, (24, 24))
    im.putdata(values)
    out = im.resize((3, 3), resample=resample, reducing_gap=2.0)
    return list(out.getdata())


if __name__ == "__main__":
    main()
