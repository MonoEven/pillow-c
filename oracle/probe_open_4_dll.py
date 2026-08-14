"""BEHAV-OPEN-004 cross-check: our open_psd vs the Pillow oracle pins."""
import ctypes
import json
import os
import struct

dll = ctypes.CDLL(r"build\x64\Release\pillow_c.dll")
dll.pillow_c_image_open_psd.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_psd.restype = ctypes.c_int
dll.pillow_c_image_mode.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_mode.restype = ctypes.c_int
dll.pillow_c_image_width.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_width.restype = ctypes.c_int
dll.pillow_c_image_height.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_height.restype = ctypes.c_int
dll.pillow_c_image_get_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_get_bytes.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]

MODES = {1: "L", 3: "RGB", 4: "RGBA", 5: "1", 6: "P", 7: "CMYK", 15: "LAB"}
tmp = os.environ["TEMP"]


def make_psd(mode_id, bits, channels, w, h, planes, compression=0, color_data=b"", resources=b"", psd_channels=None):
    if psd_channels is None:
        psd_channels = channels
    header = b"8BPS" + struct.pack(">H", 1) + b"\x00" * 6
    header += struct.pack(">H", psd_channels) + struct.pack(">I", h) + struct.pack(">I", w)
    header += struct.pack(">H", bits) + struct.pack(">H", mode_id)
    body = struct.pack(">I", len(color_data)) + color_data
    body += struct.pack(">I", len(resources)) + resources
    body += struct.pack(">I", 0)
    body += struct.pack(">H", compression)
    if compression == 1:
        for plane in planes:
            for row in plane:
                body += struct.pack(">H", len(row))
        for plane in planes:
            for row in plane:
                body += bytes(row)
    else:
        body += b"".join(planes)
    if len(body) & 1:
        body += b"\x00"
    return header + body


def try_open(blob):
    p = os.path.join(tmp, "xchk4.psd")
    with open(p, "wb") as f:
        f.write(blob)
    handle = ctypes.c_void_p()
    st = dll.pillow_c_image_open_psd(p.encode(), ctypes.byref(handle))
    if st != 0:
        return ("status", st)
    m = ctypes.c_int(); w = ctypes.c_int(); h = ctypes.c_int()
    dll.pillow_c_image_mode(handle, ctypes.byref(m))
    dll.pillow_c_image_width(handle, ctypes.byref(w))
    dll.pillow_c_image_height(handle, ctypes.byref(h))
    mode = MODES.get(m.value, str(m.value))
    size = w.value * h.value * (4 if mode in ("RGBA", "CMYK") else (3 if mode in ("RGB", "LAB") else 1))
    buf = ctypes.create_string_buffer(size)
    dll.pillow_c_image_get_bytes(handle, buf, size)
    dll.pillow_c_image_free(handle)
    return (st, mode, [w.value, h.value], bytes(buf).hex())


rgb = try_open(make_psd(3, 8, 3, 3, 2, [bytes([10 + i for i in range(6)]), bytes([20 + i for i in range(6)]), bytes([30 + i for i in range(6)])]))
print("rgb", rgb, "EXPECT RGB 0a141e0b151f0c16200d17210e18220f1923")
cmyk = try_open(make_psd(4, 8, 4, 2, 1, [bytes([100, 101]), bytes([110, 111]), bytes([120, 121]), bytes([130, 131])]))
print("cmyk", cmyk, "EXPECT 9b91877d9a90867c")
rgba = try_open(make_psd(3, 8, 4, 2, 1, [b"\x0a\x0b", b"\x14\x15", b"\x1e\x1f", b"\x28\x29"]))
print("rgba", rgba, "EXPECT 0a141e280b151f29")
one = try_open(make_psd(0, 1, 1, 3, 2, [bytes([0x60, 0x20])]))
print("mode1", one, "EXPECT 1 6020")
lab = try_open(make_psd(9, 8, 3, 2, 1, [bytes([10, 11]), bytes([20, 21]), bytes([30, 31])]))
print("lab", lab, "EXPECT 0a949e0b959f")
p = try_open(make_psd(2, 8, 1, 2, 1, [bytes([0, 1])], color_data=bytes(i % 256 for i in range(768))))
print("palette", p, "EXPECT P 0001")
def pb_row(values):
    out = bytearray()
    i = 0
    while i < len(values):
        run = 1
        while i + run < len(values) and values[i + run] == values[i] and run < 128:
            run += 1
        if run > 1:
            out.append(257 - run)
            out.append(values[i])
            i += run
        else:
            lit = 1
            while i + lit < len(values) and values[i + lit] != values[i] and lit < 128:
                lit += 1
            out.append(lit - 1)
            out += bytes(values[i:i + lit])
            i += lit
    return bytes(out)


pb_plane = [[pb_row([10, 10, 10, 11, 12, 13]), pb_row([14, 14, 15, 16, 17, 18])],
            [pb_row([20, 20, 20, 21, 22, 23]), pb_row([24, 24, 25, 26, 27, 28])],
            [pb_row([30, 30, 30, 31, 32, 33]), pb_row([34, 34, 35, 36, 37, 38])]]
pb = try_open(make_psd(3, 8, 3, 3, 2, pb_plane, compression=1))
print("packbits", pb, "EXPECT 0a141e0a141e0a141e0b151f0c16200d1721")
bad = try_open(make_psd(3, 8, 3, 2, 1, [b"\x0a"] * 3, psd_channels=2))
print("few_channels", bad, "EXPECT -47")
trunc = try_open(make_psd(3, 8, 3, 3, 2, [b"\x01\x02\x03\x04"]))
print("truncated", trunc, "EXPECT -29")
