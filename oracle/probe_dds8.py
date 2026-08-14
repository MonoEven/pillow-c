"""Python reference port of Pillow's BC7 decode to diff against the real
output and isolate the C++ port bug."""

import io
import struct

from PIL import Image

block = bytes.fromhex(
    "20f0001ec0f30f212727276f6c6c6c00"
)

bc7_modes = [
    (3, 4, 0, 0, 4, 0, 1, 0, 3, 0),
    (2, 6, 0, 0, 6, 0, 0, 1, 3, 0),
    (3, 6, 0, 0, 5, 0, 0, 0, 2, 0),
    (2, 6, 0, 0, 7, 0, 1, 0, 2, 0),
    (1, 0, 2, 1, 5, 6, 0, 0, 2, 3),
    (1, 0, 2, 0, 7, 8, 0, 0, 2, 2),
    (1, 0, 0, 0, 7, 7, 1, 0, 4, 0),
    (2, 6, 0, 0, 5, 5, 1, 0, 2, 0),
]
bc7_weights2 = [0, 21, 43, 64]
bc7_weights3 = [0, 9, 18, 27, 37, 46, 55, 64]
bc7_weights4 = [0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64]


def get_bits(src, bit, count):
    if not count:
        return 0
    by = bit >> 3
    bit &= 7
    if bit + count <= 8:
        v = (src[by] >> bit) & ((1 << count) - 1)
    else:
        x = src[by] | (src[by + 1] << 8)
        v = (x >> bit) & ((1 << count) - 1)
    return v


def weights(n):
    if n == 2:
        return bc7_weights2
    if n == 3:
        return bc7_weights3
    return bc7_weights4


def expand(v, bits):
    v = v << (8 - bits)
    return v | (v >> bits)


def decode_bc7(src):
    col = [(0, 0, 0, 255)] * 16
    mode0 = src[0]
    if not mode0:
        return col
    bit = 0
    m = mode0
    while not (m & (1 << bit)):
        bit += 1
    mode = bit
    ns, pb, rb, isb, cb, ab, epb, spb, ib, ib2 = bc7_modes[mode]
    cw = weights(ib)
    aw = weights(ib2 if (ab and ib2) else ib)
    bit = 0
    partition = get_bits(src, bit, pb) if pb else 0
    bit += pb
    rotation = get_bits(src, bit, rb) if rb else 0
    bit += rb
    index_sel = get_bits(src, bit, isb) if isb else 0
    bit += isb
    numep = ns << 1
    endpoints = [[0, 0, 0, 255] for _ in range(numep)]
    for i in range(numep):
        endpoints[i][0] = get_bits(src, bit, cb); bit += cb
    for i in range(numep):
        endpoints[i][1] = get_bits(src, bit, cb); bit += cb
    for i in range(numep):
        endpoints[i][2] = get_bits(src, bit, cb); bit += cb
    for i in range(numep):
        if ab:
            endpoints[i][3] = get_bits(src, bit, ab); bit += ab
    if epb:
        for i in range(numep):
            val = get_bits(src, bit, 1); bit += 1
            for c in range(4):
                endpoints[i][c] = (endpoints[i][c] << 1) | val
    if spb:
        for i in range(0, numep, 2):
            val = get_bits(src, bit, 1); bit += 1
            for j in range(2):
                for c in range(4):
                    endpoints[i + j][c] = (endpoints[i + j][c] << 1) | val
    ecb = cb + (1 if epb else 0) + (1 if spb else 0)
    eab = ab + (1 if epb else 0) + (1 if spb else 0)
    for i in range(numep):
        endpoints[i][0] = expand(endpoints[i][0], ecb)
        endpoints[i][1] = expand(endpoints[i][1], ecb)
        endpoints[i][2] = expand(endpoints[i][2], ecb)
        if ab:
            endpoints[i][3] = expand(endpoints[i][3], eab)
    cibit = bit
    aibit = cibit + 16 * ib - ns
    for i in range(16):
        s = 0  # ns == 1 -> subset 0
        ibi = ib
        if i == 0:
            ibi -= 1
        i0 = get_bits(src, cibit, ibi)
        cibit += ibi
        if ab and ib2:
            ib2i = ib2
            if ib2i and i == 0:
                ib2i -= 1
            i1 = get_bits(src, aibit, ib2i)
            aibit += ib2i
            if index_sel:
                e0, e1 = endpoints[s], endpoints[s + 1]
                t0, t1 = 64 - aw[i1], 64 - cw[i0]
            else:
                e0, e1 = endpoints[s], endpoints[s + 1]
                t0, t1 = 64 - cw[i0], 64 - aw[i1]
        else:
            e0, e1 = endpoints[s], endpoints[s + 1]
            t0 = t1 = 64 - cw[i0]
        px = [(t0 * e0[c] + (64 - t0) * e1[c] + 32) >> 6 for c in range(4)]
        if rotation == 1:
            px[0], px[3] = px[3], px[0]
        elif rotation == 2:
            px[1], px[3] = px[3], px[1]
        elif rotation == 3:
            px[2], px[3] = px[3], px[2]
        col[i] = tuple(px)
    return col


ref = decode_bc7(block)
print("ref:", ref[0], ref[15])

hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DX10")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + struct.pack("<5I", 98, 3, 0, 0, 1)
im = Image.open(io.BytesIO(hdr + block)); im.load()
print("real:", im.getpixel((0, 0)), im.getpixel((3, 3)))
print("real all:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
print("ref all:", ref)
