"""MODE-NUM-001CP cross-check: DLL I;16 extrema/convert -> Pillow parity."""

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
MODE_L = 1
MODE_I16 = 11
MODE_I16B = 12

VALUES = [1000, 50000, 60000, 300, 200, 65535]


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    extrema = lib.pillow_c_image_get_extrema_numeric
    extrema.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.POINTER(ctypes.c_double),
                        ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    extrema.restype = ctypes.c_int
    convert = lib.pillow_c_image_convert_mode
    convert.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    convert.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    for label, mode, fmt, pil_mode in [
        ("I;16", MODE_I16, "<H", "I;16"),
        ("I;16B", MODE_I16B, ">H", "I;16B"),
    ]:
        image = ctypes.c_void_p()
        assert create(3, 2, mode, ctypes.byref(image)) == 0
        ptr = ctypes.POINTER(ctypes.c_uint8)()
        size = ctypes.c_size_t()
        assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
        # mode-12 raw ABI stores bytes as loaded; load big-endian bytes for I;16B.
        raw = b"".join(struct.pack(fmt, v) for v in VALUES)
        ctypes.memmove(ptr, raw, 12)

        out_min = ctypes.c_double()
        out_max = ctypes.c_double()
        has = ctypes.c_uint8()
        status = extrema(image, ctypes.byref(out_min), ctypes.byref(out_max), ctypes.byref(has), 1)
        expected = Image.new(pil_mode, (3, 2))
        expected.putdata(VALUES)
        if label == "I;16B":
            ok = status == -3
            print("%s extrema: boundary status %s (expected -3)" % (label, status))
        else:
            assert status == 0
            got = (int(out_min.value), int(out_max.value))
            ok = got == expected.getextrema()
            print("%s extrema: %s (expected %s)" % (label, got, expected.getextrema()))
        if not ok:
            failures += 1

        for target_mode, target_mode_name, struct_fmt in [
            (MODE_I, "I", "<i"),
            (MODE_F, "F", "<f"),
            (MODE_L, "L", "B"),
        ]:
            pillow_expected = list(expected.convert(target_mode_name).getdata())
            if target_mode_name == "L":
                expected_out = list(expected.convert("L").tobytes())
                out = ctypes.c_void_p()
                assert convert(image, target_mode, ctypes.byref(out)) == 0
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                got_out = list(bytes(out_ptr[:6]))
            else:
                expected_out = pillow_expected
                out = ctypes.c_void_p()
                assert convert(image, target_mode, ctypes.byref(out)) == 0
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                got_out = [struct.unpack(struct_fmt, bytes(out_ptr[o:o + 4]))[0] for o in range(0, 24, 4)]
            ok = got_out == expected_out
            print("%s convert %s: %s (expected %s)" % (label, target_mode_name, got_out, expected_out))
            if not ok:
                failures += 1
            free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
