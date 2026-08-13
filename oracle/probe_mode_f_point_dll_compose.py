"""MODE-F-001B cross-check: DLL F point_transform -> Pillow parity."""

import ctypes
import os
import struct
import tempfile

from PIL import Image

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)

MODE_F = 9


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    transform = lib.pillow_c_image_point_transform
    transform.argtypes = [ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.c_void_p)]
    transform.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    samples = [1.5, -2.5, 3.5, 0.0]
    payload = struct.pack("<4f", *samples)
    image = ctypes.c_void_p()
    assert create(2, 2, MODE_F, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, payload, len(payload))

    for label, scale, offset, expected in [
        ("identity", 1.0, 0.0, samples),
        ("2x+5", 2.0, 5.0, [2.0 * s + 5.0 for s in samples]),
        ("half", 0.5, 0.0, [0.5 * s for s in samples]),
        ("constant", 0.0, -1.5, [-1.5] * 4),
    ]:
        out = ctypes.c_void_p()
        status = transform(image, scale, offset, ctypes.byref(out))
        assert status == 0, (label, status)
        data_ptr = ctypes.POINTER(ctypes.c_uint8)()
        data_size = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(data_ptr), ctypes.byref(data_size)) == 0
        got = struct.unpack("<4f", bytes(data_ptr[:16]))[:4]
        ok = list(got) == expected
        print("%s: %s (expected %s)" % (label, list(got), expected))
        if not ok:
            failures += 1
        free(out)

    im = Image.new("F", (2, 2))
    im.putdata(samples)
    pillow_out = list(im.point(lambda x: 2 * x + 5).getdata())
    print("pillow 2x+5:", pillow_out)
    if pillow_out != [2.0 * s + 5.0 for s in samples]:
        failures += 1

    free(image)
    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
