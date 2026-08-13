"""MODE-NUM-001CH cross-check: DLL affine transform -> Pillow parity.

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
    transform = lib.pillow_c_image_transform_affine
    transform.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                          ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                          ctypes.POINTER(ctypes.c_void_p)]
    transform.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    matrix = (ctypes.c_double * 6)(1.0, 0.0, 0.5, 0.0, 1.0, 0.5)

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
            out = ctypes.c_void_p()
            status = transform(image, 4, 3, matrix, resample, None, 0, ctypes.byref(out))
            assert status == 0, (label, resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<12%s" % fmt[1:], bytes(out_ptr[:48]))
            expected = list(Image.new(pil_mode, (3, 2)) and
                            _pillow_transform(pil_mode, values, pil_resample))
            ok = list(got) == expected
            print("%s resample=%s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        # Fill: scale-2 matrix pushes most pixels out of bounds.
        scale2 = (ctypes.c_double * 6)(2.0, 0.0, 0.0, 0.0, 2.0, 0.0)
        if mode == MODE_I:
            fill = struct.pack("<i", -33)
            fill_pil = -33
        else:
            fill = struct.pack("<f", 7.5)
            fill_pil = 7.5
        fill_buf = (ctypes.c_uint8 * 4).from_buffer_copy(fill)
        out = ctypes.c_void_p()
        assert transform(image, 4, 3, scale2, 0, fill_buf, 4, ctypes.byref(out)) == 0
        out_ptr = ctypes.POINTER(ctypes.c_uint8)()
        out_size = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
        got = struct.unpack("<12%s" % fmt[1:], bytes(out_ptr[:48]))
        expected = _pillow_transform_fill(pil_mode, values, fill_pil)
        ok = list(got) == expected
        print("%s fill=%s: %s (expected %s)" % (label, fill_pil, list(got), expected))
        if not ok:
            failures += 1
        free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_transform(mode, values, resample):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.transform((4, 3), Image.AFFINE, (1.0, 0.0, 0.5, 0.0, 1.0, 0.5), resample=resample)
    return list(out.getdata())


def _pillow_transform_fill(mode, values, fill):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.transform((4, 3), Image.AFFINE, (2.0, 0.0, 0.0, 0.0, 2.0, 0.0),
                       resample=Image.Resampling.NEAREST, fillcolor=fill)
    return list(out.getdata())


if __name__ == "__main__":
    main()
