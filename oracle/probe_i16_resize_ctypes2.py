"""ctypes probe v2: set_bytes vs set_raw_bytes I;16 resize paths."""

import ctypes
import struct

dll = ctypes.WinDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")

dll.pillow_c_image_create_mode.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_create_mode.restype = ctypes.c_int
dll.pillow_c_image_set_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_set_bytes.restype = ctypes.c_int
dll.pillow_c_image_get_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_get_bytes.restype = ctypes.c_int
dll.pillow_c_image_set_raw_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
dll.pillow_c_image_set_raw_bytes.restype = ctypes.c_int
dll.pillow_c_image_resize.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_resize.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]

buf = struct.pack("<16H", 0, 100, 200, 300, 0, 100, 200, 300, 0, 100, 200, 300, 0, 100, 200, 300)
b = ctypes.create_string_buffer(buf)
for setup in ["set_bytes", "set_raw"]:
    h = ctypes.c_void_p()
    assert dll.pillow_c_image_create_mode(4, 4, 11, ctypes.byref(h)) == 0
    if setup == "set_bytes":
        assert dll.pillow_c_image_set_bytes(h, b, len(buf)) == 0
    else:
        status = dll.pillow_c_image_set_raw_bytes(h, b, len(buf), b"I;16", 0, 1)
        print(setup, "set_raw status", status)
    src = ctypes.create_string_buffer(32)
    dll.pillow_c_image_get_bytes(h, src, 32)
    print(setup, "src:", src.raw[:8].hex())
    out = ctypes.c_void_p()
    status = dll.pillow_c_image_resize(h, 2, 2, 3, ctypes.byref(out))
    print(setup, "resize status", status)
    o = ctypes.create_string_buffer(8)
    dll.pillow_c_image_get_bytes(out, o, 8)
    print(setup, "out:", o.raw.hex(), struct.unpack("<4H", o.raw))
    dll.pillow_c_image_free(out)
    dll.pillow_c_image_free(h)
