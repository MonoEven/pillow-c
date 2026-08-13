"""MODE-NUM-001CI cross-check: DLL rotate -> Pillow parity.

Mode I/F per-sample interpolation (NEAREST/BILINEAR/BICUBIC) and fill.
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

I_VALUES = [1000, -2000, 3000, 7, -8, 9]
F_VALUES = [1.5, -2.5, 3.5, 0.25, -0.125, 2.0]


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    rotate = lib.pillow_c_image_rotate
    rotate.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_int, ctypes.c_int,
                       ctypes.c_double, ctypes.c_double, ctypes.c_int,
                       ctypes.c_double, ctypes.c_double, ctypes.c_int,
                       ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                       ctypes.POINTER(ctypes.c_void_p)]
    rotate.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    for label, mode, values, fmt, pil_mode in [
        ("I", MODE_I, I_VALUES, "<i", "I"),
        ("F", MODE_F, F_VALUES, "<f", "F"),
    ]:
        image = ctypes.c_void_p()
        assert create(3, 2, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        ctypes.memmove(ptr, struct.pack("<6%s" % fmt[1:], *values), 24)

        for resample, pil_resample in [
            (0, Image.Resampling.NEAREST),
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            for expand in (0, 1):
                out = ctypes.c_void_p()
                status = rotate(image, 45.0, resample, expand,
                                0.0, 0.0, 0, 0.0, 0.0, 0, None, 0, ctypes.byref(out))
                assert status == 0, (label, resample, expand, status)
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                count = (5 * 4) if expand else (3 * 2)
                got = struct.unpack("<%d%s" % (count, fmt[1:]), bytes(out_ptr[:count * 4]))
                expected = _pillow_rotate(pil_mode, values, pil_resample, bool(expand))
                ok = list(got) == expected
                print("%s resample=%s expand=%s: %s (expected %s)" % (
                    label, pil_resample.name, bool(expand), list(got), expected))
                if not ok:
                    failures += 1
                free(out)

        # Fill: numeric scalar through the 4-byte ABI fill.
        if mode == MODE_I:
            fill = struct.pack("<i", -33)
            fill_pil = -33
        else:
            fill = struct.pack("<f", 7.5)
            fill_pil = 7.5
        fill_buf = (ctypes.c_uint8 * 4).from_buffer_copy(fill)
        out = ctypes.c_void_p()
        assert rotate(image, 45.0, 0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0,
                      fill_buf, 4, ctypes.byref(out)) == 0
        out_ptr = ctypes.POINTER(ctypes.c_uint8)()
        out_size = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
        got = struct.unpack("<6%s" % fmt[1:], bytes(out_ptr[:24]))
        expected = _pillow_rotate_fill(pil_mode, values, fill_pil)
        ok = list(got) == expected
        print("%s fill=%s: %s (expected %s)" % (label, fill_pil, list(got), expected))
        if not ok:
            failures += 1
        free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_rotate(mode, values, resample, expand):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.rotate(45.0, resample=resample, expand=expand)
    return list(out.getdata())


def _pillow_rotate_fill(mode, values, fill):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.rotate(45.0, resample=Image.Resampling.NEAREST, fillcolor=fill)
    return list(out.getdata())


if __name__ == "__main__":
    main()
