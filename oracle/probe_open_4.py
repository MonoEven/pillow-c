"""BEHAV-OPEN-004 oracle: PSD open behaviors in Pillow 11.3.0."""
import io
import json
import struct

from PIL import Image


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


out = {}


def make_psd(mode_id, bits, channels, w, h, plane_data, compression=0, color_data=b"", resources=b"", psd_channels=None):
    if psd_channels is None:
        psd_channels = channels
    header = b"8BPS" + struct.pack(">H", 1) + b"\x00" * 6
    header += struct.pack(">H", psd_channels)
    header += struct.pack(">I", h)
    header += struct.pack(">I", w)
    header += struct.pack(">H", bits)
    header += struct.pack(">H", mode_id)
    body = struct.pack(">I", len(color_data)) + color_data
    body += struct.pack(">I", len(resources)) + resources
    body += struct.pack(">I", 0)  # no layers
    body += struct.pack(">H", compression)
    if compression == 1:
        bytecounts = b""
        for plane in plane_data:
            for row in plane:
                bytecounts += struct.pack(">H", len(row))
        body += bytecounts
        body += b"".join(row for plane in plane_data for row in plane)
    else:
        body += b"".join(plane_data)
    if len(body) & 1:
        body += b"\x00"
    return header + body


def open_bytes(blob, fmt):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    data = im.tobytes()
    pal = list(im.getpalette()) if im.mode == "P" else None
    info = {k: (v.hex() if isinstance(v, bytes) else str(v)) for k, v in getattr(im, "info", {}).items()}
    return (im.mode, list(im.size), data.hex(), pal, info)


def packbits_row(data):
    out = bytearray()
    i = 0
    while i < len(data):
        run = 1
        while i + run < len(data) and data[i + run] == data[i] and run < 128:
            run += 1
        if run > 1:
            out.append(257 - run)
            out.append(data[i])
            i += run
        else:
            lit = 1
            while i + lit < len(data) and data[i + lit] != data[i] and lit < 128:
                lit += 1
            out.append(lit - 1)
            out += data[i:i + lit]
            i += lit
    return bytes(out)


# --- RGB raw ---
r = capture(lambda: open_bytes(make_psd(3, 8, 3, 3, 2, [bytes([10, 11, 12, 13, 14, 15]), bytes([20, 21, 22, 23, 24, 25]), bytes([30, 31, 32, 33, 34, 35])]), "PSD"))
out["psd_rgb"] = r
# RGBA (4 channels)
r = capture(lambda: open_bytes(make_psd(3, 8, 4, 2, 1, [b"\x0a\x0b", b"\x14\x15", b"\x1e\x1f", b"\x28\x29"]), "PSD"))
out["psd_rgba"] = r
# CMYK (inverted)
r = capture(lambda: open_bytes(make_psd(4, 8, 4, 2, 1, [bytes([100, 101]), bytes([110, 111]), bytes([120, 121]), bytes([130, 131])]), "PSD"))
out["psd_cmyk"] = r
# P with palette
pal = bytes(i % 256 for i in range(768))
r = capture(lambda: open_bytes(make_psd(2, 8, 1, 2, 1, [bytes([0, 1])], color_data=pal), "PSD"))
out["psd_palette"] = r
# mode 1
r = capture(lambda: open_bytes(make_psd(0, 1, 1, 3, 2, [bytes([0b01100000, 0b00100000])]), "PSD"))
out["psd_mode1"] = r
# LAB
r = capture(lambda: open_bytes(make_psd(9, 8, 3, 2, 1, [bytes([10, 11]), bytes([20, 21]), bytes([30, 31])]), "PSD"))
out["psd_lab"] = r
# grayscale L (mode 1 = grayscale 8-bit)
r = capture(lambda: open_bytes(make_psd(1, 8, 1, 3, 2, [bytes(range(6))]), "PSD"))
out["psd_gray"] = r
# --- packbits RGB ---
r = capture(lambda: open_bytes(make_psd(3, 8, 3, 3, 2, [[packbits_row(bytes([10, 10, 10, 11, 12, 13])), packbits_row(bytes([14, 14, 15, 16, 17, 18]))], [packbits_row(bytes([20, 20, 20, 21, 22, 23])), packbits_row(bytes([24, 24, 25, 26, 27, 28]))], [packbits_row(bytes([30, 30, 30, 31, 32, 33])), packbits_row(bytes([34, 34, 35, 36, 37, 38]))]], compression=1), "PSD"))
out["psd_packbits"] = r
# --- errors ---
r = capture(lambda: Image.open(io.BytesIO(make_psd(3, 8, 3, 2, 1, [b"\x0a"] * 3, psd_channels=2)), formats=["PSD"]))
out["psd_not_enough_channels"] = r
r = capture(lambda: Image.open(io.BytesIO(make_psd(9, 16, 3, 2, 1, [b"\x00" * 8])), formats=["PSD"]))
out["psd_bad_mode"] = r
r = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["PSD"]))
out["psd_bad_magic"] = r
r = capture(lambda: Image.open(io.BytesIO(b"8BPS" + struct.pack(">H", 2) + b"\x00" * 20), formats=["PSD"]))
out["psd_bad_version"] = r
r = capture(lambda: open_bytes(make_psd(3, 8, 3, 3, 2, [b"\x01\x02\x03"]), "PSD"))
out["psd_truncated_raw"] = r
r = capture(lambda: open_bytes(make_psd(3, 8, 3, 3, 2, [[b"\x02\x01\x01"], [b"\x02\x02"]], compression=1), "PSD"))
out["psd_truncated_packbits"] = r

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_4.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
