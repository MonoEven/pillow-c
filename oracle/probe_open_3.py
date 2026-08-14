"""BEHAV-OPEN-003 oracle: IPTC and MCIDAS open behaviors in Pillow 11.3.0."""
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


def field(tag, data):
    record, num = tag
    if data is None:
        return bytes([0x1C, record, num, 0, 0])
    size = len(data)
    if size < 128:
        return bytes([0x1C, record, num, 0, 0])[:3] + struct.pack(">H", size) + data
    return bytes([0x1C, record, num, 128 + (size.bit_length() + 7) // 8, 0]) + size.to_bytes((size.bit_length() + 7) // 8, "big") + data


def make_iptc(compression, w, h, payload, layers=1, component=0):
    out_blob = b""
    out_blob += field((3, 60), bytes([layers, component]))
    out_blob += field((3, 20), struct.pack(">H", w))
    out_blob += field((3, 30), struct.pack(">H", h))
    out_blob += field((3, 120), struct.pack(">H", compression))
    out_blob += field((8, 10), payload)
    return out_blob


def open_bytes(blob, fmt):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    data = im.tobytes()
    info = {str(k): (v.hex() if isinstance(v, bytes) else str(v)) for k, v in getattr(im, "info", {}).items()}
    return (im.mode, list(im.size), data.hex(), info)


# --- IPTC: raw L payload ---
r = capture(lambda: open_bytes(make_iptc(1, 3, 2, bytes(range(6))), "IPTC"))
out["iptc_raw"] = r
# RGB: layers=3 component=1 -> mode RGB[0]? component=1 -> id from (3,65) or 0 -> "R"?? probe component 0
r = capture(lambda: open_bytes(make_iptc(1, 3, 2, bytes(range(6)), 3, 1), "IPTC"))
out["iptc_rgb_layers3"] = r
r = capture(lambda: open_bytes(make_iptc(1, 3, 2, bytes(range(6)), 3, 0), "IPTC"))
out["iptc_layers3_nocomponent"] = r
r = capture(lambda: open_bytes(make_iptc(1, 3, 2, bytes(range(6)), 4, 1), "IPTC"))
out["iptc_layers4"] = r
# (3,65) id selection
r = capture(lambda: open_bytes(field((3, 60), bytes([3, 1])) + field((3, 65), struct.pack(">H", 3)) + field((3, 20), struct.pack(">H", 3)) + field((3, 30), struct.pack(">H", 2)) + field((3, 120), struct.pack(">H", 1)) + field((8, 10), bytes(range(6))), "IPTC"))
out["iptc_id3"] = r
# jpeg compression: payload = a real JPEG
buf = io.BytesIO()
im = Image.new("L", (2, 2))
im.putdata([0, 60, 120, 200])
im.save(buf, format="JPEG")
r = capture(lambda: open_bytes(make_iptc(5, 2, 2, buf.getvalue()), "IPTC"))
out["iptc_jpeg"] = r
# unknown compression
r = capture(lambda: open_bytes(make_iptc(7, 3, 2, bytes(range(6))), "IPTC"))
out["iptc_bad_compression"] = r
# invalid record number
r = capture(lambda: Image.open(io.BytesIO(field((11, 1), b"\x00") + field((8, 10), b"\x00")), formats=["IPTC"]))
out["iptc_bad_record"] = r
# illegal field length (> 132)
r = capture(lambda: Image.open(io.BytesIO(bytes([0x1C, 3, 60, 133, 0]) + b"\x00" * 8), formats=["IPTC"]))
out["iptc_illegal_length"] = r
# extended size field (129..132)
r = capture(lambda: open_bytes(field((3, 60), b"\x01\x00" + b"\x00" * 130) + field((3, 20), struct.pack(">H", 2)) + field((3, 30), struct.pack(">H", 2)) + field((3, 120), struct.pack(">H", 1)) + field((8, 10), bytes(range(4))), "IPTC"))
out["iptc_extended_size"] = r
# no (8,10) terminator
r = capture(lambda: open_bytes(field((3, 60), b"\x01\x00") + field((3, 20), struct.pack(">H", 2)) + field((3, 30), struct.pack(">H", 2)) + field((3, 120), struct.pack(">H", 1)), "IPTC"))
out["iptc_no_data"] = r
# truncated payload (raw: PPM read short)
r = capture(lambda: open_bytes(make_iptc(1, 3, 2, b"\x01\x02"), "IPTC"))
out["iptc_truncated_payload"] = r

# --- MCIDAS: 256-byte big-endian directory ---
def make_mcidas(bpp, w, h, payload, prefix_len=0, aux=1):
    header = bytearray(256)
    header[7] = 4
    struct.pack_into("!i", header, 4 * 8, h)      # w[9] = lines
    struct.pack_into("!i", header, 4 * 9, w)      # w[10] = samples/line
    struct.pack_into("!i", header, 4 * 10, bpp)   # w[11] = bytes/sample
    struct.pack_into("!i", header, 4 * 13, aux)   # w[14]
    struct.pack_into("!i", header, 4 * 14, prefix_len)  # w[15]
    struct.pack_into("!i", header, 4 * 33, 256)   # w[34] = data offset prefix
    return bytes(header) + bytes(payload)


r = capture(lambda: open_bytes(make_mcidas(1, 3, 2, bytes(range(6))), "MCIDAS"))
out["mcidas_l"] = r
r = capture(lambda: open_bytes(make_mcidas(2, 2, 2, struct.pack(">4H", 1, 2, 3, 4)), "MCIDAS"))
out["mcidas_i16b"] = r
r = capture(lambda: open_bytes(make_mcidas(4, 2, 2, struct.pack(">4I", 1, 2, 3, 4)), "MCIDAS"))
out["mcidas_i32b"] = r
# stride with a prefix: w[15] = 2 -> stride = 2 + w*1, rows at offset 256+2
r = capture(lambda: open_bytes(make_mcidas(1, 3, 2, b"\x00\x00" + bytes([1, 2, 3]) + b"\x00\x00" + bytes([4, 5, 6]), 2), "MCIDAS"))
out["mcidas_prefix"] = r
r = capture(lambda: Image.open(io.BytesIO(make_mcidas(3, 2, 2, b"\x00" * 8)), formats=["MCIDAS"]))
out["mcidas_bad_bpp"] = r
r = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["MCIDAS"]))
out["mcidas_bad_magic"] = r
r = capture(lambda: open_bytes(make_mcidas(1, 3, 2, b"\x01\x02"), "MCIDAS"))
out["mcidas_truncated"] = r

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_3.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
