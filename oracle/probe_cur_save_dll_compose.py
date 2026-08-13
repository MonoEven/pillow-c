"""FMT-ICO-002G cross-check: DLL CUR save -> Pillow reopen.

Pillow 11.3.0 registers no CUR save, so the DLL-written CUR (ICO type-2
container with hotspot fields) is verified through Pillow's CUR reader
and our own open_cur hotspot metadata.
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


def main():
    lib = ctypes.CDLL(DLL)
    create = lib.pillow_c_image_create_mode
    create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
    create.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    save = lib.pillow_c_image_save_cur_options
    save.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    save.restype = ctypes.c_int
    open_cur = lib.pillow_c_image_open_cur
    open_cur.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    open_cur.restype = ctypes.c_int
    hotspot = lib.pillow_c_image_metadata_hotspot
    hotspot.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int)]
    hotspot.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0
    image = ctypes.c_void_p()
    assert create(16, 16, 4, ctypes.byref(image)) == 0
    ptr = ctypes.POINTER(ctypes.c_uint8)()
    size = ctypes.c_size_t()
    assert data_fn(image, ctypes.byref(ptr), ctypes.byref(size)) == 0
    payload = bytes([255, 0, 0, 255]) * (16 * 16)
    ctypes.memmove(ptr, payload, len(payload))
    path = os.path.join(tempfile.gettempdir(), "dll-cur.cur")
    try:
        assert save(image, path.encode("utf-8"), 1, 5, 7) == 0
        data = open(path, "rb").read()
        ok = data[:4] == b"\x00\x00\x02\x00"
        # entry header: width, height, 0, 0, planes=5, bits=7
        entry = data[6:22]
        ok = ok and entry[0] == 16 and entry[1] == 16
        ok = ok and struct.unpack("<H", entry[4:6])[0] == 5
        ok = ok and struct.unpack("<H", entry[6:8])[0] == 7
        with Image.open(path) as reopened:
            ok = ok and reopened.format == "CUR"
            ok = ok and reopened.size == (16, 16)
            ok = ok and reopened.tobytes()[:4] == b"\xff\x00\x00\xff"
        loaded = ctypes.c_void_p()
        assert open_cur(path.encode("utf-8"), ctypes.byref(loaded)) == 0
        has = ctypes.c_int()
        x = ctypes.c_int()
        y = ctypes.c_int()
        assert hotspot(loaded, ctypes.byref(has), ctypes.byref(x), ctypes.byref(y)) == 0
        ok = ok and has.value == 1 and x.value == 5 and y.value == 7
        free(loaded)
        print("CUR save/reopen: magic/entry/pillow/our-hotspot_ok=%s" % ok)
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
