"""MODE-I-001B cross-check: DLL point_transform -> Pillow parity.

Verifies the native mode-I linear transform against Pillow 11.3.0's
point_transform path.
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

MODE_I = 8


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
    samples = [-1, 7, 300, 0]
    payload = struct.pack("<4i", *samples)
    image = ctypes.c_void_p()
    assert create(2, 2, MODE_I, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, payload, len(payload))

    for label, scale, offset, expected in [
        ("identity", 1.0, 0.0, samples),
        ("2x+5", 2.0, 5.0, [2 * s + 5 for s in samples]),
        ("x-1000", 1.0, -1000.0, [s - 1000 for s in samples]),
        ("negate", -1.0, 0.0, [-s for s in samples]),
        ("constant", 0.0, 42.0, [42] * 4),
    ]:
        out = ctypes.c_void_p()
        status = transform(image, scale, offset, ctypes.byref(out))
        assert status == 0, (label, status)
        data_ptr = ctypes.POINTER(ctypes.c_uint8)()
        data_size = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(data_ptr), ctypes.byref(data_size)) == 0
        got = struct.unpack("<4i", bytes(data_ptr[:16]))[:4]
        ok = list(got) == expected
        print("%s: %s (expected %s)" % (label, list(got), expected))
        if not ok:
            failures += 1
        free(out)

    # Pillow parity spot check
    im = Image.new("I", (2, 2))
    im.putdata(samples)
    pillow_out = list(im.point(lambda x: 2 * x + 5).getdata())
    print("pillow 2x+5:", pillow_out)
    if pillow_out != [2 * s + 5 for s in samples]:
        failures += 1

    # wrong-mode rejection: L source -> -3
    l = ctypes.c_void_p()
    assert create(2, 2, 1, ctypes.byref(l)) == 0
    out = ctypes.c_void_p()
    reject_status = transform(l, 1.0, 0.0, ctypes.byref(out))
    print("L rejection:", reject_status)
    if reject_status != -3:
        failures += 1
    free(l)

    free(image)
    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
