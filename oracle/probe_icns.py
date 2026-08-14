"""BEHAV-ICNS-001 oracle: Pillow 11.3.0 ICNS save/open fixtures and error shapes."""
import io
import json
import struct

from PIL import Image


def hexify(data):
    return data.hex()


def build_icns(entries):
    """entries: list of (type_bytes, payload_bytes)."""
    out = bytearray()
    out += b"icns"
    file_length = 8 + (8 + 8 * len(entries)) + sum(8 + len(p) for _, p in entries)
    out += struct.pack(">i", file_length)
    out += b"TOC "
    out += struct.pack(">i", 8 + len(entries) * 8)
    for typ, payload in entries:
        out += typ + struct.pack(">i", 8 + len(payload))
    for typ, payload in entries:
        out += typ + struct.pack(">i", 8 + len(payload)) + payload
    return bytes(out)


def save_bytes(im, **kwargs):
    buf = io.BytesIO()
    im.save(buf, format="ICNS", **kwargs)
    return buf.getvalue()


def png_bytes(im):
    buf = io.BytesIO()
    im.save(buf, format="PNG")
    return buf.getvalue()


def make_im(mode, w, h):
    im = Image.new(mode, (w, h))
    if mode == "L":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), (x * 13 + y * 37) % 256)
    elif mode == "LA":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), ((x * 13 + y * 37) % 256, (x * 29 + y * 7) % 256))
    elif mode == "RGB":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), ((x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256))
    elif mode == "RGBA":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), ((x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256, (x * 29 + y * 7) % 256))
    elif mode == "P":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), (x + y * 3) % 256)
    elif mode == "1":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), (x + y) % 2)
    elif mode == "I;16":
        for y in range(h):
            for x in range(w):
                im.putpixel((x, y), (x * 1000 + y * 3000) % 65536)
    return im


def parse_entries(data):
    assert data[:4] == b"icns", data[:8]
    file_length = struct.unpack(">i", data[4:8])[0]
    entries = []
    i = 8
    while i < file_length:
        typ, blocksize = struct.unpack(">4sI", data[i:i+8])
        i += 8
        payload = data[i:i+blocksize-8]
        i += blocksize - 8
        entries.append((typ, payload))
    return entries


out = {}

# 1. Full save fixtures
for mode in ["L", "LA", "RGB", "RGBA", "P", "1", "I;16"]:
    im = make_im(mode, 20, 10)
    data = save_bytes(im)
    entries = parse_entries(data)
    out[f"save_{mode}"] = {
        "full": hexify(data),
        "file_length": struct.unpack(">i", data[4:8])[0],
        "entries": [(t.decode("latin1"), len(p), hexify(p[:24])) for t, p in entries],
    }
    # reopen
    reopened = Image.open(io.BytesIO(data))
    out[f"save_{mode}_reopen"] = {
        "preload_mode": reopened.mode,
        "preload_size": list(reopened.size),
        "sizes": reopened.info.get("sizes"),
    }
    quirk = None
    try:
        reopened.tobytes()
        quirk = "NO ERROR"
    except Exception as e:
        quirk = f"{type(e).__name__}: {e}"
    out[f"save_{mode}_reopen"]["first_tobytes"] = quirk
    reopened2 = Image.open(io.BytesIO(data))
    reopened2.load()
    out[f"save_{mode}_reopen"]["loaded_mode"] = reopened2.mode
    out[f"save_{mode}_reopen"]["loaded_size"] = list(reopened2.size)
    out[f"save_{mode}_reopen"]["tobytes_head"] = hexify(reopened2.tobytes()[:64])

# 2. append_images save
base = make_im("RGB", 20, 10)
small = make_im("RGB", 16, 16)
medium = make_im("RGB", 32, 32)
data = save_bytes(base, append_images=[small, medium])
entries = parse_entries(data)
out["save_append"] = {
    "full": hexify(data),
    "entries": [(t.decode("latin1"), len(p)) for t, p in entries],
}

# 3. error shapes: bad magic
bad_magic = b"noti" + struct.pack(">i", 8)
out["err_bad_magic"] = None
try:
    Image.open(io.BytesIO(bad_magic))
    out["err_bad_magic"] = "NO ERROR"
except Exception as e:
    out["err_bad_magic"] = f"{type(e).__name__}: {e}"

# 4. empty (no chunks) -> No 32bit icon resources found (raised at open)
empty = b"icns" + struct.pack(">i", 8)
out["err_no_resources"] = None
try:
    Image.open(io.BytesIO(empty))
    out["err_no_resources"] = "NO ERROR"
except Exception as e:
    out["err_no_resources"] = f"{type(e).__name__}: {e}"

# 5. invalid block header (blocksize <= 0)
bad_block = b"icns" + struct.pack(">i", 16) + b"ic07" + struct.pack(">I", 0)
out["err_invalid_block"] = None
try:
    Image.open(io.BytesIO(bad_block))
    out["err_invalid_block"] = "NO ERROR"
except Exception as e:
    out["err_invalid_block"] = f"{type(e).__name__}: {e}"

# 6. truncated chunk header mid-file
trunc_hdr = b"icns" + struct.pack(">i", 16) + b"ic07"
out["err_truncated_header"] = None
try:
    Image.open(io.BytesIO(trunc_hdr))
    out["err_truncated_header"] = "NO ERROR"
except Exception as e:
    out["err_truncated_header"] = f"{type(e).__name__}: {e}"

# 7. unsupported subimage format (garbage payload in ic07) - raised at load
garbage = build_icns([(b"ic07", b"GARBAGE_PAYLOAD_NOT_A_PNG" * 4)])
im = Image.open(io.BytesIO(garbage))
out["err_unsupported_subimage"] = None
try:
    im.load()
    out["err_unsupported_subimage"] = "NO ERROR"
except Exception as e:
    out["err_unsupported_subimage"] = f"{type(e).__name__}: {e}"

# 8. truncated PNG payload in ic07 - raised at load
png_data = png_bytes(make_im("RGBA", 16, 16))
trunc_png = build_icns([(b"ic07", png_data[:len(png_data)//2])])
im = Image.open(io.BytesIO(trunc_png))
out["err_truncated_png"] = None
try:
    im.load()
    out["err_truncated_png"] = "NO ERROR"
except Exception as e:
    out["err_truncated_png"] = f"{type(e).__name__}: {e}"

# 9. jpeg2000 payload (local build has jp2) in icp4 - decodes fine
jp2_buf = io.BytesIO()
make_im("RGB", 16, 16).save(jp2_buf, format="JPEG2000")
jp2_data = jp2_buf.getvalue()
jp2_file = build_icns([(b"icp4", jp2_data)])
im = Image.open(io.BytesIO(jp2_file))
im.load()
out["jp2_open"] = {"mode": im.mode, "size": list(im.size), "sig_head": hexify(jp2_data[:12])}

# 10. best-size selection
png128 = png_bytes(make_im("RGBA", 128, 128))
png256 = png_bytes(make_im("RGB", 256, 256))
sel1 = build_icns([(b"ic07", png128), (b"ic08", png256)])
im1 = Image.open(io.BytesIO(sel1))
out["best_ic07_ic08"] = {"mode": im1.mode, "size": list(im1.size), "sizes": im1.info.get("sizes")}

png512 = png_bytes(make_im("RGB", 512, 512))
png1024 = png_bytes(make_im("RGB", 1024, 1024))
sel2 = build_icns([(b"ic09", png512), (b"ic10", png1024)])
im2 = Image.open(io.BytesIO(sel2))
out["best_ic09_ic10"] = {"mode": im2.mode, "size": list(im2.size), "sizes": im2.info.get("sizes")}

# icp4 carrying PNG data (sig check wins over chunk type)
png16 = png_bytes(make_im("RGBA", 16, 16))
icp4_png = build_icns([(b"icp4", png16)])
im3 = Image.open(io.BytesIO(icp4_png))
out["icp4_png"] = {"mode": im3.mode, "size": list(im3.size), "sizes": im3.info.get("sizes")}

# 11. legacy 32-bit RGB chunks + masks (bottom-up data stored as-is)
def legacy_rgb(w, h):
    rows = []
    for y in range(h - 1, -1, -1):  # bottom-up
        for x in range(w):
            rows.append(bytes([(x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256]))
    return b"".join(rows)

def legacy_mask(w, h):
    rows = []
    for y in range(h - 1, -1, -1):
        for x in range(w):
            rows.append(bytes([(x * 29 + y * 7) % 256]))
    return b"".join(rows)

w, h = 16, 16
rgb16 = legacy_rgb(w, h)
mask16 = legacy_mask(w, h)
legacy1 = build_icns([(b"is32", rgb16), (b"s8mk", mask16)])
im4 = Image.open(io.BytesIO(legacy1))
out["legacy_is32_s8mk"] = {"mode": im4.mode, "size": list(im4.size), "sizes": im4.info.get("sizes"), "tobytes_head": hexify(im4.tobytes()[:64])}

legacy2 = build_icns([(b"is32", rgb16)])
im5 = Image.open(io.BytesIO(legacy2))
out["legacy_is32_only"] = {"mode": im5.mode, "size": list(im5.size), "tobytes_head": hexify(im5.tobytes()[:64])}

# it32/t8mk are 128x128; il32/l8mk 32x32; ih32/h8mk 48x48
for code, rcode, mcode, sz, extra in [(b"it32", "it32", b"t8mk", 128, b"\x00\x00\x00\x00"), (b"il32", "il32", b"l8mk", 32, b""), (b"ih32", "ih32", b"h8mk", 48, b"")]:
    rgbx = legacy_rgb(sz, sz)
    maskx = legacy_mask(sz, sz)
    filex = build_icns([(code, extra + rgbx), (mcode, maskx)])
    imx = Image.open(io.BytesIO(filex))
    out[f"legacy_{code.decode()}_{mcode.decode()}"] = {
        "mode": imx.mode, "size": list(imx.size), "sizes": imx.info.get("sizes"), "tobytes_head": hexify(imx.tobytes()[:64]),
    }

# it32 with the extra 0x00000000 header
rgb128 = legacy_rgb(128, 128)
it32_file = build_icns([(b"it32", b"\x00\x00\x00\x00" + rgb128)])
im6 = Image.open(io.BytesIO(it32_file))
out["legacy_it32"] = {"mode": im6.mode, "size": list(im6.size), "tobytes_head": hexify(im6.tobytes()[:64])}

# it32 bad header
it32_bad = build_icns([(b"it32", b"\x01\x02\x03\x04" + rgb128)])
im7 = Image.open(io.BytesIO(it32_bad))
out["err_it32_signature"] = None
try:
    im7.load()
    out["err_it32_signature"] = "NO ERROR"
except Exception as e:
    out["err_it32_signature"] = f"{type(e).__name__}: {e}"

# 12. RLE 32-bit chunk
def legacy_rle(w, h, truncated_band=None):
    rows = []
    for y in range(h - 1, -1, -1):
        for x in range(w):
            rows.append(((x * 13 + y) % 256, (x * 7 + y * 31) % 256, (x * 3 + y * 53) % 256))
    bands = [[pix[b] for pix in rows] for b in range(3)]
    outb = bytearray()
    for band_ix, band in enumerate(bands):
        if truncated_band is not None and band_ix >= truncated_band:
            break
        i = 0
        n = len(band)
        while i < n:
            # find run of identical values
            run = 1
            while i + run < n and band[i + run] == band[i] and run < 3:
                run += 1
            if run >= 2:
                # compressed block: blocksize up to 3, byte = 0x80 | (blocksize - 125)
                outb.append(0x80 | (run - 125))
                outb.append(band[i])
                i += run
            else:
                # literal block
                lit = 1
                while i + lit < n and lit < 128:
                    nxt_run = 1
                    while i + lit + nxt_run < n and band[i + lit + nxt_run] == band[i + lit] and nxt_run < 3:
                        nxt_run += 1
                    if nxt_run >= 2:
                        break
                    lit += 1
                outb.append(lit - 1)
                outb.extend(band[i:i + lit])
                i += lit
    return bytes(outb)

rle_payload = legacy_rle(w, h)
rle_file = build_icns([(b"is32", rle_payload)])
im8 = Image.open(io.BytesIO(rle_file))
im8.load()
out["legacy_rle"] = {"mode": im8.mode, "size": list(im8.size), "tobytes_head": hexify(im8.tobytes()[:64])}

# truncated RLE: only band 0 + part of band 1
rle_trunc = legacy_rle(w, h, truncated_band=1)
rle_trunc_file = build_icns([(b"is32", rle_trunc)])
im9 = Image.open(io.BytesIO(rle_trunc_file))
out["err_rle_leftover"] = None
try:
    im9.load()
    out["err_rle_leftover"] = "NO ERROR"
except Exception as e:
    out["err_rle_leftover"] = f"{type(e).__name__}: {e}"

# 13. size setter error
out["err_size_setter"] = None
try:
    im10 = Image.open(io.BytesIO(sel1))
    im10.size = (999, 999)
    out["err_size_setter"] = "NO ERROR"
except Exception as e:
    out["err_size_setter"] = f"{type(e).__name__}: {e}"

with open(r"oracle/probe_icns.json", "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1)
print("ICNS oracle probe complete")
for key in sorted(out):
    print(key, "=", out[key])
