import ctypes
import struct

dll = ctypes.CDLL(r"build\x64\Release\pillow_c.dll")
dll.pillow_c_image_create_mode.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_create_mode.restype = ctypes.c_int
dll.pillow_c_image_set_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_set_bytes.restype = ctypes.c_int
dll.pillow_c_image_save_jpeg.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_jpeg.restype = ctypes.c_int
dll.pillow_c_image_save_jpeg_extra_options.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_double, ctypes.c_double, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_save_jpeg_extra_options.restype = ctypes.c_int
dll.pillow_c_image_open_jpeg.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_jpeg.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]


def make_image(w, h, base):
    data = bytearray()
    for y in range(h):
        for x in range(w):
            data += bytes([(base + x + y) % 256, (base * 2 + x) % 256, (base * 3 + y) % 256])
    handle = ctypes.c_void_p()
    assert dll.pillow_c_image_create_mode(w, h, 3, ctypes.byref(handle)) == 0
    buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(bytes(data))
    assert dll.pillow_c_image_set_bytes(handle, buf, len(data)) == 0
    return handle


a = make_image(4, 2, 10)
b = make_image(4, 2, 60)

total = 2
ifd_length = 66 + 16 * total
extra = b"\xff\xe2" + struct.pack(">H", 6 + ifd_length) + b"MPF\0" + b" " * ifd_length
extra_buf = (ctypes.c_uint8 * len(extra)).from_buffer_copy(extra)

p1 = rb"oracle\mpo_ours_f1.jpg"
p2 = rb"oracle\mpo_ours_f2.jpg"
print("extra save:", dll.pillow_c_image_save_jpeg_extra_options(a, p1, 75, 0, 0.0, 0.0, -1, -1, -1, extra_buf, len(extra)))
print("plain save:", dll.pillow_c_image_save_jpeg(b, p2))
dll.pillow_c_image_free(a)
dll.pillow_c_image_free(b)

f1 = open(p1, "rb").read()
f2 = open(p2, "rb").read()
print("f1 len", len(f1), "f2 len", len(f2))

offsets = [len(f1), len(f2)]
mpentries = b""
data_offset = 0
for i, size in enumerate(offsets):
    mptype = 0x030000 if i == 0 else 0
    mpentries += struct.pack("<LLLHH", mptype, size, data_offset, 0, 0)
    if i == 0:
        data_offset -= 28
    data_offset += size

ifd = b""
ifd += struct.pack("<H", 3)
ifd += struct.pack("<HH", 0xB000, 4) + struct.pack("<L", 1) + b"0100"
ifd += struct.pack("<HH", 0xB001, 4) + struct.pack("<L", 1) + struct.pack("<L", total)
ifd += struct.pack("<HH", 0xB002, 7) + struct.pack("<L", len(mpentries)) + struct.pack("<L", 50)
ifd += struct.pack("<L", 0)
ifd += mpentries

out = bytearray(f1 + f2)
out[28:28 + 8 + len(ifd)] = b"II\x2a\x00" + struct.pack("<L", 8) + ifd
out_path = rb"oracle\mpo_ours.mpo"
with open(out_path, "wb") as f:
    f.write(out)

handle = ctypes.c_void_p()
status = dll.pillow_c_image_open_jpeg(out_path, ctypes.byref(handle))
print("open OUR MPO status:", status)
if status == 0:
    dll.pillow_c_image_free(handle)

# also verify Pillow can read OUR mpo
from PIL import Image
try:
    im = Image.open(out_path)
    print("Pillow open ours:", im.format, im.mode, im.size, getattr(im, "n_frames", None))
    im.seek(1)
    print("frame1 size:", im.size)
except Exception as e:
    print("Pillow open ours FAILED:", type(e).__name__, e)
