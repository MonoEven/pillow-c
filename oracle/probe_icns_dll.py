"""ctypes probe: native ICNS open/save/sizes vs the Pillow 11.3.0 oracle."""
import ctypes
import io
import json
import struct

from PIL import Image

dll = ctypes.CDLL(r"build\x64\Release\pillow_c.dll")
dll.pillow_c_image_create_mode.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_create_mode.restype = ctypes.c_int
dll.pillow_c_image_set_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_set_bytes.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]
dll.pillow_c_image_free.restype = ctypes.c_int
dll.pillow_c_image_get_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_get_bytes.restype = ctypes.c_int
dll.pillow_c_image_save_icns.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
dll.pillow_c_image_save_icns.restype = ctypes.c_int
dll.pillow_c_image_open_icns.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_open_icns.restype = ctypes.c_int
dll.pillow_c_image_icns_sizes.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_int), ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
dll.pillow_c_image_icns_sizes.restype = ctypes.c_int

oracle = json.load(open(r"oracle/probe_icns.json", encoding="utf-8"))

MODE_L = 1  # check against the facade: mode id 1 for L (verify below)


def create_image(mode, w, h, data):
    handle = ctypes.c_void_p()
    assert dll.pillow_c_image_create_mode(w, h, mode, ctypes.byref(handle)) == 0, "create failed"
    buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
    assert dll.pillow_c_image_set_bytes(handle, buf, len(data)) == 0, "set_bytes failed"
    return handle


def read_bytes(handle, size):
    out = (ctypes.c_uint8 * size)()
    assert dll.pillow_c_image_get_bytes(handle, out, size) == 0
    return bytes(out)


def read_head(handle, head_size, channels):
    # get_bytes requires the EXACT storage size; read it all, slice the head
    return read_bytes(handle, head_size)


def make_l_pattern(w, h):
    data = bytearray()
    for y in range(h):
        for x in range(w):
            data.append((x * 13 + y * 37) % 256)
    return bytes(data)


failures = 0

# 1. save L 20x10 -> compare whole file with the oracle
l_data = make_l_pattern(20, 10)
im = create_image(MODE_L, 20, 10, l_data)
path = rb"oracle\icns_probe_out.icns"
status = dll.pillow_c_image_save_icns(im, path)
dll.pillow_c_image_free(im)
print("save_icns status =", status)
if status != 0:
    failures += 1
else:
    ours = open(path, "rb").read()
    expected = bytes.fromhex(oracle["save_L"]["full"])
    print("our file length =", len(ours), "oracle length =", len(expected))
    print("whole-file byte-exact (not expected: PNG payloads use our deflate):", ours == expected)
    if ours == expected:
        print("NOTE: whole file unexpectedly byte-exact")

# 2. reopen: mode/size + tobytes head vs oracle
out_handle = ctypes.c_void_p()
status = dll.pillow_c_image_open_icns(path, ctypes.byref(out_handle))
print("open_icns status =", status)
if status != 0:
    failures += 1
else:
    head = read_head(out_handle.value, 1024 * 1024, 1)
    print("open head match:", head[:64].hex() == oracle["save_L_reopen"]["tobytes_head"])
    if head[:64].hex() != oracle["save_L_reopen"]["tobytes_head"]:
        failures += 1
        print("ours:", head[:64].hex())
        print("oracle:", oracle["save_L_reopen"]["tobytes_head"])
    dll.pillow_c_image_free(out_handle)

# 3. sizes
sizes = (ctypes.c_int * 64)()
count = ctypes.c_int(0)
status = dll.pillow_c_image_icns_sizes(path, sizes, 64, ctypes.byref(count))
print("icns_sizes status =", status, "count =", count.value)
triples = list(sizes[:count.value])
expected_triples = [v for t in oracle["save_L_reopen"]["sizes"] for v in t]
print("sizes match:", triples == expected_triples, triples)
if triples != expected_triples:
    failures += 1

# 4. error shapes
def expect_status(file_bytes, expected_status, label):
    p = rb"oracle\icns_probe_err.icns"
    with open(p, "wb") as f:
        f.write(file_bytes)
    handle = ctypes.c_void_p()
    status = dll.pillow_c_image_open_icns(p, ctypes.byref(handle))
    ok = status == expected_status
    print(label, "->", status, "expected", expected_status, "OK" if ok else "MISMATCH")
    if not ok:
        global failures
        failures += 1

expect_status(b"noti\x00\x00\x00\x08", -3, "bad magic")
expect_status(b"icns\x00\x00\x00\x08", -3, "no resources")
expect_status(b"icns\x00\x00\x00\x10ic07\x00\x00\x00\x00", -3, "invalid block header")
expect_status(b"icns\x00\x00\x00\x10ic07", -3, "truncated chunk header")

def build_icns(entries):
    out = bytearray(b"icns")
    file_length = 8 + (8 + 8 * len(entries)) + sum(8 + len(p) for _, p in entries)
    out += struct.pack(">i", file_length)
    out += b"TOC "
    out += struct.pack(">i", 8 + len(entries) * 8)
    for typ, payload in entries:
        out += typ + struct.pack(">i", 8 + len(payload))
    for typ, payload in entries:
        out += typ + struct.pack(">i", 8 + len(payload)) + payload
    return bytes(out)

expect_status(build_icns([(b"ic07", b"GARBAGE_PAYLOAD" * 8)]), -21, "garbage payload")

jp2_buf = io.BytesIO()
Image.new("RGB", (16, 16)).save(jp2_buf, format="JPEG2000")
expect_status(build_icns([(b"icp4", jp2_buf.getvalue())]), -20, "jpeg2000 payload")

# truncated PNG payload
png_buf = io.BytesIO()
Image.new("RGBA", (16, 16)).save(png_buf, format="PNG")
png_data = png_buf.getvalue()
expect_status(build_icns([(b"ic07", png_data[: len(png_data) // 2])]), -25, "truncated PNG payload")

# unsupported PNG mode: 16-bit grayscale PNG
png16 = io.BytesIO()
Image.new("I;16", (16, 16)).save(png16, format="PNG")
expect_status(build_icns([(b"ic07", png16.getvalue())]), -24, "16-bit PNG payload")

# it32 bad signature
legacy_rgb = bytearray()
for y in range(127, -1, -1):
    for x in range(128):
        legacy_rgb += bytes([(x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256])
expect_status(build_icns([(b"it32", b"\x01\x02\x03\x04" + bytes(legacy_rgb))]), -22, "it32 bad signature")

# RLE truncated: only band 0 present
rle = bytearray()
band0 = [(x * 13 + y) % 256 for y in range(15, -1, -1) for x in range(16)]
i = 0
n = len(band0)
while i < n:
    run = 1
    while i + run < n and band0[i + run] == band0[i] and run < 3:
        run += 1
    if run >= 2:
        rle.append(0x80 | (run - 125))
        rle.append(band0[i])
        i += run
    else:
        lit = 1
        while i + lit < n and lit < 128:
            nxt = 1
            while i + lit + nxt < n and band0[i + lit + nxt] == band0[i + lit] and nxt < 3:
                nxt += 1
            if nxt >= 2:
                break
            lit += 1
        rle.append(lit - 1)
        rle.extend(band0[i:i + lit])
        i += lit
expect_status(build_icns([(b"is32", bytes(rle))]), -23, "RLE truncated")

# legacy full decode: is32 + s8mk
legacy_rgb16 = bytearray()
legacy_mask16 = bytearray()
for y in range(15, -1, -1):
    for x in range(16):
        legacy_rgb16 += bytes([(x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256])
        legacy_mask16.append((x * 29 + y * 7) % 256)
legacy_file = build_icns([(b"is32", bytes(legacy_rgb16)), (b"s8mk", bytes(legacy_mask16))])
p = rb"oracle\icns_probe_legacy.icns"
with open(p, "wb") as f:
    f.write(legacy_file)
handle = ctypes.c_void_p()
status = dll.pillow_c_image_open_icns(p, ctypes.byref(handle))
print("legacy open status =", status)
if status != 0:
    failures += 1
else:
    head = read_head(handle.value, 1024, 4)
    print("legacy head match:", head[:64].hex() == oracle["legacy_is32_s8mk"]["tobytes_head"])
    if head[:64].hex() != oracle["legacy_is32_s8mk"]["tobytes_head"]:
        failures += 1
        print("ours:", head.hex())
        print("oracle:", oracle["legacy_is32_s8mk"]["tobytes_head"])
    dll.pillow_c_image_free(handle)

# legacy RGB only (is32 without mask)
legacy_file2 = build_icns([(b"is32", bytes(legacy_rgb16))])
with open(p, "wb") as f:
    f.write(legacy_file2)
handle = ctypes.c_void_p()
status = dll.pillow_c_image_open_icns(p, ctypes.byref(handle))
print("legacy rgb-only open status =", status)
if status != 0:
    failures += 1
else:
    head = read_head(handle.value, 768, 3)
    # Pillow pre-load tobytes packs RGB->RGBA; post-load mode is RGB
    print("legacy rgb-only head =", head[:64].hex())
    dll.pillow_c_image_free(handle)

# reopen an RGB save for mode fidelity
rgb_data = bytearray()
for y in range(10):
    for x in range(20):
        rgb_data += bytes([(x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256])
im = create_image(3, 20, 10, bytes(rgb_data))  # mode RGB id = 3?
status = dll.pillow_c_image_save_icns(im, path)
dll.pillow_c_image_free(im)
print("save RGB status =", status)
handle = ctypes.c_void_p()
status = dll.pillow_c_image_open_icns(path, ctypes.byref(handle))
print("open RGB status =", status)
if status == 0:
    head = read_head(handle.value, 3 * 1024 * 1024, 3)
    print("RGB reopen head match:", head[:64].hex() == oracle["save_RGB_reopen"]["tobytes_head"])
    if head[:64].hex() != oracle["save_RGB_reopen"]["tobytes_head"]:
        failures += 1
    dll.pillow_c_image_free(handle)

print("FAILURES:", failures)
