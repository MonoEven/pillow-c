"""ctypes probe: native I;16 resize 4x4->2x2 BICUBIC on [0,100,200,300]."""

import ctypes
import struct

dll = ctypes.WinDLL(r"D:\Tech\Projects\Autohotkey\Lib\visual_studio\tasks\2026-06-07-pillow-c-foundation\build\x64\Release\pillow_c.dll")

dll.pillow_c_image_create_mode.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_create_mode.restype = ctypes.c_int
dll.pillow_c_image_set_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_set_bytes.restype = ctypes.c_int
dll.pillow_c_image_resize.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_resize.restype = ctypes.c_int
dll.pillow_c_image_get_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_get_bytes.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]

h = ctypes.c_void_p()
assert dll.pillow_c_image_create_mode(4, 4, 11, ctypes.byref(h)) == 0
buf = struct.pack("<16H", 0, 100, 200, 300, 0, 100, 200, 300, 0, 100, 200, 300, 0, 100, 200, 300)
b = ctypes.create_string_buffer(buf)
assert dll.pillow_c_image_set_bytes(h, b, len(buf)) == 0
out = ctypes.c_void_p()
status = dll.pillow_c_image_resize(h, 2, 2, 3, ctypes.byref(out))
print("status", status)
o = ctypes.create_string_buffer(8)
dll.pillow_c_image_get_bytes(out, o, 8)
print("out:", o.raw.hex(), struct.unpack("<4H", o.raw))
dll.pillow_c_image_free(out)
dll.pillow_c_image_free(h)

