"""Python reference port of Pillow's BC6 decode vs the real output."""

import io
import struct

from PIL import Image

block = bytes.fromhex("f08faa002c5ed460e200108df5118df5")

bc6_packings0 = [
    116, 132, 180, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 48, 49, 50, 51, 52, 164, 112, 113, 114, 115, 64, 65,
    66, 67, 68, 176, 160, 161, 162, 163, 80, 81, 82, 83, 84, 177, 128,
    129, 130, 131, 96, 97, 98, 99, 100, 178, 144, 145, 146, 147, 148, 179,
]
bc6_mode0 = (2, 1, 5, 10, 5, 5, 5)
ai0 = [15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
       15, 2, 8, 2, 2, 8, 8, 15, 2, 8, 2, 2, 8, 8, 2, 2, 15, 15, 6, 8, 2,
       8, 15, 15, 2, 8, 2, 2, 2, 15, 15, 6, 6, 2, 6, 8, 15, 15, 2, 2, 15,
       15, 15, 15, 15, 2, 2, 15]
w3 = [0, 9, 18, 27, 37, 46, 55, 64]
si2 = [0xcccc, 0x8888, 0xeeee, 0xecc8, 0xc880, 0xfeec, 0xfec8, 0xec80, 0xc800, 0xffec,
       0xfe80, 0xe800, 0xffe8, 0xff00, 0xfff0, 0xf000, 0xf710, 0x008e, 0x7100, 0x08ce,
       0x008c, 0x7310, 0x3100, 0x8cce, 0x088c, 0x3110, 0x6666, 0x366c, 0x17e8, 0x0ff0,
       0x718e, 0x399c, 0xaaaa, 0xf0f0, 0x5a5a, 0x33cc, 0x3c3c, 0x55aa, 0x9696, 0xa55a,
       0x73ce, 0x13c8, 0x324c, 0x3bdc, 0x6996, 0xc33c, 0x9966, 0x0660, 0x0272, 0x04e4,
       0x4e40, 0x2720, 0xc936, 0x936c, 0x39c6, 0x639c, 0x9336, 0x9cc6, 0x817e, 0xe718,
       0xccf0, 0x0fcc, 0x7744, 0xee22]


def get_bit(src, bit):
    by = bit >> 3
    bit &= 7
    return (src[by] >> bit) & 1


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


def sign_extend(v, prec):
    x = v
    if x & (1 << (prec - 1)):
        x |= -1 << prec
    return x & 0xFFFF


def unquantize(v, prec, sign):
    if not sign:
        x = v
        if prec >= 15:
            return x
        if x == 0:
            return 0
        if x == ((1 << prec) - 1):
            return 0xFFFF
        return ((x << 15) + 0x4000) >> (prec - 1)
    x = v if v < 0x8000 else v - 0x10000
    s = 0
    if prec >= 16:
        return x
    if x < 0:
        s = 1
        x = -x
    if x != 0:
        if x >= ((1 << (prec - 1)) - 1):
            x = 0x7FFF
        else:
            x = ((x << 15) + 0x4000) >> (prec - 1)
    return -x if s else x


def half_to_float(h):
    import struct as st
    o = (h & 0x7FFF) << 13
    m = 0x77800000
    f = st.unpack("<f", st.pack("<I", o))[0] * st.unpack("<f", st.pack("<I", m))[0]
    if f >= st.unpack("<f", st.pack("<I", 0x47800000))[0]:
        o |= 255 << 23
    o |= (h & 0x8000) << 16
    return st.unpack("<f", st.pack("<I", o))[0]


def finalize(v, sign):
    if sign:
        if v < 0:
            v = ((-v) * 31) // 32
            return half_to_float(0x8000 | v)
        return half_to_float((v * 31) // 32)
    return half_to_float((v * 31) // 64)


def clamp(f):
    if f < 0:
        return 0
    if f > 1:
        return 255
    return int(f * 255)


def decode_bc6(src, sign=0):
    col = []
    bit = 5
    epbits = 75
    ib = 3
    mode = src[0] & 0x1F
    if (mode & 3) == 0 or (mode & 3) == 1:
        mode &= 3
        bit = 2
    elif (mode & 3) == 2:
        mode = 2 + (mode >> 2)
        epbits = 72
    else:
        mode = 10 + (mode >> 2)
        epbits = 60
        ib = 4
    if mode >= 14:
        return [(0, 0, 0)] * 16
    ns, tr, pb, epb, rb, gb, bb = bc6_mode0
    endpoints = [0] * 12
    for i in range(epbits):
        di = bc6_packings0[i]
        dw = di >> 4
        dbit = di & 15
        endpoints[dw] |= get_bit(src, bit + i) << dbit
    bit += epbits
    partition = get_bits(src, bit, pb)
    bit += pb
    mask = (1 << epb) - 1
    if sign:
        endpoints[0] = sign_extend(endpoints[0], epb)
        endpoints[1] = sign_extend(endpoints[1], epb)
        endpoints[2] = sign_extend(endpoints[2], epb)
    if sign or tr:
        for i in range(3, 12, 3):
            endpoints[i] = sign_extend(endpoints[i], rb)
            endpoints[i + 1] = sign_extend(endpoints[i + 1], gb)
            endpoints[i + 2] = sign_extend(endpoints[i + 2], bb)
    if tr:
        for i in range(3, 12, 3):
            endpoints[i] = (endpoints[i] + endpoints[0]) & mask
            endpoints[i + 1] = (endpoints[i + 1] + endpoints[1]) & mask
            endpoints[i + 2] = (endpoints[i + 2] + endpoints[2]) & mask
    ueps = [unquantize(endpoints[i], epb, sign) for i in range(12)]
    for i in range(16):
        s = (1 & (si2[partition] >> i)) * 6
        ib2 = ib
        if i == 0:
            ib2 -= 1
        elif ns == 2 and i == ai0[partition]:
            ib2 -= 1
        i0 = get_bits(src, bit, ib2)
        bit += ib2
        t = 64 - w3[i0]
        r = (ueps[s] * t + ueps[s + 3] * w3[i0]) >> 6
        g = (ueps[s + 1] * t + ueps[s + 4] * w3[i0]) >> 6
        b = (ueps[s + 2] * t + ueps[s + 5] * w3[i0]) >> 6
        col.append((clamp(finalize(r, sign)), clamp(finalize(g, sign)), clamp(finalize(b, sign))))
    return col


ref = decode_bc6(block)
print("ref:", ref[0], ref[15])
print("ref all:", ref)

hdr = struct.pack("<4s", b"DDS ") + struct.pack("<7I", 124, 0x81007, 4, 4, 16, 0, 0) + struct.pack("11I", *((0,) * 11)) + struct.pack("<4I", 32, 0x4, struct.unpack("<I", b"DX10")[0], 32) + struct.pack("<4I", 0, 0, 0, 0) + struct.pack("<5I", 0x1000, 0, 0, 0, 0) + struct.pack("<5I", 95, 3, 0, 0, 1)
im = Image.open(io.BytesIO(hdr + block)); im.load()
print("real all:", [im.getpixel((x, y)) for y in range(4) for x in range(4)])
