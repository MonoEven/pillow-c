"""API-GRAB-001 cross-check: DLL clipboard DIB -> Pillow grabclipboard parity."""

import ctypes
import os
import struct

from PIL import ImageGrab

DLL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build", "x64", "Release", "pillow_c.dll",
)

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
    elif bpp == 1:
        palette = bytes([0, 0, 0, 0, 255, 255, 255, 0])
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, bpp, 0, 0, 0, 0, 0, 0)
    return header + palette + rows


def main():
    lib = ctypes.CDLL(DLL)
    grab = lib.pillow_c_image_grab_clipboard
    grab.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    grab.restype = ctypes.c_int
    data_fn = lib.pillow_c_image_data
    data_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)), ctypes.POINTER(ctypes.c_size_t)]
    data_fn.restype = ctypes.c_int
    mode_fn = lib.pillow_c_image_mode
    mode_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    mode_fn.restype = ctypes.c_int
    size_fn = lib.pillow_c_image_size
    size_fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
    size_fn.restype = ctypes.c_int
    free = lib.pillow_c_image_free
    free.argtypes = [ctypes.c_void_p]
    free.restype = ctypes.c_int

    failures = 0

    cases = [
        (3, 2, 24, bytes([0, 0, 255, 0, 255, 0, 255, 0, 0, 0, 0, 0])
             + bytes([10, 20, 30, 40, 50, 60, 70, 80, 90, 0, 0, 0])),
        (3, 1, 32, bytes([0, 0, 255, 128, 0, 255, 0, 128, 255, 0, 0, 128])),
        (3, 2, 8, bytes([0, 1, 2, 0]) + bytes([2, 1, 0, 0])),
        (8, 2, 1, bytes([0xAA, 0, 0, 0]) + bytes([0x55, 0, 0, 0])),
    ]

    for width, height, bpp, rows in cases:
        set_dib(dib(width, height, bpp, rows))
        expected = ImageGrab.grabclipboard()
        expected_raw = expected.tobytes()
        if expected.mode == "1":
            expected_raw = bytes(
                255 if (byte >> (7 - bit)) & 1 else 0
                for byte in expected_raw
                for bit in range(8)
            )

        out = ctypes.c_void_p()
        status = grab(ctypes.byref(out))
        assert status == 0, (bpp, status)
        p = ctypes.POINTER(ctypes.c_uint8)()
        s = ctypes.c_size_t()
        assert data_fn(out, ctypes.byref(p), ctypes.byref(s)) == 0
        raw = bytes(p[:s.value])
        byte_size = ctypes.c_size_t()
        assert size_fn(out, ctypes.byref(byte_size)) == 0
        mode_value = ctypes.c_int()
        assert mode_fn(out, ctypes.byref(mode_value)) == 0
        ok = raw == expected_raw and byte_size.value == len(expected_raw)
        print("%dbpp: mode=%s size=%s raw=%s (expected %s %s)" % (
            bpp, mode_value.value, byte_size.value, raw.hex(" "), expected.mode,
            expected_raw.hex(" ")))
        if not ok:
            failures += 1
        free(out)

    # Empty clipboard -> null handle, status 0.
    user32.OpenClipboard(0)
    user32.EmptyClipboard()
    user32.CloseClipboard()
    out = ctypes.c_void_p()
    status = grab(ctypes.byref(out))
    ok = status == 0 and out.value is None
    print("empty: status=%s handle=%s (expected 0 / None)" % (status, out.value))
    if not ok:
        failures += 1

    print("FAILURES:", failures)
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
