"""FMT-TIFF-003BF cross-check: DLL BigTIFF exif patch -> Pillow reopen.

Patches a DLL-written plain BigTIFF through the new BigTIFF IFD0 exif
entries export and verifies every bounded family reopens in Pillow 11.3.0.
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


def build_int_array(values):
    return (ctypes.c_int * len(values))(*values)


def build_u32_array(values):
    return (ctypes.c_uint32 * len(values))(*values)


def build_i32_array(values):
    return (ctypes.c_int32 * len(values))(*values)


def build_size_array(values):
    return (ctypes.c_size_t * len(values))(*values)


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    save = lib.pillow_c_image_save_tiff_bigtiff
    save.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    save.restype = ctypes.c_int
    patch = lib.pillow_c_image_patch_tiff_bigtiff_exif_entries
    patch.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_int), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint32), ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint32), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_size_t), ctypes.c_size_t,
    ]
    patch.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    image = ctypes.c_void_p()
    assert create(2, 2, 1, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, bytes([7, 7, 7, 7]), 4)
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-exif-patch.tif")
    try:
        assert save(image, path.encode("utf-8")) == 0

        ascii_ptrs = []
        ascii_sizes = []
        for text in [b"desc-probe", b"artist"]:
            buf = ctypes.create_string_buffer(text + b"\0")
            ascii_ptrs.append(ctypes.cast(buf, ctypes.c_char_p))
            ascii_sizes.append(len(text) + 1)
        ascii_ptr_array = (ctypes.c_char_p * len(ascii_ptrs))(*ascii_ptrs)

        status = patch(
            path.encode("utf-8"),
            build_int_array([270, 315]),
            ascii_ptr_array,
            build_size_array(ascii_sizes),
            2,
            build_int_array([317]),
            build_u32_array([3]),
            build_int_array([3]),
            1,
            build_int_array([33434]),
            build_u32_array([1]),
            build_u32_array([2]),
            1,
            build_int_array([318]),
            build_u32_array([1, 3]),
            build_u32_array([2, 4]),
            2,
            build_size_array([0]),
            build_size_array([2]),
            1,
            build_int_array([34735]),
            build_u32_array([1, 2, 3]),
            3,
            build_size_array([0]),
            build_size_array([3]),
            1,
            build_int_array([]),
            (ctypes.c_uint8 * 0)(),
            0,
            build_size_array([]),
            build_size_array([]),
            0,
            build_int_array([]),
            build_u32_array([]),
            0,
            build_size_array([]),
            build_size_array([]),
            0,
            build_int_array([37380]),
            build_i32_array([3]),
            build_i32_array([7]),
            1,
            build_int_array([40091]),
            (ctypes.c_uint8 * 8)(*b"ASCII\x00\x00\x00"),
            8,
            build_size_array([0]),
            build_size_array([8]),
            1,
        )
        assert status == 0, ("patch", status)
        with open(path, "rb") as handle:
            data = handle.read()
        is_bigtiff = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            ok = (
                is_bigtiff
                and reopened.size == (2, 2)
                and reopened.tobytes() == b"\x07\x07\x07\x07"
            )
            exif = reopened.getexif()
            expected = {
                270: "desc-probe",
                315: "artist",
                317: 3,
                33434: (1, 2),
                318: (0.5, 0.75),
                34735: (1, 2, 3),
                37380: (3, 7),
                40091: b"ASCII\x00\x00\x00",
            }
            for tag, value in expected.items():
                got = exif.get(tag)
                if tag in (33434, 37380):
                    match = abs(float(got) - value[0] / value[1]) < 1e-9
                elif tag == 318:
                    match = len(got) == len(value) and all(
                        abs(float(item) - expected_item) < 1e-9
                        for item, expected_item in zip(got, value)
                    )
                else:
                    match = got == value
                ok = ok and match
                print("  exif[%d] = %r (match=%s)" % (tag, got, match))
            print("bigtiff=%s pixels/metadata_ok=%s" % (is_bigtiff, ok))
            if not ok:
                failures += 1
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(image)

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
