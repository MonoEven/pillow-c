"""API-GRAB-001 clipboard probe: modes per DIB bit count."""

import ctypes
import struct

from PIL import ImageGrab

CF_DIB = 8
GMEM_MOVEABLE = 0x0002

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
kernel32.GlobalAlloc.argtypes = [ctypes.c_uint, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = ctypes.c_void_p
kernel32.GlobalLock.argtypes = [ctypes.c_void_p]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalUnlock.argtypes = [ctypes.c_void_p]
user32.OpenClipboard.argtypes = [ctypes.c_void_p]
user32.OpenClipboard.restype = ctypes.c_int
user32.EmptyClipboard.restype = ctypes.c_int
user32.SetClipboardData.argtypes = [ctypes.c_uint, ctypes.c_void_p]
user32.SetClipboardData.restype = ctypes.c_void_p
user32.CloseClipboard.restype = ctypes.c_int


def set_dib(data):
    h = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
    p = kernel32.GlobalLock(h)
    ctypes.memmove(p, data, len(data))
    kernel32.GlobalUnlock(h)
    assert user32.OpenClipboard(0)
    user32.EmptyClipboard()
    assert user32.SetClipboardData(CF_DIB, h)
    user32.CloseClipboard()


def dib(width, height, bpp, rows):
    palette = b""
    if bpp == 8:
        palette = b"".join(bytes([i, i, i, 0]) for i in range(256))
    elif bpp == 4:
        palette = b"".join(bytes([i * 17, 0, 0, 0]) for i in range(16))
    elif bpp == 1:
        palette = bytes([0, 0, 0, 0, 255, 255, 255, 0])
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, bpp, 0, 0, 0, 0, 0, 0)
    return header + palette + rows


def main():
    # 24bpp bottom-up: rows y=1 then y=0, BGR, DWORD-aligned strides.
    rows24 = bytes([0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0]) + bytes([10, 20, 30, 40, 50, 60, 70, 80, 90, 0, 0, 0])
    set_dib(dib(3, 2, 24, rows24))
    im = ImageGrab.grabclipboard()
    print("24bpp ->", im.mode, im.size, list(im.getdata()), im.tobytes().hex(" "))

    # 32bpp top-down BGRA.
    rows32 = bytes([0, 0, 255, 128, 0, 255, 0, 128, 255, 0, 0, 128])
    set_dib(dib(3, 1, 32, rows32))
    im = ImageGrab.grabclipboard()
    print("32bpp ->", im.mode, im.size, list(im.getdata()), im.tobytes().hex(" "))

    # 8bpp bottom-up paletted, stride 4.
    rows8 = bytes([0, 1, 2, 0]) + bytes([2, 1, 0, 0])
    set_dib(dib(3, 2, 8, rows8))
    im = ImageGrab.grabclipboard()
    print("8bpp ->", im.mode, im.size, list(im.getdata()), im.tobytes().hex(" "))

    # 1bpp: two rows of 8 pixels each -> 0b10101010, 0b01010101.
    rows1 = bytes([0xAA, 0, 0, 0]) + bytes([0x55, 0, 0, 0])
    set_dib(dib(8, 2, 1, rows1))
    im = ImageGrab.grabclipboard()
    print("1bpp ->", im.mode, im.size, list(im.getdata()), im.tobytes().hex(" "))


if __name__ == "__main__":
    main()
