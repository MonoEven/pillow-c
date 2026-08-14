"""BEHAV-OPEN-002 oracle: FTEX / SUN / GBR / FITS / XPM open behaviors."""
import io
import json
import math
import struct

from PIL import Image


def capture(fn):
    try:
        return {"ok": fn()}
    except Exception as e:
        return {"err": f"{type(e).__name__}: {e}"}


out = {}


def open_bytes(blob, fmt):
    im = Image.open(io.BytesIO(blob), formats=[fmt])
    data = im.tobytes()
    pal = None
    if im.mode == "P":
        pal = list(im.getpalette())
    info = {k: (str(v) if not isinstance(v, bytes) else v.hex()) for k, v in getattr(im, "info", {}).items()}
    return (im.mode, list(im.size), data.hex(), pal, info)


# --- FTEX ---
def make_ftex(w, h, payload, fmt_id):
    header = struct.pack("<6i", 0x58455446, 1, w, h, 1, 1)  # "FTEX" LE
    directory = struct.pack("<2i", fmt_id, 32)
    return header + directory + struct.pack("<i", len(payload)) + payload


rgb = bytes([(x * 7 + y) % 256 for y in range(2) for x in range(3) for _ in range(3)])
out["ftex_rgb"] = capture(lambda: open_bytes(make_ftex(3, 2, rgb, 1), "FTEX"))
out["ftex_rgb_truncated"] = capture(lambda: open_bytes(make_ftex(3, 2, rgb[:10], 1), "FTEX"))
out["ftex_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["FTEX"]))
out["ftex_bad_format"] = capture(lambda: open_bytes(make_ftex(3, 2, rgb, 7), "FTEX"))
out["ftex_two_formats"] = capture(lambda: Image.open(io.BytesIO(struct.pack("<6i", 0x58455446, 1, 3, 2, 1, 2) + b"\x00" * 64), formats=["FTEX"]))
# DXT1 4x4 solid block: color0=0x7C00 (R5G6B5 red), color1=0x0000, indices 0
dxt1 = struct.pack("<HHI", 0x7C00, 0x0000, 0x00000000)
out["ftex_dxt1"] = capture(lambda: open_bytes(make_ftex(4, 4, dxt1, 0), "FTEX"))

# --- SUN ---
def make_sun(w, h, depth, data, file_type=1, palette_type=0, palette=b""):
    length = len(data)
    header = struct.pack(">8I", 0x59A66A95, w, h, depth, length, file_type, palette_type, len(palette))
    return header + palette + data


sun1 = make_sun(3, 2, 1, b"\x80\x40\xc0\x00", 1)  # stride 2: rows 100/010 then 110+pad -> 0b10000000,0b01000000,0b11000000,pad
out["sun_depth1"] = capture(lambda: open_bytes(sun1, "SUN"))
sun8 = make_sun(3, 2, 8, bytes(range(6)), 1)
out["sun_depth8"] = capture(lambda: open_bytes(sun8, "SUN"))
sun24_rgb = make_sun(3, 2, 24, bytes([i % 256 for i in range(18)]), 3)
out["sun_depth24_type3"] = capture(lambda: open_bytes(sun24_rgb, "SUN"))
sun24_bgr = make_sun(3, 2, 24, bytes([i % 256 for i in range(18)]), 1)
out["sun_depth24_type1"] = capture(lambda: open_bytes(sun24_bgr, "SUN"))
sun32 = make_sun(2, 1, 32, bytes([1, 2, 3, 9, 4, 5, 6, 9]), 1)
out["sun_depth32_type1"] = capture(lambda: open_bytes(sun32, "SUN"))
sun32t3 = make_sun(2, 1, 32, bytes([1, 2, 3, 9, 4, 5, 6, 9]), 3)
out["sun_depth32_type3"] = capture(lambda: open_bytes(sun32t3, "SUN"))
# palette 8-bit
pal = bytes([255, 0, 0, 0, 255, 0])
sunp = make_sun(3, 2, 8, bytes([0, 1, 0, 1, 0, 1]), 1, 1, pal)
out["sun_palette"] = capture(lambda: open_bytes(sunp, "SUN"))
# SUN RLE 0x80 semantics: literal passthrough vs run escape
rle2 = bytes([0x80, 0x02, 7, 8, 9, 9, 9, 9, 9, 9, 9, 9])
out["sun_rle_semantics"] = capture(lambda: open_bytes(make_sun(3, 2, 8, rle2, 2), "SUN"))
rle = bytes([0x82, 7, 7, 8, 0x83, 9, 9, 9, 3])  # needs stride((3*8+15)/16)*2=4 per row -> 12 bytes total
rle += bytes([0]) * (12 - len(rle))
sunrle = make_sun(3, 2, 8, rle, 2)
out["sun_rle"] = capture(lambda: open_bytes(sunrle, "SUN"))
out["sun_rle_short"] = capture(lambda: open_bytes(make_sun(3, 2, 8, b"\x80\x80\x80", 2), "SUN"))
out["sun_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["SUN"]))
out["sun_bad_depth"] = capture(lambda: Image.open(io.BytesIO(make_sun(3, 2, 6, b"\x00" * 4)), formats=["SUN"]))
out["sun_bad_pal_len"] = capture(lambda: Image.open(io.BytesIO(struct.pack(">8I", 0x59A66A95, 3, 2, 8, 0, 1, 1, 2048)), formats=["SUN"]))
out["sun_bad_pal_type"] = capture(lambda: Image.open(io.BytesIO(struct.pack(">8I", 0x59A66A95, 3, 2, 8, 0, 1, 2, 3) + b"\x00\x00\x00"), formats=["SUN"]))
out["sun_bad_file_type"] = capture(lambda: Image.open(io.BytesIO(make_sun(3, 2, 8, b"\x00" * 6, 9)), formats=["SUN"]))
out["sun_truncated"] = capture(lambda: open_bytes(make_sun(3, 2, 8, b"\x01\x02"), "SUN"))

# --- GBR ---
def make_gbr(w, h, depth, data, version=1, comment=b""):
    header_size = 20 + len(comment) + (1 if version == 1 else 9)  # comment includes newline
    comment_pad = comment + b"\n"
    if version == 1:
        head = struct.pack(">5I", header_size, version, w, h, depth) + comment_pad
    else:
        head = struct.pack(">5I", header_size, version, w, h, depth) + b"GIMP" + struct.pack(">I", 25) + comment_pad
    return head + data


out["gbr_v1"] = capture(lambda: open_bytes(make_gbr(3, 2, 1, bytes(range(6)), 1), "GBR"))
out["gbr_v2"] = capture(lambda: open_bytes(make_gbr(3, 2, 4, bytes(range(24)), 2, b"hello"), "GBR"))
out["gbr_v2_bad_magic"] = capture(lambda: Image.open(io.BytesIO(make_gbr(3, 2, 1, bytes(range(6)), 2, b"x").replace(b"GIMP", b"NOPE", 1)), formats=["GBR"]))
out["gbr_bad_header_size"] = capture(lambda: Image.open(io.BytesIO(struct.pack(">5I", 12, 1, 3, 2, 1) + b"\x00" * 16), formats=["GBR"]))
out["gbr_bad_version"] = capture(lambda: Image.open(io.BytesIO(struct.pack(">5I", 20, 3, 3, 2, 1) + b"\x00" * 16), formats=["GBR"]))
out["gbr_bad_depth"] = capture(lambda: Image.open(io.BytesIO(struct.pack(">5I", 20, 1, 3, 2, 2) + b"\x00" * 16), formats=["GBR"]))
out["gbr_short_data"] = capture(lambda: open_bytes(make_gbr(3, 2, 1, b"\x01"), "GBR"))
out["gbr_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["GBR"]))

# --- FITS ---
def make_fits(bitpix, naxis1, naxis2, payload, extra_headers=None):
    lines = ["SIMPLE  =                    T"]
    lines.append(f"BITPIX  = {bitpix:20d}")
    lines.append(f"NAXIS   = {2 if naxis2 else 0:20d}")
    lines.append(f"NAXIS1  = {naxis1:20d}")
    lines.append(f"NAXIS2  = {naxis2:20d}")
    if extra_headers:
        lines += extra_headers
    lines.append("END")
    records = [l.ljust(80)[:80].encode() for l in lines]
    header = b"".join(records)
    header += b" " * (2880 - len(header) % 2880)
    return header + payload


fits8 = make_fits(8, 40, 2, bytes(range(80)))
out["fits8"] = capture(lambda: open_bytes(fits8, "FITS"))
fits16 = make_fits(16, 10, 2, struct.pack(">20h", *range(20)))
out["fits16"] = capture(lambda: open_bytes(fits16, "FITS"))
fits32 = make_fits(32, 10, 2, struct.pack(">20i", *range(20)))
out["fits32"] = capture(lambda: open_bytes(fits32, "FITS"))
fitsf = make_fits(-32, 10, 2, struct.pack(">20f", *[float(i) for i in range(20)]))
out["fitsf"] = capture(lambda: open_bytes(fitsf, "FITS"))
out["fits_naxis0"] = capture(lambda: open_bytes(make_fits(8, 0, 0, b""), "FITS"))
out["fits_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"\x00" * 64), formats=["FITS"]))
out["fits_bad_simple"] = capture(lambda: Image.open(io.BytesIO(b"SIMPLE  =                    F".ljust(2880) + b"\x00" * 16), formats=["FITS"]))
out["fits_truncated_header"] = capture(lambda: Image.open(io.BytesIO(b"SIMPLE  =                    T"[:40]), formats=["FITS"]))
out["fits_truncated_data"] = capture(lambda: open_bytes(make_fits(8, 3, 2, b"\x01\x02"), "FITS"))
out["fits_short_70"] = capture(lambda: open_bytes(make_fits(8, 40, 2, bytes(range(70))), "FITS"))

# --- XPM ---
def make_xpm(w, h, colors, pixels, bpp=1):
    lines = ["/* XPM */", f'"{w} {h} {len(colors)} {bpp}"']
    for key, value in colors:
        lines.append(f'"{key} c {value}",')
    lines.append("/* pixels */")
    for row in pixels:
        lines.append('"' + row + '",')
    return ("\n".join(lines) + "\n").encode()


xpm_p = make_xpm(3, 2, [("a", "#FF0000"), (".", "#00FF00")], ["a.a", ".a."])
out["xpm_p"] = capture(lambda: open_bytes(xpm_p, "XPM"))
xpm_rgb = make_xpm(3, 2, [(f"{i:03d}", f"#{(i * 3) % 256:02x}{(i * 5) % 256:02x}{(i * 7) % 256:02x}") for i in range(300)], ["000001002", "003004005"], 3)
out["xpm_rgb"] = capture(lambda: open_bytes(xpm_rgb, "XPM"))
xpm_none = make_xpm(2, 1, [("a", "None"), (".", "#112233")], ["a."])
out["xpm_none"] = capture(lambda: open_bytes(xpm_none, "XPM"))
out["xpm_bad_magic"] = capture(lambda: Image.open(io.BytesIO(b"not xpm"), formats=["XPM"]))
out["xpm_broken"] = capture(lambda: Image.open(io.BytesIO(b"/* XPM */\n"), formats=["XPM"]))
out["xpm_bad_color"] = capture(lambda: open_bytes(make_xpm(2, 1, [("a", "rgb:1/2/3")], ["aa"]), "XPM"))
out["xpm_missing_c"] = capture(lambda: open_bytes(make_xpm(2, 1, [("a", "m #000000")], ["aa"]), "XPM"))
out["xpm_no_pixels_marker"] = capture(lambda: open_bytes(make_xpm(2, 1, [("a", "#000000")], ["aa"]), "XPM"))
out["xpm_truncated_pixels"] = capture(lambda: open_bytes(make_xpm(3, 2, [("a", "#FF0000")], ["aaa"]), "XPM"))
out["xpm_unknown_key"] = capture(lambda: open_bytes(make_xpm(2, 1, [("a", "#FF0000")], ["ab"]), "XPM"))
# quoted-wrap: a row split across two quoted segments
wrapped = make_xpm(3, 2, [("a", "#FF0000"), (".", "#00FF00")], ["a.a", ".a."]).replace(b'"a.a",', b'"aa" "a",')
out["xpm_quote_wrapped"] = capture(lambda: open_bytes(wrapped, "XPM"))

print(json.dumps(out, indent=1, default=str))
with open("oracle/probe_open_mid.json", "w") as f:
    json.dump(out, f, indent=1, default=str)
