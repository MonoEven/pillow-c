"""MODE-NUM-001CK cross-check: DLL resize -> Pillow parity.

Mode I/F per-sample resize interpolation (NEAREST and all filter kernels).
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
            (1, Image.Resampling.LANCZOS),
            (4, Image.Resampling.BOX),
            (5, Image.Resampling.HAMMING),
        ]:
            out = ctypes.c_void_p()
            status = resize(image, 3, 3, resample, ctypes.byref(out))
            assert status == 0, (label, resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<9%s" % fmt[1:], bytes(out_ptr[:36]))
            expected = _pillow_resize(pil_mode, values, pil_resample)
            ok = list(got) == expected
            print("%s resample=%s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_resize(mode, values, resample):
    im = Image.new(mode, (2, 2))
    im.putdata(values)
    out = im.resize((3, 3), resample=resample)
    return list(out.getdata())


if __name__ == "__main__":
    main()
