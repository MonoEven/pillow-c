"""MODE-NUM-001CJ cross-check: DLL perspective/quad/mesh -> Pillow parity.

Mode I/F per-sample interpolation for the three non-affine transforms.
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

PERSPECTIVE = (1.0, 0.05, 0.5, -0.05, 1.0, 0.5, 0.0005, 0.0005)
QUAD = (0.5, 0.5, 2.5, 0.5, 2.5, 1.5, 0.5, 1.5)
MESH_BOX = (0, 0, 2, 2)
MESH_QUAD = (0.0, 0.0, 2.5, 0.0, 2.5, 1.5, 0.0, 1.5)


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    perspective = lib.pillow_c_image_transform_perspective
    perspective.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                            ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                            ctypes.POINTER(ctypes.c_void_p)]
    perspective.restype = ctypes.c_int
    quad = lib.pillow_c_image_transform_quad
    quad.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double),
                     ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                     ctypes.POINTER(ctypes.c_void_p)]
    quad.restype = ctypes.c_int
    mesh = lib.pillow_c_image_transform_mesh
    mesh.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                     ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_double), ctypes.c_size_t,
                     ctypes.c_int, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
                     ctypes.POINTER(ctypes.c_void_p)]
    mesh.restype = ctypes.c_int

    failures = 0
    perspective_buf = (ctypes.c_double * 8)(*PERSPECTIVE)
    quad_buf = (ctypes.c_double * 8)(*QUAD)
    box_buf = (ctypes.c_int * 4)(*MESH_BOX)
    mesh_quad_buf = (ctypes.c_double * 8)(*MESH_QUAD)

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
            for method, fn, arg, pil_method, pil_data in [
                ("PERSPECTIVE", perspective, perspective_buf, Image.PERSPECTIVE, PERSPECTIVE),
                ("QUAD", quad, quad_buf, Image.QUAD, QUAD),
            ]:
                out = ctypes.c_void_p()
                status = fn(image, 3, 2, arg, resample, None, 0, ctypes.byref(out))
                assert status == 0, (label, method, resample, status)
                out_ptr = ctypes.POINTER(ctypes.c_uint8)()
                out_size = ctypes.c_size_t()
                assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
                got = struct.unpack("<6%s" % fmt[1:], bytes(out_ptr[:24]))
                expected = _pillow_transform(pil_mode, values, pil_method, pil_data, pil_resample)
                ok = list(got) == expected
                print("%s %s %s: %s (expected %s)" % (
                    label, method, pil_resample.name, list(got), expected))
                if not ok:
                    failures += 1
                free(out)

            out = ctypes.c_void_p()
            status = mesh(image, 3, 2, box_buf, mesh_quad_buf, 1, resample, None, 0, ctypes.byref(out))
            assert status == 0, (label, "MESH", resample, status)
            out_ptr = ctypes.POINTER(ctypes.c_uint8)()
            out_size = ctypes.c_size_t()
            assert data_fn(out, ctypes.byref(out_ptr), ctypes.byref(out_size)) == 0
            got = struct.unpack("<6%s" % fmt[1:], bytes(out_ptr[:24]))
            expected = _pillow_transform(
                pil_mode, values, Image.MESH, [(MESH_BOX, MESH_QUAD)], pil_resample)
            ok = list(got) == expected
            print("%s MESH %s: %s (expected %s)" % (
                label, pil_resample.name, list(got), expected))
            if not ok:
                failures += 1
            free(out)

        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


def _pillow_transform(mode, values, method, data, resample):
    im = Image.new(mode, (3, 2))
    im.putdata(values)
    out = im.transform((3, 2), method, data, resample=resample)
    return list(out.getdata())


if __name__ == "__main__":
    main()
