"""BEHAV-PDF-001 cross-check: our pillow_c_image_save_pdf vs the Pillow oracle."""
import ctypes
import json
import os
import re
import struct

dll = ctypes.CDLL(r"build\x64\Release\pillow_c.dll")
dll.pillow_c_image_create_mode.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_void_p)]
dll.pillow_c_image_create_mode.restype = ctypes.c_int
dll.pillow_c_image_set_bytes.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_set_bytes.restype = ctypes.c_int
dll.pillow_c_image_put_palette_rgb.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
dll.pillow_c_image_put_palette_rgb.restype = ctypes.c_int
dll.pillow_c_image_save_pdf.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_double, ctypes.c_double,
                                        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                        ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
dll.pillow_c_image_save_pdf.restype = ctypes.c_int
dll.pillow_c_image_save_pdf_frames.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.c_char_p,
                                               ctypes.c_double, ctypes.c_double,
                                               ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                               ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
dll.pillow_c_image_save_pdf_frames.restype = ctypes.c_int
dll.pillow_c_image_free.argtypes = [ctypes.c_void_p]

oracle = json.load(open("oracle/probe_pdf.json"))
tmp = os.environ["TEMP"]


def mk(mode_id, w, h, data, palette=None):
    handle = ctypes.c_void_p()
    assert dll.pillow_c_image_create_mode(w, h, mode_id, ctypes.byref(handle)) == 0
    if data:
        buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(bytes(data))
        assert dll.pillow_c_image_set_bytes(handle, buf, len(data)) == 0
    if palette:
        pal = (ctypes.c_uint8 * len(palette)).from_buffer_copy(bytes(palette))
        assert dll.pillow_c_image_put_palette_rgb(handle, pal, len(palette)) == 0
    return handle


def save(handle, path, x=72.0, y=72.0):
    return dll.pillow_c_image_save_pdf(handle, path.encode(), x, y, None, None, None, None, None, None, None, None)


# --- P-mode single page: expect byte-exact vs oracle (modulo timestamps) ---
p = mk(6, 4, 3, bytes(range(12)), bytes(i % 256 for i in range(768)))  # P mode id 6
path = os.path.join(tmp, "single.pdf")
assert save(p, path) == 0
ours = open(path, "rb").read()
fixture = bytes.fromhex(oracle["single_p_full"])
ours_patched = re.sub(rb"D:\d{14}Z", b"D:20260814113723Z", ours)
print("single len:", len(ours), "fixture len:", len(fixture))
if ours_patched == fixture:
    print("P-mode single page: BYTE-EXACT (timestamps patched) PASS")
else:
    for i in range(min(len(ours_patched), len(fixture))):
        if ours_patched[i] != fixture[i]:
            print("first diff at", i)
            print("ours  :", ours_patched[max(0, i - 60):i + 60])
            print("expect:", fixture[max(0, i - 60):i + 60])
            break
    else:
        print("prefix equal; length diff", len(ours_patched), len(fixture))
    print("FAIL")

# --- RGB/L/CMYK single page: structure head match (DCT payload differs) ---
for mode_id, name in [(3, "rgb"), (1, "gray"), (7, "cmyk")]:
    w, h = (4, 2)
    if mode_id == 3:
        data = bytes(sum(([(10 + x + y) % 256, (20 + x) % 256, (30 + y) % 256] for y in range(h) for x in range(w)), []))
    elif mode_id == 1:
        data = bytes([(30 + x + y) % 256 for y in range(h) for x in range(w)])
    else:
        data = bytes(sum(([(x * 4) % 256, (y * 7) % 256, 128, 255] for y in range(h) for x in range(w)), []))
    im = mk(mode_id, w, h, data)
    out_path = os.path.join(tmp, name + ".pdf")
    assert save(im, out_path) == 0
    ours = open(out_path, "rb").read()
    expected_head = bytes.fromhex(oracle[name + "_head"])
    cut = ours.find(b"/Length ")
    head_ours = ours[:cut]
    head_expect = expected_head[:expected_head.find(b"/Length ")]
    common = min(len(head_ours), len(head_expect))
    print(name, "image-dict head match:", head_ours[:common] == head_expect[:common], "len:", len(ours))
    if head_ours[:common] != head_expect[:common]:
        print("  ours  :", head_ours[:common])
        print("  expect:", head_expect[:common])
    dll.pillow_c_image_free(im)

# --- multi-page: Kids and count ---
a = mk(3, 4, 2, bytes(sum(([(10 + x + y) % 256, (20 + x) % 256, (30 + y) % 256] for y in range(2) for x in range(4)), [])))
b = mk(3, 4, 2, bytes(sum(([(60 + x + y) % 256, (120 + x) % 256, (180 + y) % 256] for y in range(2) for x in range(4)), [])))
frames = (ctypes.c_void_p * 2)(a, b)
multi_path = os.path.join(tmp, "multi.pdf")
status = dll.pillow_c_image_save_pdf_frames(frames, 2, multi_path.encode(), 72.0, 72.0, None, None, None, None, None, None, None, None)
assert status == 0
multi = open(multi_path, "rb").read()
fixture_multi = bytes.fromhex(oracle["multi_head"])
print("multi head (first 260):", multi[:260].hex())
print("multi len:", len(multi), "fixture multi len:", oracle["multi_len"])
kids_match = b"/Kids [ 2 0 R 5 0 R ]" in multi
count_match = b"/Count 2" in multi
print("multi Kids/Count match:", kids_match, count_match)
# --- dpi/resolution + info fields ---
im = mk(3, 4, 2, bytes(sum(([(10 + x + y) % 256, (20 + x) % 256, (30 + y) % 256] for y in range(2) for x in range(4)), [])))
dpi_path = os.path.join(tmp, "dpi.pdf")
status = dll.pillow_c_image_save_pdf(im, dpi_path.encode(), 300.0, 150.0, b"custom title", b"author name", None, None, None, None, None, None)
assert status == 0
raw = open(dpi_path, "rb").read()
print("dpi mediabox:", b"/MediaBox [ 0 0 0.96 0.96 ]" in raw)
print("dpi contents:", b"q 0.960000 0 0 0.960000 0 0 cm" in raw)
print("dpi title/author:", b"/Title (" in raw, b"/Author (" in raw)
print("dpi dates:", len(re.findall(rb"D:\d{14}Z", raw)) == 2)
stem_path = os.path.join(tmp, "single2.pdf")
status = dll.pillow_c_image_save_pdf(im, stem_path.encode(), 72.0, 72.0, None, None, None, None, None, None, None, None)
assert status == 0
raw2 = open(stem_path, "rb").read()
title_line = raw2.split(b"/Title ")[1].split(b"\n")[0]
print("stem title line:", title_line)
dll.pillow_c_image_free(im)

dll.pillow_c_image_free(p)
