"""API-MATH-001 cross-check: DLL ImageMath RPN -> Pillow ImageMath parity."""

import ctypes
import os
import struct

from PIL import Image, ImageMath

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)

MODE_L = 1

A = [10, 20, 30, 40]
B = [2, 5, 3, 4]

# Expression -> (program, slots, constants, constant_floats)
CASES = [
    ("a + b", [1, 1, 1, 2, 2], [0, 0], [], []),
    ("a - b", [1, 1, 1, 2, 3], [0, 0], [], []),
    ("a * b", [1, 1, 1, 2, 4], [0, 0], [], []),
    ("a / b", [1, 1, 1, 2, 5], [0, 0], [], []),
    ("a & b", [1, 1, 1, 2, 7], [0, 0], [], []),
    ("a | b", [1, 1, 1, 2, 8], [0, 0], [], []),
    ("a ^ b", [1, 1, 1, 2, 9], [0, 0], [], []),
    ("min(a, b)", [1, 1, 1, 2, 21], [0, 0], [], []),
    ("max(a, b)", [1, 1, 1, 2, 22], [0, 0], [], []),
    ("abs(a - b)", [1, 1, 1, 2, 3, 20], [0, 0], [], []),
    ("a >> 1", [1, 1, 1, 2, 11], [0, 1], [1.0], [0]),
    ("a << 2", [1, 1, 1, 2, 10], [0, 1], [2.0], [0]),
    ("float(a) * b", [1, 1, 23, 1, 2, 4], [0, 0], [], []),
    ("a + 1", [1, 1, 1, 2, 2], [0, 1], [1.0], [0]),
    ("a * 3", [1, 1, 1, 2, 4], [0, 1], [3.0], [0]),
    ("-a", [1, 1, 18], [0], [], []),
    ("convert(a, 'F')", [1, 1, 25, 9], [0], [], []),
    ("a == b", [1, 1, 1, 2, 12], [0, 0], [], []),
    ("a < b", [1, 1, 1, 2, 14], [0, 0], [], []),
    ("a >= b", [1, 1, 1, 2, 17], [0, 0], [], []),
]


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    rpn = lib.pillow_c_image_math_rpn
    rpn.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_uint8),
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                    ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                    ctypes.POINTER(ctypes.c_void_p)]
    rpn.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    img_a = ctypes.c_void_p()
    assert create(2, 2, MODE_L, ctypes.byref(img_a)) == 0
    img_b = ctypes.c_void_p()
    assert create(2, 2, MODE_L, ctypes.byref(img_b)) == 0
    pa = ctypes.POINTER(ctypes.c_uint8)()
    sa = ctypes.c_size_t()
    assert data_fn(img_a, ctypes.byref(pa), ctypes.byref(sa)) == 0
    ctypes.memmove(pa, bytes(A), 4)
    pb = ctypes.POINTER(ctypes.c_uint8)()
    sb = ctypes.c_size_t()
    assert data_fn(img_b, ctypes.byref(pb), ctypes.byref(sb)) == 0
    ctypes.memmove(pb, bytes(B), 4)

    failures = 0
    pillow_a = Image.new("L", (2, 2))
    pillow_a.putdata(A)
    pillow_b = Image.new("L", (2, 2))
    pillow_b.putdata(B)

    for expr, program, kinds, constants, cfloats in CASES:
        slot_count = len(kinds)
        images = (ctypes.c_void_p * slot_count)()
        images[0] = img_a
        if slot_count > 1:
            images[1] = img_b
        constants_arr = (ctypes.c_double * slot_count)(*([0.0] * (slot_count - len(constants)) + constants))
        cfloats_arr = (ctypes.c_uint8 * slot_count)(*([0] * (slot_count - len(cfloats)) + cfloats))
        kinds_arr = (ctypes.c_uint8 * slot_count)(*kinds)
        program_arr = (ctypes.c_uint8 * len(program))(*program)
        out = ctypes.c_void_p()
        status = rpn(images, constants_arr, cfloats_arr, kinds_arr, slot_count,
                     program_arr, len(program), ctypes.byref(out))
        assert status == 0, (expr, status)
        p = ctypes.POINTER(ctypes.c_uint8)()
        s = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(p), ctypes.byref(s)) == 0
        raw = bytes(p[:16])
        expected = ImageMath.eval(expr, a=pillow_a, b=pillow_b)
        expected_raw = expected.tobytes()
        ok = raw == expected_raw
        got_i = struct.unpack("<4i", raw[:16])
        got_f = struct.unpack("<4f", raw[:16])
        print("%s: i=%s f=%s (expected %s %s)" % (
            expr, list(got_i), list(got_f), expected.mode, list(expected.getdata())))
        if not ok:
            failures += 1
        free(out)

    free(img_a)
    free(img_b)
    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
