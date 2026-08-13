"""MODE-NUM-001CM cross-check: DLL I;16 resize/transform -> Pillow parity.

Mode I;16 (little-endian uint16) resize parity, NEAREST transform parity,
and the documented bilinear/bicubic transform + I;16B filter boundaries.
"""

import ctypes
import os
import struct

from PIL import Image

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)

MODE_I16 = 11
MODE_I16B = 12

VALUES = [1000, 50000, 60000, 300, 200, 65535]
INVALID = -3


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
    transform = lib.pillow_c_image_transform_affine
    transform.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                          ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                          ctypes.POINTER(ctypes.c_void_p)]
    transform.restype = ctypes.c_int
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
    payload = struct.pack("<6H", *VALUES)

    for label, mode, fmt, pil_mode in [
        ("I;16", MODE_I16, "<H", "I;16"),
        ("I;16B", MODE_I16B, ">H", "I;16B"),
    ]:
        image = ctypes.c_void_p()
        assert create(3, 2, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        ctypes.memmove(ptr, struct.pack("<6H", *VALUES), 12)

        # Resize: parity for I;16, explicit boundary for I;16B filters.
        for resample, pil_resample in [
            (0, Image.Resampling.NEAREST),
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            out = ctypes.c_void_p()
            status = resize(image, 4, 3, resample, ctypes.byref(out))
            if label == "I;16B" and resample != 0:
                ok = status == INVALID
                print("%s RESIZE %s: boundary status %s (expected %s)" % (
                    label, pil_resample.name, status, INVALID))
            else:
                assert status == 0, (label, resample, status)
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                got = struct.unpack("<12%s" % fmt[1:], bytes(out_ptr[:24]))
                expected = _pillow_resize(pil_mode, values=VALUES, resample=pil_resample)
                ok = list(got) == expected
                print("%s RESIZE %s: %s (expected %s)" % (
                    label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            if out:
                free(out)
                out = ctypes.c_void_p()

        # AFFINE: NEAREST parity; bilinear/bicubic documented boundaries.
        matrix = (ctypes.c_double * 6)(1.0, 0.0, 0.5, 0.0, 1.0, 0.5)
        for resample, pil_resample in [
            (0, Image.Resampling.NEAREST),
            (2, Image.Resampling.BILINEAR),
            (3, Image.Resampling.BICUBIC),
        ]:
            out = ctypes.c_void_p()
            status = transform(image, 4, 3, matrix, resample, None, 0, ctypes.byref(out))
            if resample != 0:
                ok = status == INVALID
                print("%s AFFINE %s: boundary status %s (expected %s)" % (
                    label, pil_resample.name, status, INVALID))
            else:
                assert status == 0, (label, resample, status)
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                got = struct.unpack("<12%s" % fmt[1:], bytes(out_ptr[:24]))
                expected = _pillow_affine_nearest(pil_mode)
                ok = list(got) == expected
                print("%s AFFINE %s: %s (expected %s)" % (
                    label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            if out:
                free(out)
                out = ctypes.c_void_p()

        # Rotate NEAREST parity; bilinear documented boundary.
        out = ctypes.c_void_p()
        status = rotate(image, 45.0, 0, 0, 0.0, 0.0, 0, 0.0, 0.0, 0, None, 0, ctypes.byref(out))
        assert status == 0, (label, status)
        out_ptr = ctypes.POINTER(ctypes.c_uint8)()
        out_size = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
        got = struct.unpack("<6%s" % fmt[1:], bytes(out_ptr[:12]))
        expected = _pillow_rotate_nearest(pil_mode)
        ok = list(got) == expected
        print("%s ROT45 NEAREST: %s (expected %s)" % (label, list(got), expected))
        if not ok:
            failures += 1
        free(out)

        out = ctypes.c_void_p()
        status = rotate(image, 45.0, 2, 0, 0.0, 0.0, 0, 0.0, 0.0, 0, None, 0, ctypes.byref(out))
        ok = status == INVALID
        print("%s ROT45 BILINEAR: boundary status %s (expected %s)" % (label, status, INVALID))
        if not ok:
            failures += 1
        if out:
            free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_resize(mode, values, resample):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.resize((4, 3), resample=resample)
    return list(out.getdata())


def _pillow_affine_nearest(mode):
    im = Image.new(mode, (3, 2))
    im.putdata(VALUES)
    out = im.transform((4, 3), Image.AFFINE, (1.0, 0.0, 0.5, 0.0, 1.0, 0.5),
                       resample=Image.Resampling.NEAREST)
    return list(out.getdata())


def _pillow_rotate_nearest(mode):
    im = Image.new(mode, (3, 2))
    im.putdata(VALUES)
    out = im.rotate(45.0, resample=Image.Resampling.NEAREST)
    return list(out.getdata())


if __name__ == "__main__":
    main()
