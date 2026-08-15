"""Oracle pin: Pillow 11.3.0 + numpy 1.25.0 interop behavior.

Covers numpy.asarray(im) per mode and Image.fromarray per dtype,
including error shapes, for the pillow-c Numpy interop packet.
"""
import warnings
import numpy as np
from PIL import Image

warnings.simplefilter("ignore")

MODES = ["1", "L", "P", "RGB", "RGBA", "CMYK", "YCbCr", "LAB", "HSV",
         "I", "F", "I;16", "I;16B", "LA"]
MODES_EXIST = ["1", "L", "P", "RGB", "RGBA", "CMYK", "YCbCr", "LAB",
               "HSV", "I", "F", "I;16", "I;16B", "LA"]


def build(mode):
    if mode == "1":
        return Image.frombytes("1", (3, 2), b"\x80\x80\x80" + b"\x00\x00\x00")
    if mode in ("L", "P"):
        return Image.frombytes(mode, (3, 2), bytes(range(1, 7)))
    if mode == "I":
        return Image.frombytes("I", (2, 2), np.array([-2, 3, 4, -5], dtype="<i4").tobytes())
    if mode == "F":
        return Image.frombytes("F", (2, 2), np.array([0.5, -1.25, 2.0, 3.5], dtype="<f4").tobytes())
    if mode == "I;16":
        return Image.frombytes("I;16", (2, 2), np.array([1, 2, 3, 4], dtype="<u2").tobytes())
    if mode == "I;16B":
        return Image.frombytes("I;16B", (2, 2), np.array([1, 2, 3, 4], dtype=">u2").tobytes())
    if mode == "RGB":
        return Image.frombytes("RGB", (3, 2), bytes(range(1, 19)))
    if mode == "RGBA":
        return Image.frombytes("RGBA", (3, 2), bytes(range(1, 25)))
    if mode == "CMYK":
        return Image.frombytes("CMYK", (3, 2), bytes(range(1, 25)))
    if mode == "YCbCr":
        return Image.frombytes("YCbCr", (3, 2), bytes(range(1, 19)))
    if mode == "LAB":
        return Image.frombytes("LAB", (3, 2), bytes(range(1, 19)))
    if mode == "HSV":
        return Image.frombytes("HSV", (3, 2), bytes(range(1, 19)))
    if mode == "LA":
        return Image.frombytes("LA", (3, 2), bytes(range(1, 13)))
    raise AssertionError(mode)


print("== asarray per mode ==")
for mode in MODES:
    im = build(mode)
    a = np.asarray(im)
    print(f"{mode!r}: dtype={a.dtype.str} shape={a.shape} bytes={a.tobytes().hex()}")

print("== fromarray per dtype ==")
DTYPES = [np.bool_, np.int8, np.uint8, np.int16, np.uint16, np.int32,
          np.uint32, np.int64, np.uint64, np.float32, np.float64]
for dt in DTYPES:
    for shape in [(2, 3), (2, 3, 3), (2, 3, 4)]:
        try:
            a = np.zeros(shape, dtype=dt)
            im = Image.fromarray(a)
            print(f"{np.dtype(dt).str} {shape}: mode={im.mode} size={im.size} "
                  f"bytes={im.tobytes().hex()}")
        except Exception as e:  # noqa: BLE001
            print(f"{np.dtype(dt).str} {shape}: ERROR {type(e).__name__}: {e}")
    try:
        a = np.zeros((2, 3, 2), dtype=dt)
        im = Image.fromarray(a)
        print(f"{np.dtype(dt).str} (2,3,2): mode={im.mode} size={im.size}")
    except Exception as e:  # noqa: BLE001
        print(f"{np.dtype(dt).str} (2,3,2): ERROR {type(e).__name__}: {e}")
    try:
        a = np.zeros((2, 3, 5), dtype=dt)
        im = Image.fromarray(a)
        print(f"{np.dtype(dt).str} (2,3,5): mode={im.mode} size={im.size}")
    except Exception as e:  # noqa: BLE001
        print(f"{np.dtype(dt).str} (2,3,5): ERROR {type(e).__name__}: {e}")

print("== fromarray values ==")
a = np.array([[1, 2, 3], [4, 5, 6]], dtype=np.int8)
im = Image.fromarray(a)
print(f"i1 values: mode={im.mode} bytes={im.tobytes().hex()}")
a = np.array([[-2, 300], [4, -5]], dtype=np.int16)
im = Image.fromarray(a)
print(f"i2 values: mode={im.mode} bytes={im.tobytes().hex()}")
a = np.array([[0.5, -1.25], [2.0, 3.5]], dtype=np.float64)
im = Image.fromarray(a)
print(f"f8 values: mode={im.mode} bytes={im.tobytes().hex()}")
a = np.array([[True, False], [False, True]], dtype=np.bool_)
im = Image.fromarray(a)
print(f"b1 values: mode={im.mode} size={im.size} bytes={im.tobytes().hex()}")
a = np.array([[300, 0]], dtype=np.uint16)
im = Image.fromarray(a, mode="L")
print(f"u2 mode=L: getpixel={im.getpixel((0, 0))} bytes={im.tobytes().hex()}")
try:
    a = np.array([[300, 0, 0]], dtype=np.uint16)
    im = Image.fromarray(a, mode="RGB")
    print(f"u2 mode=RGB: getpixel={im.getpixel((0, 0))}")
except Exception as e:  # noqa: BLE001
    print(f"u2 mode=RGB: ERROR {type(e).__name__}: {e}")
a = np.arange(12, dtype=np.uint32).reshape(3, 4)
im = Image.fromarray(a)
print(f"u4 values: mode={im.mode} getpixel={im.getpixel((0, 1))}")
a = np.array([1, 2], dtype=np.float32).reshape(2, 1)
im = Image.fromarray(a)
print(f"f4 1d: mode={im.mode} size={im.size} bytes={im.tobytes().hex()}")

print("== fromarray errors ==")
try:
    Image.fromarray(np.zeros((2, 2, 2, 2), dtype=np.uint8))
except Exception as e:  # noqa: BLE001
    print(f"4d: {type(e).__name__}: {e}")
try:
    Image.fromarray(np.zeros((2, 2, 2), dtype=np.int64))
except Exception as e:  # noqa: BLE001
    print(f"i8 3d: {type(e).__name__}: {e}")
try:
    Image.fromarray(np.zeros((2, 2), dtype=np.float16))
except Exception as e:  # noqa: BLE001
    print(f"f2: {type(e).__name__}: {e}")
try:
    Image.fromarray(np.zeros((2, 2), dtype=np.complex64))
except Exception as e:  # noqa: BLE001
    print(f"c8: {type(e).__name__}: {e}")

print("== non-contiguous ==")
a = np.arange(36, dtype=np.uint8).reshape(6, 6)
b = a[::2, ::2]
im = Image.fromarray(b)
print(f"strided u1: mode={im.mode} size={im.size} bytes={im.tobytes().hex()}")

print("== round trips ==")
for mode in ["L", "RGB", "RGBA", "I", "F", "I;16", "LA", "CMYK", "YCbCr"]:
    im = build(mode)
    a = np.asarray(im)
    back = Image.fromarray(a)
    ok = back.mode == im.mode and back.size == im.size and back.tobytes() == im.tobytes()
    print(f"{mode}: roundtrip_ok={ok} back_mode={back.mode}")

print("== asarray P palette ==")
im = Image.new("P", (2, 2), 0)
im.putpalette([255, 0, 0] * 256)
a = np.asarray(im)
print(f"P: dtype={a.dtype.str} shape={a.shape} bytes={a.tobytes().hex()}")

print("== asarray mode 1 ==")
im = build("1")
a = np.asarray(im)
print(f"1: dtype={a.dtype.str} shape={a.shape} bytes={a.tobytes().hex()}")

print("== asarray I;16B ==")
im = build("I;16B")
a = np.asarray(im)
print(f"I;16B: dtype={a.dtype.str} shape={a.shape} bytes={a.tobytes().hex()}")
