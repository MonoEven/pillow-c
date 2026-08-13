"""FMT-TIFF-003BH cross-check: DLL BigTIFF P-mode save -> Pillow reopen.

Verifies the palette BigTIFF save (photometric 3, ColorMap 320) against
the Pillow 11.3.0 authority, plus the DLL reopen of Pillow's P BigTIFF.
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

MODE_P = 6


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    put_palette = lib.pillow_c_image_put_palette_rgb
    put_palette.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
    put_palette.restype = ctypes.c_int
    save = lib.pillow_c_image_save_tiff_bigtiff
    save.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    save.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    palette = bytes([0, 0, 0, 255, 0, 0, 0, 255, 0, 255, 255, 255])
    indices = bytes([0, 1, 2, 3])

    image = ctypes.c_void_p()
    assert create(2, 2, MODE_P, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    ctypes.memmove(ptr, indices, 4)
    palette_buf = (ctypes.c_uint8 * len(palette)).from_buffer_copy(palette)
    assert put_palette(image, palette_buf, len(palette)) == 0
    path = os.path.join(tempfile.gettempdir(), "dll-bigtiff-p.tif")
    try:
        assert save(image, path.encode("utf-8")) == 0
        data = open(path, "rb").read()
        is_bigtiff = data[:2] == b"II" and struct.unpack("<H", data[2:4])[0] == 43
        with Image.open(path) as reopened:
            ok = (
                is_bigtiff
                and reopened.mode == "P"
                and reopened.size == (2, 2)
                and reopened.tobytes() == indices
            )
            got_palette = list(reopened.getpalette())[:12]
            ok = ok and got_palette == list(palette)
        print("P save: bigtiff=%s mode/indices/palette_ok=%s" % (is_bigtiff, ok))
        if not ok:
            failures += 1
            print("  palette:", got_palette)
    finally:
        try:
            os.remove(path)
        except OSError:
            pass
        free(image)

    # DLL reopen of Pillow's own P BigTIFF
    path = os.path.join(tempfile.gettempdir(), "pillow-bigtiff-p.tif")
    try:
        im = Image.new("P", (2, 2), 0)
        im.putpalette(list(palette))
        im.putdata(list(indices))
        im.save(path, "TIFF", big_tiff=True)
        open_fn = lib.pillow_c_image_open_tiff
        open_fn.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
        open_fn.restype = ctypes.c_int
        get_palette = lib.pillow_c_image_get_palette_rgb
        get_palette.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        get_palette.restype = ctypes.c_int
        loaded = ctypes.c_void_p()
        assert open_fn(path.encode("utf-8"), ctypes.byref(loaded)) == 0
        data_ptr = ctypes.POINTER(ctypes.c_uint8)()
        data_size = ctypes.c_size_t()
        assert data_fn(loaded, ctypes.byref(data_ptr), ctypes.byref(data_size)) == 0
        indices_ok = bytes(data_ptr[:data_size.value]) == indices
        req = ctypes.c_size_t(768)
        palette_out = (ctypes.c_uint8 * 768)()
        assert get_palette(loaded, palette_out, 768, ctypes.byref(req)) == 0
        palette_ok = bytes(palette_out[:12]) == palette
        print("P open: indices_ok=%s palette_ok=%s" % (indices_ok, palette_ok))
        if not (indices_ok and palette_ok):
            failures += 1
        free(loaded)
    finally:
        try:
            os.remove(path)
        except OSError:
            pass

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
